#include "aw_sndcard.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static snd_card_t *g_snd_card_info = NULL;

#define SPK_VOLUME_CONTROL "name='LINEOUT volume'"
#define MIC_VOLUME_CONTROL "name='MIC1 gain volume'"

int convert_volume(int percent, int min, int max)
{
    int range = max - min;
    if (range == 0) {
        return 0;
    }
    return ((range * percent / 100) + min);
}

int snd_card_set_volume(char *dev_name, char *elem_name, int volume_percent)
{
    int err = -1;
    snd_ctl_t *handle = NULL;
    char *card = "hw:audiocodec";
    char *volume_control = elem_name;
    char volume_string[4];
    int min, max, raw;
    char snd_name[256];
    if (elem_name == NULL) {
        return -1;
    }
    memset(snd_name, 0, sizeof(snd_name));
    if (dev_name && strlen(dev_name)) {
        sprintf(snd_name, "hw:%s", dev_name);
        card = snd_name;
    }

    snd_ctl_elem_info_t *info = NULL;
    snd_ctl_elem_id_t *id = NULL;
    snd_ctl_elem_value_t *value = NULL;

    if (volume_percent > 100) {
        volume_percent = 100;
    }
    if (volume_percent < 0) {
        volume_percent = 0;
    }

    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&value);
    if (info == NULL || id == NULL || value == NULL) {
        DOORLOCK_ERR("elem staff alloca failed\n");
        goto failed;
    }
    err = snd_ctl_ascii_elem_id_parse(id, volume_control);
    if (err < 0) {
        DOORLOCK_ERR("snd_ctl_ascii_elem_id_parse [%s] failed\n", volume_control);
        goto failed;
    }
    err = snd_ctl_open(&handle, card, 0);
    if (err < 0) {
        DOORLOCK_ERR("snd_ctl_open [%s] failed\n", card);
        goto failed;
    }
    snd_ctl_elem_info_get_id(info, id);
    snd_ctl_elem_value_set_id(value, id);
    err = snd_ctl_elem_read(handle, value);
    if (err < 0) {
        DOORLOCK_ERR("snd_ctl_elem_read [%s] failed, %s\n", card, snd_strerror(err));
        goto failed;
    }
    min = snd_ctl_elem_info_get_min(info);
    max = snd_ctl_elem_info_get_max(info);

    snprintf(volume_string, sizeof(volume_string), "%d", convert_volume(volume_percent, min, max));
    err = snd_ctl_ascii_value_parse(handle, value, info, volume_string);
    if (err < 0) {
        DOORLOCK_ERR("snd_ctl_ascii_value_parse [%s] failed\n", card);
        goto failed;
    }
    err = snd_ctl_elem_write(handle, value);
    if (err < 0) {
        DOORLOCK_ERR("snd_ctl_elem_write [%s] failed\n", card);
        goto failed;
    }
failed:
    if (info) {
        snd_ctl_elem_info_free(info);
    }
    if (id) {
        snd_ctl_elem_id_free(id);
    }
    if (value) {
        snd_ctl_elem_value_free(value);
    }
    if (handle) {
        snd_ctl_close(handle);
    }

    return err;
}
static int snd_card_open(snd_alsa_config_t *snd_cfg, char *dev_name, snd_pcm_stream_t stream,
                         uint32_t bit_width, uint32_t channel_cnt, uint32_t sample_rate)
{
    int rc;
    char snd_name[256];
    DOORLOCK_DBG("enter ===>\n");
    memset(snd_name, 0, sizeof(snd_name));
    sprintf(snd_name, "hw:%s", dev_name);
    DOORLOCK_INFO("snd card name: [%s], stream = %d\n", snd_name, stream);
    rc = snd_pcm_open(&snd_cfg->snd_handle, snd_name, stream,
                      0);  // SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE
    if (rc < 0 || snd_cfg->snd_handle == NULL) {
        snd_cfg->snd_handle = NULL;
        DOORLOCK_ERR("unable to open pcm device, stream = %d\n", stream);
        DOORLOCK_DBG("exit <===\n");
        return -1;
    } else {
        DOORLOCK_DBG("open pcm device success, stream = %d\n", stream);
    }

    /* Allocate a hardware parameters object */
    snd_pcm_hw_params_alloca(&snd_cfg->snd_hwparams);
    if (snd_cfg->snd_hwparams == NULL) {
        DOORLOCK_ERR("snd_pcm_hw_params_alloca failed, snd_name = %s, stream = %d\n", snd_name,
                     stream);
        goto err1;
    }
    /* Fill it in with default values. */
    rc = snd_pcm_hw_params_any(snd_cfg->snd_handle, snd_cfg->snd_hwparams);
    if (rc < 0) {
        DOORLOCK_ERR("unable to Fill it in with default values.\n");
        goto err1;
    }

    /* Interleaved mode */
    rc = snd_pcm_hw_params_set_access(snd_cfg->snd_handle, snd_cfg->snd_hwparams,
                                      SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) {
        DOORLOCK_ERR("unable to Interleaved mode.\n");
        goto err1;
    }
    snd_pcm_format_t format;

    switch (bit_width) {
    case 8:
        DOORLOCK_DBG("set 8bit for snd\n");
        format = SND_PCM_FORMAT_U8;
        break;
    case 16:
        DOORLOCK_DBG("set 16bit for snd\n");
        format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        DOORLOCK_DBG("set 24bit for snd\n");
        format = SND_PCM_FORMAT_U24_LE;
        break;
    case 32:
        DOORLOCK_DBG("set 32bit for snd\n");
        format = SND_PCM_FORMAT_U32_LE;
        break;
    default:
        DOORLOCK_DBG("SND_PCM_FORMAT_UNKNOWN.\n");
        format = SND_PCM_FORMAT_UNKNOWN;
        goto err1;
    }

    /* set format */
    rc = snd_pcm_hw_params_set_format(snd_cfg->snd_handle, snd_cfg->snd_hwparams, format);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set format.\n");
        goto err1;
    }
    /* set channels (stero)  snd to pc only support stero */
    snd_pcm_hw_params_set_channels(snd_cfg->snd_handle, snd_cfg->snd_hwparams, channel_cnt);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set channels (stero).\n");
        goto err1;
    }
    /* set sampling rate */
    unsigned int dir;
    unsigned int rate = sample_rate;
    rc = snd_pcm_hw_params_set_rate_near(snd_cfg->snd_handle, snd_cfg->snd_hwparams, &rate, &dir);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set sampling rate.\n");
        goto err1;
    }

    if (snd_cfg->period_size == 0) {
        // snd_cfg->period_size = 1024;
        snd_cfg->period_size = 256 * sample_rate / 8000;
    }
    if (snd_cfg->periods == 0) snd_cfg->periods = 4;

    snd_cfg->bytes_per_frame = channel_cnt * bit_width / 8;
    DOORLOCK_INFO("snd set uac card %d, period_size %d, periods %d\n", stream, snd_cfg->period_size,
                  snd_cfg->periods);

    snd_pcm_hw_params_set_period_size_near(snd_cfg->snd_handle, snd_cfg->snd_hwparams,
                                           &snd_cfg->period_size, &dir);
    snd_pcm_hw_params_set_periods(snd_cfg->snd_handle, snd_cfg->snd_hwparams, snd_cfg->periods, 0);

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = period_size(in bytes) * periods / (rate * bytes_per_frame)     */
    /* latency = period_size(in frames) * periods / rate     */
    snd_cfg->buffer_size = (snd_cfg->period_size * snd_cfg->periods);
    DOORLOCK_INFO("snd set uac card %d, buffer_size %d\n", stream, snd_cfg->buffer_size);
    snd_pcm_hw_params_set_buffer_size(snd_cfg->snd_handle, snd_cfg->snd_hwparams,
                                      snd_cfg->buffer_size);

    usleep(200 * 1000);  // why ???
    /* Write the parameters to the dirver */
    rc = snd_pcm_hw_params(snd_cfg->snd_handle, snd_cfg->snd_hwparams);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set hw parameters: %s\n", snd_strerror(rc));
        goto err1;
    }

    snd_pcm_hw_params_get_period_size(snd_cfg->snd_hwparams, &snd_cfg->period_size, &dir);
    DOORLOCK_INFO("snd get snd card period_size :%d \n", snd_cfg->period_size);

    snd_pcm_hw_params_get_periods(snd_cfg->snd_hwparams, &snd_cfg->periods, &dir);
    DOORLOCK_INFO("snd get snd card periods :%d \n", snd_cfg->periods);

    snd_pcm_hw_params_get_buffer_size(snd_cfg->snd_hwparams,
                                      (snd_pcm_uframes_t *)&snd_cfg->buffer_size);
    DOORLOCK_INFO("snd get snd card buffer size = %d frames\n", snd_cfg->buffer_size);

#if 1
    snd_pcm_sw_params_alloca(&snd_cfg->snd_swparams);
    if (snd_cfg->snd_swparams == NULL) {
        DOORLOCK_ERR("snd_pcm_sw_params_alloca failed\n");
        goto err1;
    }
    snd_pcm_sw_params_current(snd_cfg->snd_handle, snd_cfg->snd_swparams);
    snd_pcm_sw_params_set_avail_min(snd_cfg->snd_handle, snd_cfg->snd_swparams,
                                    snd_cfg->period_size);
    // snd_pcm_sw_params_set_avail_min(snd_cfg->snd_handle, snd_cfg->snd_swparams,
    // snd_cfg->buffer_size/2);
    int start_threshold = 0;
    int stop_threshold = snd_cfg->buffer_size;
    if (stream == SND_PCM_STREAM_PLAYBACK) {
        start_threshold = snd_cfg->buffer_size / 2;  // snd_cfg->buffer_size;
        stop_threshold = snd_cfg->buffer_size;       // snd_cfg->buffer_size/2;
    } else {
        start_threshold = snd_cfg->period_size;  // snd_cfg->buffer_size;
        stop_threshold = snd_cfg->buffer_size;   // snd_cfg->buffer_size/2;
    }

    snd_pcm_sw_params_set_start_threshold(snd_cfg->snd_handle, snd_cfg->snd_swparams,
                                          start_threshold);
    snd_pcm_sw_params_set_stop_threshold(snd_cfg->snd_handle, snd_cfg->snd_swparams,
                                         stop_threshold);
    snd_pcm_sw_params(snd_cfg->snd_handle, snd_cfg->snd_swparams);
    if (snd_cfg->snd_swparams) {
        snd_pcm_sw_params_free(snd_cfg->snd_swparams);
        snd_cfg->snd_swparams = NULL;
    }
#endif
    // snd_pcm_nonblock(snd_cfg->snd_handle, 1);

    snd_pcm_prepare(snd_cfg->snd_handle);
    snd_pcm_start(snd_cfg->snd_handle);
    // snd_pcm_pause(snd_cfg->snd_handle, 0);

    DOORLOCK_DBG("exit <===\n");
    return 0;
err1:
    if (snd_cfg->snd_hwparams) {
        snd_pcm_hw_params_free(snd_cfg->snd_hwparams);
        snd_cfg->snd_hwparams = NULL;
    }
    if (snd_cfg->snd_handle) {
        snd_pcm_close(snd_cfg->snd_handle);
        snd_cfg->snd_handle = NULL;
    }
    DOORLOCK_DBG("exit <===\n");
    return -1;
}

static int snd_card_close(snd_alsa_config_t *snd_cfg)
{
    DOORLOCK_DBG("enter ===>\n");
    if (snd_cfg->snd_handle != NULL) {
        snd_pcm_drain(snd_cfg->snd_handle);
        snd_pcm_close(snd_cfg->snd_handle);
        snd_cfg->snd_handle = NULL;
    } else {
        DOORLOCK_ERR("SND close failed, snd_handle == NULL\n");
        return -1;
    }
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int snd_card_open_mic(snd_card_t *snd_card_info)
{
    int rc = -1;
    snd_alsa_config_t *snd_cfg = NULL;
    DOORLOCK_DBG("enter ===>\n");
    DOORLOCK_DBG("snd audio card : %s \n", snd_card_info->snd_devname);
    if (snd_card_info->mic) {
        snd_alsa_config_t *snd_cfg = &snd_card_info->mic_alsa_cfg;
        rc = snd_card_open(snd_cfg, snd_card_info->snd_devname, SND_PCM_STREAM_CAPTURE,
                           snd_card_info->mic_bitwidth, snd_card_info->mic_channel_cnt,
                           snd_card_info->mic_sample_rate);
        if (rc == 0)
            snd_card_info->mic_is_streaming = 1;
        else
            snd_card_info->mic_is_streaming = 0;
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

int snd_card_close_mic(snd_card_t *snd_card_info)
{
    int rc = 0;
    snd_alsa_config_t *snd_cfg = NULL;
    DOORLOCK_DBG("enter ===>\n");
    if (snd_card_info->mic) {
        snd_alsa_config_t *snd_cfg = &snd_card_info->mic_alsa_cfg;
        rc = snd_card_close(snd_cfg);
        snd_card_info->mic_is_streaming = 0;
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

int snd_card_open_spk(snd_card_t *snd_card_info)
{
    int rc = -1;
    snd_alsa_config_t *snd_cfg = NULL;
    DOORLOCK_DBG("enter ===>\n");
    DOORLOCK_DBG("snd audio card : %s \n", snd_card_info->snd_devname);
    if (snd_card_info->spk) {
        snd_alsa_config_t *snd_cfg = &snd_card_info->spk_alsa_cfg;
        rc = snd_card_open(snd_cfg, snd_card_info->snd_devname, SND_PCM_STREAM_PLAYBACK,
                           snd_card_info->spk_bitwidth, snd_card_info->spk_channel_cnt,
                           snd_card_info->spk_sample_rate);
        if (rc == 0)
            snd_card_info->spk_is_streaming = 1;
        else
            snd_card_info->spk_is_streaming = 0;
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

int snd_card_close_spk(snd_card_t *snd_card_info)
{
    int rc = 0;
    snd_alsa_config_t *snd_cfg = NULL;
    DOORLOCK_DBG("enter ===>\n");
    if (snd_card_info->spk) {
        snd_alsa_config_t *snd_cfg = &snd_card_info->spk_alsa_cfg;
        rc = snd_card_close(snd_cfg);
        snd_card_info->spk_is_streaming = 0;
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

int snd_card_find(char *dev_name)
{
    int total_cards = 0;  // No cards found yet
    int card_num = -1;    // Start with first card
    int find = -1;
    int err;
    snd_ctl_t *handle = NULL;
    snd_ctl_card_info_t *info = NULL;
    snd_pcm_info_t *pcm_info = NULL;
    int card = -1;
    char hwdev[256] = {0};
    if (dev_name == NULL || strlen(dev_name) == 0) {
        DOORLOCK_ERR("dev_name must be valid\n");
        return -1;
    }
    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcm_info);
    if (info == NULL || pcm_info == NULL) {
        DOORLOCK_ERR("snd_ctl_card_info_alloca or snd_pcm_info_alloca NULL\n");
        return -1;
    }
    while (1) {
        // Get next sound card's card number.
        if ((err = snd_card_next(&card_num)) < 0) {
            DOORLOCK_ERR("Can't get the next card number: %s\n", snd_strerror(err));
            break;
        }
        if (card_num < 0) {
            // No more cards
            DOORLOCK_INFO("No more audio cards\n");
            break;
        }
        ++total_cards;
        DOORLOCK_INFO("ALSA found %i card(s)\n", total_cards);

        memset(hwdev, 0, sizeof(hwdev));
        sprintf(hwdev, "hw:%d", card_num);
        handle = NULL;
        err = snd_ctl_open(&handle, hwdev, 0);
        if (err < 0) {
            DOORLOCK_ERR("Can't open card: %s, card_num: %d\n", snd_strerror(err), card_num);
            continue;
        }

        err = snd_ctl_card_info(handle, info);
        if (err < 0) {
            snd_ctl_close(handle);
            DOORLOCK_ERR("Can't get card info: %s, card_num: %d\n", snd_strerror(err), card_num);
            continue;
        }
        const char *card_id = snd_ctl_card_info_get_id(info);
        const char *card_name = snd_ctl_card_info_get_name(info);
        if (card_id == NULL || card_name == NULL) {
            snd_ctl_close(handle);
            DOORLOCK_ERR("snd_ctl_card_info_get_id or snd_ctl_card_info_get_name NULL\n");
            continue;
        }
        DOORLOCK_DBG("Card %d, ID [%s], name [%s]\n", card_num, card_id, card_name);

        if (memcmp(card_id, dev_name, strlen(dev_name)) == 0) {
            DOORLOCK_INFO("Got it\n");
            find = 0;
        } else {
            snd_ctl_close(handle);
            continue;
        }

#if 0
		// ALSA allocates some memory to load its config file when we call
		// snd_card_next. Now that we're done getting the info,tell ALSA
		// to unload the info and release the memory.
		int devNum = -1;
		while(1){

			if( snd_ctl_pcm_next_device(handle, &devNum) < 0 ) {
				break;
			}
			if(devNum < 0){
				break;
			}
			snd_pcm_info_set_device(pcm_info, devNum);
			snd_pcm_info_set_subdevice(pcm_info, 0);
			for(int stream = 0; stream < 2; stream++){
				snd_pcm_info_set_stream(pcm_info, stream);
				err = snd_ctl_pcm_info(handle, pcm_info);
				if(err < 0) {
					if(stream == SND_PCM_STREAM_PLAYBACK){
						DOORLOCK_ERR("  Can't get snd_ctl_pcm_info: %s, card_num: %d, devNum: %d, stream: %d, SND_PCM_STREAM_PLAYBACK\n", snd_strerror(err), card_num, devNum, stream);
					}else{
						DOORLOCK_ERR("  Can't get snd_ctl_pcm_info: %s, card_num: %d, devNum: %d, stream: %d, SND_PCM_STREAM_CAPTURE\n", snd_strerror(err), card_num, devNum, stream);
					}
					continue;
				}
				int nsubd = snd_pcm_info_get_subdevices_count(pcm_info);
				char *pcmID = snd_pcm_info_get_id(pcm_info);
				char *pcmName = snd_pcm_info_get_name(pcm_info);
				if(pcmID == NULL || pcmName == NULL){
					DOORLOCK_ERR("snd_pcm_info_get_id or snd_pcm_info_get_name NULL\n");
				}
				if(stream == SND_PCM_STREAM_PLAYBACK){
					DOORLOCK_DBG("  Device %d, ID [%s], name [%s], %d subdevices (%d available), stream %d, SND_PCM_STREAM_PLAYBACK\n", \
						devNum, pcmID, pcmName, \
						nsubd, snd_pcm_info_get_subdevices_avail(pcm_info), stream);
				}else{
					DOORLOCK_DBG("  Device %d, ID [%s], name [%s], %d subdevices (%d available), stream %d, SND_PCM_STREAM_CAPTURE\n", \
						devNum, pcmID, pcmName, \
						nsubd, snd_pcm_info_get_subdevices_avail(pcm_info), stream);
				}
				//memset(hwdev, 0, sizeof(hwdev));
				//sprintf(hwdev, "hw:%d,%d", card_num, devNum);
				//err = snd_pcm_open(&pcm, hwdev, stream, SND_PCM_NONBLOCK);
			}
		}
#endif
        if (handle) {
            snd_ctl_close(handle);
        }
        if (find == 0) {
            break;
        }
    }
    if (info) {
        snd_ctl_card_info_free(info);
    }
    if (pcm_info) {
        snd_pcm_info_free(pcm_info);
    }
    // snd_config_update_free_global();
    return find;
}

/* I/O error handler */
static void snd_xrun(snd_pcm_t *handle)
{
#if 1
    snd_pcm_status_t *status;
    int res;

    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(handle, status)) < 0) {
        DOORLOCK_ERR("status error: %s\n", snd_strerror(res));
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        DOORLOCK_ERR("xrun: SND_PCM_STATE_XRUN\n");
        if ((res = snd_pcm_prepare(handle)) < 0) {
            DOORLOCK_ERR("xrun: prepare error: %s\n", snd_strerror(res));
        }
        /* ok, data should be accepted again */
        goto snd_xrun_exit;
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
        DOORLOCK_ERR("SND_PCM_STATE_DRAINING!!!\n");
    }
    DOORLOCK_ERR("read/write error, state = %s",
                 snd_pcm_state_name(snd_pcm_status_get_state(status)));
snd_xrun_exit:
    snd_pcm_status_free(status);
#else
    snd_pcm_prepare(handle);
#endif
    return;
}

/* I/O suspend handler */
static void snd_suspend(snd_pcm_t *handle)
{
    int res;
    DOORLOCK_ERR("Suspended. Trying resume.\n");
    while ((res = snd_pcm_resume(handle)) == -EAGAIN)
        usleep(100 * 1000); /* wait until suspend flag is released */
    if (res < 0) {
        DOORLOCK_ERR("Failed. Restarting stream.\n");
        if ((res = snd_pcm_prepare(handle)) < 0) {
            DOORLOCK_ERR("suspend: prepare error: %s\n", snd_strerror(res));
        }
    }
    DOORLOCK_ERR("Done.\n");
}

int snd_card_read_mic_frame(snd_card_t *snd_card_info, uint8_t *data_buf, uint32_t data_size)
{
    // DOORLOCK_DBG("mic_is_streaming = %d\n", snd_card_info->mic_is_streaming);
    int avail_nums = 0;
    int count = 0;
    int read_len = 0;
    snd_alsa_config_t *snd_cfg = &snd_card_info->mic_alsa_cfg;
    if ((snd_cfg->snd_handle != NULL) && (snd_card_info->mic_is_streaming)) {
        count = data_size / snd_cfg->bytes_per_frame;
        DOORLOCK_DBG("try to read data_size = %d (Bytes)\n", data_size);
#if 0
		avail_nums = snd_pcm_avail(snd_cfg->snd_handle);
		//avail_nums = snd_pcm_avail_update(snd_cfg->snd_handle);
		//DOORLOCK_DBG("snd_pcm_avail in frames %d\n", avail_nums);
		//if (avail_nums >= snd_cfg->buffer_size/2)
		//if (avail_nums >= count)
		{
			//count = avail_nums>count?count:avail_nums;
			read_len = snd_pcm_readi(snd_cfg->snd_handle, data_buf, count);
			DOORLOCK_DBG("snd_pcm_readi read_len = %d (Frames)\n", read_len);
			if (read_len == -EAGAIN) {
				snd_pcm_wait(snd_cfg->snd_handle, 100);
				DOORLOCK_ERR("EGAIN read_len[%d], count[%d]\n", read_len, count);
			} else if (read_len == -EPIPE) {
				snd_xrun(snd_cfg->snd_handle);//overrun, app read too slow
				DOORLOCK_ERR("EPIPE\n");
			} else if (read_len == -ESTRPIPE) {
				snd_suspend(snd_cfg->snd_handle);
				DOORLOCK_ERR("ESTRPIPE\n");
			} else if (read_len < 0) {
				DOORLOCK_ERR("error from readi: %s\n",snd_strerror(read_len));
				snd_suspend(snd_cfg->snd_handle);
			} else if (read_len < count/2){
				snd_pcm_wait(snd_cfg->snd_handle, 100);
			}
		}
		//if (avail_nums == 0){
		//	snd_pcm_prepare(snd_cfg->snd_handle);
		//	snd_pcm_start(snd_cfg->snd_handle);
		//}
#else
        read_len = snd_pcm_readi(snd_cfg->snd_handle, data_buf, count);
        if (read_len < 0) {
            snd_pcm_prepare(snd_cfg->snd_handle);
            // usleep(1000*20);
            DOORLOCK_INFO("overrun!!!\n");
        }
#endif
    } else {
        DOORLOCK_ERR("mSndCardFd = %d, mic_is_streaming = %d\n", snd_card_info->snd_card_fd,
                     snd_card_info->mic_is_streaming);
    }
    return read_len * snd_cfg->bytes_per_frame;
}

int snd_card_write_spk_frame(snd_card_t *snd_card_info, uint8_t *data_buf, uint32_t data_size)
{
    // DOORLOCK_DBG("spk_is_streaming = %d\n", snd_card_info->spk_is_streaming);
    int avail_nums = 0;
    int count = 0;
    int write_len = 0;
    snd_alsa_config_t *snd_cfg = &snd_card_info->spk_alsa_cfg;
    if ((snd_cfg->snd_handle != NULL) && (snd_card_info->spk_is_streaming)) {
        // DOORLOCK_DBG("try to write dataSize = %d (Bytes)\n", dataSize);
        count = data_size / snd_cfg->bytes_per_frame;
#if 0
		//avail_nums = snd_pcm_avail_update(snd_cfg->snd_handle);
		//DOORLOCK_DBG("snd_pcm_avail in frames %d\n", avail_nums);
		//if (avail_nums >= snd_cfg->buffer_size/2)
		//if (avail_nums >= count)
		{
			//count = avail_nums>count?count:avail_nums;
			write_len = snd_pcm_writei(snd_cfg->snd_handle, data_buf, count);
			if (write_len == -EAGAIN) {
				snd_pcm_wait(snd_cfg->snd_handle, 100);
				DOORLOCK_ERR("EGAIN ret[%d], count[%d]\n", write_len, count);
			} else if (write_len == -EPIPE) {
				snd_xrun(snd_cfg->snd_handle);//underrun, app write too slow
				DOORLOCK_ERR("EPIPE\n");
			} else if (write_len == -ESTRPIPE) {
				snd_suspend(snd_cfg->snd_handle);
				SENSLAB_ERR("ESTRPIPE\n");
			} else if (write_len < 0) {
				DOORLOCK_ERR("error from writei: %s\n",snd_strerror(write_len));
				snd_suspend(snd_cfg->snd_handle);
			} else if (write_len < count/2){
				snd_pcm_wait(snd_cfg->snd_handle, 100);
			}
		}
#else
        write_len = snd_pcm_writei(snd_cfg->snd_handle, data_buf, count);
        if (write_len < 0) {
            DOORLOCK_INFO("underrun!!!\n");
            snd_pcm_prepare(snd_cfg->snd_handle);
            // usleep(1000*20);
            // snd_pcm_recover(snd_cfg->snd_handle, write_len, 0);
        }
#endif
    } else {
        DOORLOCK_ERR("snd_card_fd = %d, spk_is_streaming = %d\n", snd_card_info->snd_card_fd,
                     snd_card_info->spk_is_streaming);
    }

    return write_len * snd_cfg->bytes_per_frame;
}

int snd_card_set_mic_volume(snd_card_t *snd_card_info, int volume_percent)
{
#if 0
	return SndCardSetVolume(snd_card_info->snd_devname, MIC_VOLUME_CONTROL, volume_percent);
#else
    char cmd_buf[1024] = {0};
    memset(cmd_buf, 0, sizeof(cmd_buf));
    sprintf(cmd_buf, "amixer -D hw:audiocodec cset name='MIC1 gain volume' %d",
            convert_volume(volume_percent, 0, 31));
    system(cmd_buf);
#endif
}

int snd_card_set_spk_volume(snd_card_t *snd_card_info, int volume_percent)
{
#if 0
	return SndCardSetVolume(snd_card_info->snd_devname, SPK_VOLUME_CONTROL, volume_percent);
#else
    char cmd_buf[1024] = {0};
    memset(cmd_buf, 0, sizeof(cmd_buf));
    sprintf(cmd_buf, "amixer -D hw:audiocodec cset name='LINEOUT volume' %d",
            convert_volume(volume_percent, 0, 31));
    system(cmd_buf);
#endif
}

int snd_card_init(snd_card_t *snd_card_info)
{
    DOORLOCK_DBG("enter ===>\n");
    g_snd_card_info = snd_card_info;

    if (strlen(snd_card_info->snd_devname) == 0) {
        strcpy(snd_card_info->snd_devname, "audiocodec");
    }
    while (1) {
        if (snd_card_find(snd_card_info->snd_devname) == 0) {
            break;
        }
    }
    if (snd_card_info->mic) {
        // snd_card_open_mic(snd_card_info);
    }
    if (snd_card_info->spk) {
        // snd_card_open_spk(snd_card_info);
    }

    DOORLOCK_DBG("exit  <=== !!\n");
    return 0;
}

int snd_card_deinit(snd_card_t *snd_card_info)
{
    DOORLOCK_DBG("enter ===>\n");

    if (snd_card_info->mic) {
        // snd_card_close_mic(snd_card_info);
    }
    if (snd_card_info->spk) {
        // snd_card_close_spk(snd_card_info);
    }

    snd_card_info->snd_card_fd = 0;
    g_snd_card_info = NULL;

    DOORLOCK_DBG("exit  <=== !!\n");
    return 0;
}
