#include <sys/time.h>
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
#include <sys/types.h>
#include <unistd.h>

#include "doorlock_common.h"
#include "aw_uacdev.h"

uac_dev_t *g_uac_dev_info = NULL;

void gen_set_uac_function_script(uac_dev_t *uac_dev_info)
{
    //  ==> /tmp/set_uac_function.sh
    // uac_dev_t *uac_dev_info = g_uac_dev_info;
    char shell_cmd_buf[1024];
    FILE *fp = fopen("/tmp/set_uac_function.sh", "w+");
    if (fp == NULL) {
        DOORLOCK_ERR("fopen /tmp/set_uac_function.sh failed\n");
        return;
    }
    // memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    // sprintf(shell_cmd_buf, "mkdir /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0 \n");
    // fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    if (uac_dev_info->mic) {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo %d > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/p_chmask \n",
                uac_dev_info->mic_channel_cnt * 2 - 1);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo %d > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/p_srate \n",
                uac_dev_info->mic_sample_rate);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo %d > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/p_ssize \n",
                uac_dev_info->mic_bitwidth / 8);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    } else {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo 0 > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/p_chmask \n");
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo 0 > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/p_srate \n");
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo 0 > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/p_ssize \n");
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    }

    if (uac_dev_info->spk) {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo %d > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/c_chmask \n",
                uac_dev_info->spk_channel_cnt * 2 - 1);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo %d > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/c_srate \n",
                uac_dev_info->spk_sample_rate);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo %d > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/c_ssize \n",
                uac_dev_info->spk_bitwidth / 8);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    } else {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo 0 > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/c_chmask \n");
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo 0 > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/c_srate \n");
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf,
                "echo 0 > /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/c_ssize \n");
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    }

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "echo \"Tina UAC1\" > /sys/kernel/config/usb_gadget/g1/strings/0x409/product \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "ln -s /sys/kernel/config/usb_gadget/g1/functions/uac1.usb0/ "
            "/sys/kernel/config/usb_gadget/g1/configs/c.1/uac1.usb0 \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    if (uac_dev_info->usb_vid) {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf, "echo 0x%04x > /sys/kernel/config/usb_gadget/g1/idVendor \n",
                uac_dev_info->usb_vid);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    }

    if (uac_dev_info->usb_pid) {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf, "echo 0x%04x > /sys/kernel/config/usb_gadget/g1/idProduct \n",
                uac_dev_info->usb_pid);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    }

    fclose(fp);
}

int uac_dev_is_spk_stream_on()
{
    FILE *my_file = fopen("/sys/kernel/config/usb_gadget/g1/configs/c.1/uac1.usb0/out_alt", "rb");
    if (my_file == NULL) {
        return 1;  // always on
    }
    char tmp_buf[10];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    fread(tmp_buf, 1, sizeof(tmp_buf) - 1, my_file);
    int tmpInt = 0;
    if (strlen(tmp_buf)) {
        tmpInt = atoi(tmp_buf);
    }
    fclose(my_file);
    return tmpInt;
}

int uac_dev_is_mic_stream_on()
{
    FILE *my_file = fopen("/sys/kernel/config/usb_gadget/g1/configs/c.1/uac1.usb0/in_alt", "rb");

    if (my_file == NULL) {
        return 1;  // always on
    }
    char tmp_buf[10];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    fread(tmp_buf, 1, sizeof(tmp_buf) - 1, my_file);
    int tmpInt = 0;
    if (strlen(tmp_buf)) {
        tmpInt = atoi(tmp_buf);
    }
    fclose(my_file);
    return tmpInt;
}

static int uac_dev_init_sndcard(uac_alsa_config_t *uac_cfg, char *dev_name, snd_pcm_stream_t stream,
                             uint32_t bit_width, uint32_t channel_cnt, uint32_t sample_rate)
{
    int rc;
    char snd_name[256];
    DOORLOCK_DBG("enter ===>\n");
    memset(snd_name, 0, sizeof(snd_name));
    sprintf(snd_name, "hw:%s", dev_name);

    DOORLOCK_DBG("snd card name: [%s]\n", snd_name);
     // SND_PCM_STREAM_PLAYBACK, SND_PCM_STREAM_CAPTURE
    rc = snd_pcm_open(&uac_cfg->uac_handle, snd_name, stream, 0);
    if (rc < 0) {
        uac_cfg->uac_handle = NULL;
        DOORLOCK_ERR("unable to open pcm device, stream = %d\n", stream);
        DOORLOCK_DBG("exit <===\n");
        return -1;
    } else {
        DOORLOCK_INFO("open pcm device success, stream = %d\n", stream);
    }

    /* Allocate a hardware parameters object */
    snd_pcm_hw_params_alloca(&uac_cfg->uac_hwparams);
    /* Fill it in with default values. */
    rc = snd_pcm_hw_params_any(uac_cfg->uac_handle, uac_cfg->uac_hwparams);
    if (rc < 0) {
        DOORLOCK_ERR("unable to Fill it in with default values.\n");
        goto err1;
    }

    /* Interleaved mode */
    rc = snd_pcm_hw_params_set_access(uac_cfg->uac_handle, uac_cfg->uac_hwparams,
                                      SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) {
        DOORLOCK_ERR("unable to Interleaved mode.\n");
        goto err1;
    }

    snd_pcm_format_t format;

    switch (bit_width) {
    case 8:
        DOORLOCK_DBG("set 8bit for uac\n");
        format = SND_PCM_FORMAT_S8;
        break;
    case 16:
        DOORLOCK_DBG("set 16bit for uac\n");
        format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        DOORLOCK_DBG("set 24bit for uac\n");
        format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        DOORLOCK_DBG("set 32bit for uac\n");
        format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        DOORLOCK_ERR("SND_PCM_FORMAT_UNKNOWN.\n");
        format = SND_PCM_FORMAT_UNKNOWN;
        goto err1;
    }

    /* set format */
    rc = snd_pcm_hw_params_set_format(uac_cfg->uac_handle, uac_cfg->uac_hwparams, format);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set format.\n");
        goto err1;
    }

    /* set channels (stero)  uac to pc only support stero */
    rc = snd_pcm_hw_params_set_channels(uac_cfg->uac_handle, uac_cfg->uac_hwparams, channel_cnt);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set channels (stero).\n");
        goto err1;
    }

    /* set sampling rate */
    unsigned int dir;
    unsigned int rate = sample_rate;
    rc = snd_pcm_hw_params_set_rate_near(uac_cfg->uac_handle, uac_cfg->uac_hwparams, &rate, &dir);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set sampling rate.\n");
        goto err1;
    }

    if (uac_cfg->period_size == 0) {
        // uac_cfg->period_size = 1024;
        uac_cfg->period_size = 256 * sample_rate / 8000;
    }
    if (uac_cfg->periods == 0) uac_cfg->periods = 4;

    uac_cfg->bytes_per_frame = channel_cnt * bit_width / 8;
    DOORLOCK_INFO("snd set uac card %d, period_size %d, periods %d\n", stream, uac_cfg->period_size,
                 uac_cfg->periods);

    snd_pcm_hw_params_set_period_size_near(uac_cfg->uac_handle, uac_cfg->uac_hwparams,
                                           &uac_cfg->period_size, &dir);
    snd_pcm_hw_params_set_periods(uac_cfg->uac_handle, uac_cfg->uac_hwparams, uac_cfg->periods, 0);

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize(in bytes) * periods / (rate * bytes_per_frame)     */
    /* latency = periodsize(in frames) * periods / rate     */
    uac_cfg->buffer_size = (uac_cfg->period_size * uac_cfg->periods);
    DOORLOCK_INFO("snd set uac card %d, buffer_size %d\n", stream, uac_cfg->buffer_size);
    snd_pcm_hw_params_set_buffer_size(uac_cfg->uac_handle, uac_cfg->uac_hwparams,
                                      uac_cfg->buffer_size);

    /* Write the parameters to the dirver */
    rc = snd_pcm_hw_params(uac_cfg->uac_handle, uac_cfg->uac_hwparams);
    if (rc < 0) {
        DOORLOCK_ERR("unable to set hw parameters: %s\n", snd_strerror(rc));
        goto err1;
    }

    snd_pcm_hw_params_get_period_size(uac_cfg->uac_hwparams, &uac_cfg->period_size, &dir);
    DOORLOCK_INFO("snd get uac card period_size :%d \n", uac_cfg->period_size);

    snd_pcm_hw_params_get_periods(uac_cfg->uac_hwparams, &uac_cfg->periods, &dir);
    DOORLOCK_INFO("snd get uac card periods :%d \n", uac_cfg->periods);

    snd_pcm_hw_params_get_buffer_size(uac_cfg->uac_hwparams,
                                      (snd_pcm_uframes_t *)&uac_cfg->buffer_size);
    DOORLOCK_INFO("snd get uac card buffer size = %d frames\n", uac_cfg->buffer_size);

#if 1
    snd_pcm_sw_params_alloca(&uac_cfg->uac_swparams);
    snd_pcm_sw_params_current(uac_cfg->uac_handle, uac_cfg->uac_swparams);
    snd_pcm_sw_params_set_avail_min(uac_cfg->uac_handle, uac_cfg->uac_swparams,
                                    uac_cfg->period_size);
    int start_threshold;
    int stop_threshold;
#if 1
    if (stream == SND_PCM_STREAM_CAPTURE) {
        // spk
        start_threshold = uac_cfg->period_size / 2;
        stop_threshold = uac_cfg->buffer_size;
    } else  // SND_PCM_STREAM_PLAYBACK
#endif
    {
        // mic
        start_threshold = 0;  // uac_cfg->buffer_size;
        stop_threshold = uac_cfg->buffer_size;
    }
    snd_pcm_sw_params_set_start_threshold(uac_cfg->uac_handle, uac_cfg->uac_swparams,
                                          start_threshold);
    snd_pcm_sw_params_set_stop_threshold(uac_cfg->uac_handle, uac_cfg->uac_swparams,
                                         stop_threshold);
    snd_pcm_sw_params(uac_cfg->uac_handle, uac_cfg->uac_swparams);
#endif

    if (uac_cfg->nonblock_mode) {
        snd_pcm_nonblock(uac_cfg->uac_handle, 1);
    }

    snd_pcm_prepare(uac_cfg->uac_handle);
    snd_pcm_start(uac_cfg->uac_handle);

    DOORLOCK_DBG("exit <===\n");
    return 0;

err1:
    snd_pcm_close(uac_cfg->uac_handle);
    uac_cfg->uac_handle = NULL;
    DOORLOCK_DBG("exit <===\n");
    return -1;
}

static int uac_dev_deinit_sndcard(uac_alsa_config_t *uac_cfg)
{
    DOORLOCK_DBG("enter ===>\n");
    if (uac_cfg->uac_handle != NULL) {
        snd_pcm_drain(uac_cfg->uac_handle);
        snd_pcm_close(uac_cfg->uac_handle);
    } else {
        DOORLOCK_ERR("SND close failed, uac_handle == NULL\n");
        return -1;
    }
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

static int uac_dev_open_micsnd(uac_dev_t *uac_dev_info)
{
    int rc = -1;
    uac_alsa_config_t *uac_cfg = NULL;

    DOORLOCK_DBG("enter ===>\n");
    DOORLOCK_DBG("uac audio card : %s \n", uac_dev_info->uac_devname);
    if (uac_dev_info->mic) {
        uac_alsa_config_t *uac_cfg = &uac_dev_info->mic_alsa_cfg;
        uac_cfg->nonblock_mode = uac_dev_info->mic_nonblock;
        if (uac_cfg->nonblock_mode) {
            DOORLOCK_INFO("UAC MIC nonblock mode !!!\n");
        } else {
            DOORLOCK_INFO("UAC MIC block mode !!!\n");
        }
        rc = uac_dev_init_sndcard(uac_cfg, uac_dev_info->uac_devname, SND_PCM_STREAM_PLAYBACK,
                               uac_dev_info->mic_bitwidth, uac_dev_info->mic_channel_cnt,
                               uac_dev_info->mic_sample_rate);
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

static int uac_dev_close_micsnd(uac_dev_t *uac_dev_info)
{
    int rc = -1;
    uac_alsa_config_t *uac_cfg = NULL;

    DOORLOCK_DBG("enter ===>\n");
    if (uac_dev_info->mic) {
        uac_alsa_config_t *uac_cfg = &uac_dev_info->mic_alsa_cfg;
        rc = uac_dev_deinit_sndcard(uac_cfg);
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

static int uac_dev_open_spksnd(uac_dev_t *uac_dev_info)
{
    int rc = -1;
    uac_alsa_config_t *uac_cfg = NULL;

    DOORLOCK_DBG("enter ===>\n");
    DOORLOCK_DBG("uac audio card : %s \n", uac_dev_info->uac_devname);
    if (uac_dev_info->spk) {
        uac_alsa_config_t *uac_cfg = &uac_dev_info->spk_alsa_cfg;
        uac_cfg->nonblock_mode = uac_dev_info->spk_nonblock;
        if (uac_cfg->nonblock_mode) {
            DOORLOCK_INFO("UAC SPK nonblock mode !!!\n");
        } else {
            DOORLOCK_INFO("UAC SPK block mode !!!\n");
        }
        rc = uac_dev_init_sndcard(uac_cfg, uac_dev_info->uac_devname, SND_PCM_STREAM_CAPTURE,
                               uac_dev_info->spk_bitwidth, uac_dev_info->spk_channel_cnt,
                               uac_dev_info->spk_sample_rate);
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

static int uac_dev_close_spksnd(uac_dev_t *uac_dev_info)
{
    int rc = -1;
    uac_alsa_config_t *uac_cfg = NULL;

    DOORLOCK_DBG("enter ===>\n");
    if (uac_dev_info->spk) {
        uac_alsa_config_t *uac_cfg = &uac_dev_info->spk_alsa_cfg;
        rc = uac_dev_deinit_sndcard(uac_cfg);
    }
    DOORLOCK_DBG("exit <===\n");
    return rc;
}

static int uac_dev_find_sndcard(char *dev_name)
{
    int find = -1;
    int total_cards = 0;  // No cards found yet
    int card_num = -1;    // Start with first card
    int err;
    snd_ctl_t *handle = NULL;
    snd_ctl_card_info_t *info = NULL;
    snd_pcm_info_t *pcm_info = NULL;
    int card = -1;
    char hwdev[256] = {0};

    if (dev_name == NULL) {
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
            break;
        }

        ++total_cards;
        DOORLOCK_DBG("ALSA found %i card(s)\n", total_cards);
        memset(hwdev, 0, sizeof(hwdev));
        sprintf(hwdev, "hw:%d", card_num);
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
            DOORLOCK_DBG("Got it\n");
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
						devNum, pcmID, pcmName, nsubd, snd_pcm_info_get_subdevices_avail(pcm_info), stream);
				}else{
					DOORLOCK_DBG("  Device %d, ID [%s], name [%s], %d subdevices (%d available), stream %d, SND_PCM_STREAM_CAPTURE\n", \
						devNum, pcmID, pcmName, nsubd, snd_pcm_info_get_subdevices_avail(pcm_info), stream);
				}
				//memset(hwdev, 0, sizeof(hwdev));
				//sprintf(hwdev, "hw:%d,%d", card_num, devNum);
				//err = snd_pcm_open(&pcm, hwdev, stream, SND_PCM_NONBLOCK);
			}
		}
#endif
        snd_ctl_close(handle);
        if (find == 0) {
            break;
        }
    }

    snd_ctl_card_info_free(info);
    snd_pcm_info_free(pcm_info);
    snd_config_update_free_global();
    return find;
}

static void uac_dev_mic_stream_on(uac_dev_t *uac_dev_info, int on)
{
    int ret = 0;
    uac_dev_info->mic_frame_cnt = 0;

    DOORLOCK_DBG("enter ===>on %d\n", on);
    // pthread_mutex_lock(&g_uac_dev_info->uac_dev_lock);
    if (on) {
        // ret = 0;
        if (ret == 0) {
            DOORLOCK_WARN("mic begin to streaming\n");
#if !UAC_TEST
            while (!uac_dev_info->force_exit) {
                if (uac_dev_open_micsnd(uac_dev_info) != 0) {
                    DOORLOCK_ERR("uac_dev_open_micsnd failed\n");
                } else {
                    break;
                }
                usleep(10 * 1000);
            }
#endif
            g_uac_dev_info->mic_is_streaming = 1;
        } else {
            DOORLOCK_ERR("VIDIOC_STREAMON failed\n");
        }

    } else {
        if (ret == 0) {
            DOORLOCK_WARN("mic stop to streaming\n");
#if !UAC_TEST
            uac_dev_close_micsnd(uac_dev_info);
#endif
            g_uac_dev_info->mic_is_streaming = 0;
        } else {
            DOORLOCK_ERR("VIDIOC_STREAMOFF failed\n");
        }
    }
    // pthread_mutex_unlock(&g_uac_dev_info->uac_dev_lock);
    DOORLOCK_DBG("exit <=== on=%d,\n", on);
    return;
}

static void uac_dev_spk_stream_on(uac_dev_t *uac_dev_info, int on)
{
    int ret = 0;
    uac_dev_info->spk_frame_cnt = 0;
    DOORLOCK_DBG("enter ===>on=%d, \n", on);

    // pthread_mutex_lock(&g_uac_dev_info->uac_dev_lock);
    if (on) {
        // ret = 0;
        if (ret == 0) {
            DOORLOCK_WARN("spk begin to streaming\n");
            g_uac_dev_info->spk_is_streaming = 1;
#if !UAC_TEST
            while (!uac_dev_info->force_exit) {
                if (uac_dev_open_spksnd(uac_dev_info) != 0) {
                    DOORLOCK_ERR("uac_dev_open_micsnd failed\n");
                } else {
                    break;
                }
                usleep(10 * 1000);
            }
#endif
        } else {
            DOORLOCK_ERR("VIDIOC_STREAMON failed\n");
        }
    } else {
        if (ret == 0) {
            DOORLOCK_WARN("spk stop to streaming\n");
#if !UAC_TEST
            uac_dev_close_spksnd(uac_dev_info);
#endif
            g_uac_dev_info->spk_is_streaming = 0;
        } else {
            DOORLOCK_ERR("VIDIOC_STREAMOFF failed\n");
        }
    }
    // pthread_mutex_unlock(&g_uac_dev_info->uac_dev_lock);

    DOORLOCK_DBG("exit flag=%d, ===>\n", on);
    return;
}

static int uac_dev_produce_audio_spk_frame(uac_dev_t *uac_dev_info)
{
    int ret = 0;
    frm_manager_t *frm_manager = &uac_dev_info->spk_frm_manager;
    frame_mem_t *buf_tmp = NULL;

    if (frm_manager->prefetch_first_idle_frame)
        ret = frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);

    if (buf_tmp == NULL || ret < 0) {
        // DOORLOCK_DBG("prefetch_first_idle_frame fail\n");
        return -1;
    }
    // DOORLOCK_INFO("prefetch_first_idle_frame success\n");
    // do something
    int audio_data_size = 0;
    int alreadySize = 0;
#if 0
	int packetSize = uac_dev_info->spk_alsa_cfg.period_size*uac_dev_info->spk_alsa_cfg.bytes_per_frame;
	//while(alreadySize < buf_tmp->mem_info.mem_size)
#else
    int packetSize = buf_tmp->mem_info.mem_size;
#endif
    {
        if (alreadySize + packetSize > buf_tmp->mem_info.mem_size) {
            // last one
            packetSize = buf_tmp->mem_info.mem_size - alreadySize;
        }
#if UAC_CHECK_EVENT_SUPPORT
        int ready_to_transfer = uac_dev_is_spk_stream_on();
#if UAC_SPK_SAMPLING_FREQ
        ready_to_transfer = ready_to_transfer == 2 ? 1 : 0;
#endif
        if (ready_to_transfer) {
            audio_data_size = uac_dev_transfer_audio_spk_frame(
                uac_dev_info, buf_tmp->mem_info.mem_vir + alreadySize, packetSize);
        } else {
            // DOORLOCK_ERR("UACSpk not stream on yet!\n");
        }
#else
        audio_data_size = uac_dev_transfer_audio_spk_frame(
            uac_dev_info, buf_tmp->mem_info.mem_vir + alreadySize, packetSize);
#endif
        if (audio_data_size > 0) {
            alreadySize += audio_data_size;
        }
        usleep(1 * 1000);
    }
    audio_data_size = alreadySize;

    if (uac_dev_info->spk_frame_cnt == 0) {
        if (audio_data_size > 0)
            printf("spk frame [%d], size = %d, us = %lld\n", uac_dev_info->spk_frame_cnt,
                   audio_data_size, get_cur_time_us());
    }

    // DOORLOCK_INFO("audio_data_size = %d\n", audio_data_size);
    if (audio_data_size <= 0) {
        return -1;
    }
    buf_tmp->cur_mem_size = audio_data_size;
    if (frm_manager->first_idle_to_using_frame)
        frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);

    uac_dev_info->spk_frame_cnt++;
    return 0;
}

static int uac_dev_produce_audio_spk_frame_fake(uac_dev_t *uac_dev_info)
{
    int ret = 0;
    static int transfer_len = 0;
    frm_manager_t *frm_manager = &uac_dev_info->spk_frm_manager;
    frame_mem_t *buf_tmp = NULL;

    if (frm_manager->prefetch_first_idle_frame)
        ret = frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);

    if (buf_tmp == NULL || ret < 0) {
        // DOORLOCK_DBG("prefetch_first_idle_frame fail\n");
        return -1;
    }
    // DOORLOCK_INFO("prefetch_first_idle_frame success\n");
    // do something
    int audio_data_size = buf_tmp->mem_info.mem_size;

    if (1) {
        char *file_name = "/data/spk-8k-16bit-mono.pcm";
        FILE *my_file = fopen(file_name, "rb");
        if (my_file) {
            fseek(my_file, transfer_len, SEEK_SET);
            fread(buf_tmp->mem_info.mem_vir, 1, audio_data_size, my_file);
            transfer_len += audio_data_size;
            if (feof(my_file)) {
                DOORLOCK_INFO("fake spk file ends, loop again\n");
                transfer_len = 0;
            }
            fclose(my_file);
        } else {
            DOORLOCK_WARN("fopen %s failed\n", file_name);
            memset(buf_tmp->mem_info.mem_vir, 0, buf_tmp->mem_info.mem_size);
        }
    }

    if (uac_dev_info->spk_frame_cnt == 0) {
        DOORLOCK_INFO("spk fake frame  [%d], size = %d, us = %lld\n", uac_dev_info->spk_frame_cnt,
               audio_data_size, get_cur_time_us());
    }

    buf_tmp->cur_mem_size = audio_data_size;
    if (frm_manager->first_idle_to_using_frame)
        frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);

    // uac_dev_info->spk_frame_cnt++;
    return 0;
}

static int uac_dev_consume_audio_mic_frame(uac_dev_t *uac_dev_info)
{
    int ret = 0;
    uint8_t *data_buf = NULL;
    frm_manager_t *frm_manager = &uac_dev_info->mic_frm_manager;
    frame_mem_t *buf_tmp = NULL;

    if (frm_manager->prefetch_first_using_frame)
        ret = frm_manager->prefetch_first_using_frame(frm_manager, &buf_tmp);
    if (buf_tmp == NULL || ret < 0) {
        // DOORLOCK_DBG("frm_manager->prefetch_first_using_frame fail\n");
        return -1;
    } else {
        // DOORLOCK_DBG("frm_manager->prefetch_first_using_frame success\n");
    }

    if (buf_tmp->cur_mem_size > 0 && buf_tmp->cur_mem_size <= uac_dev_info->max_mic_frame_size) {
        // DOORLOCK_DBG("cur_mem_size = %d\n", buf_tmp->cur_mem_size);
        int alreadySize = 0;
#if 0
		int packetSize = uac_dev_info->mic_alsa_cfg.period_size*uac_dev_info->mic_alsa_cfg.bytes_per_frame;
		while(alreadySize < buf_tmp->cur_mem_size)
#else
        int packetSize = buf_tmp->cur_mem_size;
#endif
        {
            int write_len = 0;
            if (alreadySize + packetSize > buf_tmp->cur_mem_size) {
                // last one
                packetSize = buf_tmp->cur_mem_size - alreadySize;
            }
#if UAC_CHECK_EVENT_SUPPORT
            int ready_to_transfer = uac_dev_is_mic_stream_on();
#if UAC_MIC_SAMPLING_FREQ
            ready_to_transfer = (ready_to_transfer == 2) ? 1 : 0;
#endif
            if (ready_to_transfer) {
                write_len = uac_dev_transfer_audio_mic_frame(
                    uac_dev_info, buf_tmp->mem_info.mem_vir + alreadySize, packetSize);
            } else {
                // DOORLOCK_ERR("UACMic not stream on yet!\n");
            }
#else
            write_len = uac_dev_transfer_audio_mic_frame(uac_dev_info, buf_tmp->mem_info.mem_vir + alreadySize,
                                                   packetSize);
#endif
            if (write_len > 0) {
                alreadySize += packetSize;
            }
            usleep(1 * 1000);
        }
    } else {
        DOORLOCK_ERR("cur_mem_size = %d, max_mic_frame_size = %d\n", buf_tmp->cur_mem_size,
                    uac_dev_info->max_mic_frame_size);
    }

    frm_manager->first_using_to_idle_frame(frm_manager, buf_tmp);
    return ret;
}

static int uac_dev_consume_audio_mic_frame_fake(uac_dev_t *uac_dev_info)
{
    static int transfer_len = 0;
    int ret = 0;
    uint8_t audio_data_buf[1024];
    int audio_data_size = sizeof(audio_data_buf);
    if (1) {
        char *file_name = "/data/mic-8k-16bit-mono.pcm";
        FILE *my_file = fopen(file_name, "rb");
        if (my_file) {
            fseek(my_file, transfer_len, SEEK_SET);
            fread(audio_data_buf, 1, audio_data_size, my_file);
            transfer_len += audio_data_size;
            if (feof(my_file)) {
                DOORLOCK_INFO("fake mic file ends, loop again\n");
                transfer_len = 0;
            }
            fclose(my_file);
        } else {
            DOORLOCK_WARN("fopen %s failed\n", file_name);
            memset(audio_data_buf, 0, audio_data_size);
        }
    }
    ret = uac_dev_transfer_audio_mic_frame(uac_dev_info, audio_data_buf, audio_data_size);
    if (ret > 0) {
        DOORLOCK_INFO("write fake frame len = %d\n", ret);
    }
    return ret;
}

static void *uac_dev_mic_thread(void *arg)
{
    int exit_flag = false;
    uac_dev_t *uac_dev_info = (uac_dev_t *)g_uac_dev_info;

    DOORLOCK_DBG("enter ===>\n");

#if !UAC_TEST
    if (uac_dev_info->fast_connect) {
        uac_dev_fast_connect_mic(uac_dev_info);
    }
#if 0
	while(!uac_dev_info->force_exit){
		// /proc/asound/UAC1Gadget/pcm0p/info
		// /proc/asound/UAC1Gadget/pcm0c/info
		if(access(uac_dev_info->uac_devname, F_OK) == 0){
			break;
		}
		usleep(10*1000);
	}
#else
    while (!uac_dev_info->force_exit) {
        if (uac_dev_find_sndcard(uac_dev_info->uac_devname) == 0) {
            break;
        }
        usleep(10 * 1000);
    }
#endif
#endif

#if 1  //! UAC_CHECK_EVENT_SUPPORT
    uac_dev_mic_stream_on(uac_dev_info, true);
#if 0  //! UAC_TEST
	while(uac_dev_consume_audio_mic_frame_fake(uac_dev_info) <= 0){
		usleep(10*1000);
	}
#endif
#endif

    while (1) {
#if 0  // UAC_CHECK_EVENT_SUPPORT
		int isMicOn = uac_dev_is_mic_stream_on();
		if(isMicOn && uac_dev_info->mic_is_streaming == 0){
#if !UAC_MIC_SAMPLING_FREQ
			usleep(300*1000);
#endif
			uac_dev_mic_stream_on(uac_dev_info, true);
		}else if(isMicOn == 0 && uac_dev_info->mic_is_streaming){
			uac_dev_mic_stream_on(uac_dev_info, false);
		}
#endif
        if (exit_flag) {
            // when receive exit sinal
            DOORLOCK_INFO("time to exit UACDev mic\n");
            // ioctl(UACDev_fd, UACDev_EVENT_DISCONNECT, NULL);
            uac_dev_mic_stream_on(g_uac_dev_info, false);
        }
        // snd_pcm_state_t micState = snd_pcm_state(g_uac_dev_info->mic_alsa_cfg.uac_handle);
        // DOORLOCK_INFO("micState = %d\n", micState);

        // DOORLOCK_DBG("mic_is_streaming = %d, mic_is_resource_ok = %d\n",
        // uac_dev_info->mic_is_streaming, uac_dev_info->mic_is_resource_ok);
        if (uac_dev_info->mic_is_streaming && (uac_dev_info->mic_is_resource_ok == 0)) {
#if !UAC_CHECK_EVENT_SUPPORT
            DOORLOCK_INFO("time to create UACDev Resource mic\n");
#if !UAC_TEST
            if (uac_dev_info->mic_resource_on_off) {
                if (uac_dev_info->mic_frm_manager.frm_node_cnt == 0) {
                    uac_dev_info->mic_frm_manager.frm_node_cnt = 2;
                }
                uac_dev_info->mic_frm_manager.frm_node_memsize = uac_dev_info->max_mic_frame_size;
                frm_manager_init(&uac_dev_info->mic_frm_manager);
                uac_dev_info->mic_resource_on_off(uac_dev_info, 1);
            } else {
                DOORLOCK_ERR("mic_resource_on_off = NULL\n");
            }
#else
            uac_dev_info->mic_is_resource_ok = 1;
#endif
#endif
        } else if ((!uac_dev_info->mic_is_streaming) && (uac_dev_info->mic_is_resource_ok == 1)) {
#if !UAC_CHECK_EVENT_SUPPORT
            DOORLOCK_INFO("time to destroy UACDev Resource mic\n");
#if !UAC_TEST
            if (uac_dev_info->mic_resource_on_off) {
                uac_dev_info->mic_resource_on_off(uac_dev_info, 0);
                frm_manager_deinit(&uac_dev_info->mic_frm_manager);
            } else {
                DOORLOCK_ERR("mic_resource_on_off = NULL\n");
            }
#else
            uac_dev_info->mic_is_resource_ok = 0;
#endif
#endif
        } else if (uac_dev_info->mic_is_streaming && (uac_dev_info->mic_is_resource_ok == 1)) {
            // DOORLOCK_INFO("working mic\n");
            if (uac_dev_info->mic) {
#if !UAC_TEST
                uac_dev_consume_audio_mic_frame(uac_dev_info);
#endif
            } else {
                DOORLOCK_ERR("mic = %d\n", uac_dev_info->mic);
            }
            if (exit_flag) {
                break;  // when streaming
            }
        } else {
            // DOORLOCK_DBG("idle...\n");
            if (exit_flag) {
                break;  // when no streaming
            }
        }
        if (uac_dev_info->force_exit) {
            exit_flag = true;
        }

        usleep(1 * 1000);
    }

    DOORLOCK_DBG("exit <===\n");
}

static void *uac_dev_spk_thread(void *arg)
{
    uac_dev_t *uac_dev_info = (uac_dev_t *)g_uac_dev_info;
    int exit_flag = false;
    uint32_t fake_frame_cnt = 0;

    DOORLOCK_DBG("enter ===>\n");

#if !UAC_TEST
    if (uac_dev_info->fast_connect) {
        uac_dev_fast_connect_spk(uac_dev_info);
    }
#if 0
	while(!uac_dev_info->force_exit){
		if(access(uac_dev_info->uac_devname, F_OK) == 0){
			break;
		}
		usleep(10*1000);
	}
#else
    while (!uac_dev_info->force_exit) {
        if (uac_dev_find_sndcard(uac_dev_info->uac_devname) == 0) {
            break;
        }
        usleep(10 * 1000);
    }
#endif
#endif

#if 1  //! UAC_CHECK_EVENT_SUPPORT
    uac_dev_spk_stream_on(uac_dev_info, true);
#endif

    while (1) {
#if 0  // UAC_CHECK_EVENT_SUPPORT
		int isSpkOn = uac_dev_is_spk_stream_on();
		if(isSpkOn && uac_dev_info->spk_is_streaming == 0){
#if !UAC_MIC_SAMPLING_FREQ
			usleep(300*1000);
#endif
			uac_dev_spk_stream_on(uac_dev_info, true);
		}else if(isSpkOn == 0 && uac_dev_info->spk_is_streaming){
			uac_dev_spk_stream_on(uac_dev_info, false);
		}
#endif
        if (exit_flag) {
            // when receive exit sinal
            DOORLOCK_INFO("time to exit UACDev Spk\n");
            // ioctl(UACDev_fd, UACDev_EVENT_DISCONNECT, NULL);
            uac_dev_spk_stream_on(g_uac_dev_info, false);
        }
        // snd_pcm_state_t spkState = snd_pcm_state(g_uac_dev_info->spk_alsa_cfg.uac_handle);
        // DOORLOCK_INFO("spkState = %d\n", spkState);

        // DOORLOCK_DBG("spk_is_streaming = %d, spk_is_resource_ok = %d\n",
        // uac_dev_info->spk_is_streaming, uac_dev_info->spk_is_resource_ok);
        if (uac_dev_info->spk_is_streaming && (uac_dev_info->spk_is_resource_ok == 0)) {
#if !UAC_CHECK_EVENT_SUPPORT
            DOORLOCK_INFO("time to create UACDev Resource spk\n");
            fake_frame_cnt = 0;
#if !UAC_TEST
            if (uac_dev_info->spk_resource_on_off) {
                if (uac_dev_info->spk_frm_manager.frm_node_cnt == 0) {
                    uac_dev_info->spk_frm_manager.frm_node_cnt = 2;
                }
                uac_dev_info->spk_frm_manager.frm_node_memsize = uac_dev_info->max_spk_frame_size;
                frm_manager_init(&uac_dev_info->spk_frm_manager);
                uac_dev_info->spk_resource_on_off(uac_dev_info, 1);
            } else {
                DOORLOCK_ERR("spk_resource_on_off = NULL\n");
            }
#else
            uac_dev_info->spk_is_resource_ok = 1;
#endif
#endif
        } else if ((!uac_dev_info->spk_is_streaming) && (uac_dev_info->spk_is_resource_ok == 1)) {
#if !UAC_CHECK_EVENT_SUPPORT
            DOORLOCK_INFO("time to destroy UACDev Resource spk\n");
            fake_frame_cnt = 0;
#if !UAC_TEST
            if (uac_dev_info->spk_resource_on_off) {
                uac_dev_info->spk_resource_on_off(uac_dev_info, 0);
                frm_manager_deinit(&uac_dev_info->spk_frm_manager);
            } else {
                DOORLOCK_ERR("spk_resource_on_off = NULL\n");
            }
#else
            uac_dev_info->spk_is_resource_ok = 0;
#endif
#endif
        } else if (uac_dev_info->spk_is_streaming && (uac_dev_info->spk_is_resource_ok == 1)) {
            // DOORLOCK_INFO("working spk\n");
            if (uac_dev_info->spk) {
#if !UAC_TEST
                // if(fake_frame_cnt < 100)
                if (0) {
                    // send fake frame when uac spk not ready
                    uac_dev_produce_audio_spk_frame_fake(uac_dev_info);
                    fake_frame_cnt++;
                } else {
                    uac_dev_produce_audio_spk_frame(uac_dev_info);
                }
#endif
            } else {
                DOORLOCK_ERR("spk = %d\n", uac_dev_info->spk);
            }
            if (exit_flag) {
                break;  // when streaming
            }
        } else {
            // DOORLOCK_DBG("idle...\n");
            if (exit_flag) {
                break;  // when no streaming
            }
        }
        if (uac_dev_info->force_exit) {
            exit_flag = true;
        }

        usleep(1 * 1000);
    }

    DOORLOCK_DBG("=====<<<<<< spk exit\n");
}

// typedef enum _snd_pcm_state {
// /** Open */
// SND_PCM_STATE_OPEN = 0,
// /** Setup installed */
// SND_PCM_STATE_SETUP,
// /** Ready to start */
// SND_PCM_STATE_PREPARED,
// /** Running */
// SND_PCM_STATE_RUNNING,
// /** Stopped: underrun (playback) or overrun (capture) detected */
// SND_PCM_STATE_XRUN,
// /** Draining: running (playback) or stopped (capture) */
// SND_PCM_STATE_DRAINING,
// /** Paused */
// SND_PCM_STATE_PAUSED,
// /** Hardware is suspended */
// SND_PCM_STATE_SUSPENDED,
// /** Hardware is disconnected */
// SND_PCM_STATE_DISCONNECTED,
// SND_PCM_STATE_LAST = SND_PCM_STATE_DISCONNECTED
// } snd_pcm_state_t;

static void uac_state_process(snd_pcm_t *handle)
{
#if 1
    snd_pcm_status_t *status;
    int res;

    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(handle, status)) < 0) {
        DOORLOCK_ERR("status error: %s\n", snd_strerror(res));
    }
    snd_pcm_state_t pcm_state = snd_pcm_status_get_state(status);
    snd_pcm_status_free(status);
#else
    snd_pcm_state_t pcm_state = snd_pcm_state(handle);
#endif
    // DOORLOCK_INFO("pcm state: %d\n", pcm_state);
    if (pcm_state == SND_PCM_STATE_PREPARED) {
        DOORLOCK_INFO("pcm state prepared, ready to start!!!\n");
        snd_pcm_start(handle);
    }
    if (pcm_state != SND_PCM_STATE_RUNNING) {
        // DOORLOCK_ERR("pcm state not running yet!!!\n");
    }
    return;
}

/* I/O error handler */
static void uac_xrun(snd_pcm_t *handle)
{
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
        goto uac_xrun_exit;
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
        DOORLOCK_ERR("SND_PCM_STATE_DRAINING!!!\n");
    }
    DOORLOCK_ERR("read/write error, state = %s",
                snd_pcm_state_name(snd_pcm_status_get_state(status)));
uac_xrun_exit:
    snd_pcm_status_free(status);
    return;
}

/* I/O suspend handler */
static void uac_suspend(snd_pcm_t *handle)
{
    int res;
    // DOORLOCK_ERR("Suspended. Trying resume.\n");
    while ((res = snd_pcm_resume(handle)) == -EAGAIN)
        usleep(100 * 1000); /* wait until suspend flag is released */
    if (res < 0) {
        // DOORLOCK_ERR("Failed %d. Restarting stream.\n", res);
        if ((res = snd_pcm_prepare(handle)) < 0) {
            // DOORLOCK_ERR("suspend: prepare error: %s\n", snd_strerror(res));
        }
    }
    snd_pcm_start(handle);
    // DOORLOCK_ERR("Done.\n");
}

int uac_dev_transfer_audio_spk_frame(uac_dev_t *uac_dev_info, uint8_t *data_buf, uint32_t data_size)
{
    // DOORLOCK_DBG("spk_is_streaming = %d\n", uac_dev_info->spk_is_streaming);
    int avail_nums = 0;
    int count = 0;
    int read_len = 0;
    uac_alsa_config_t *uac_cfg = &uac_dev_info->spk_alsa_cfg;
    //   /sys/kernel/config/usb_gadget/g1/configs/c.1/uac1.usb0/out_alt

    if ((uac_cfg->uac_handle != NULL) && (uac_dev_info->spk_is_streaming != false)) {
        // DOORLOCK_DBG("try to read data_size = %d (Bytes)\n", data_size);
        count = data_size / uac_cfg->bytes_per_frame;
        if (uac_dev_info->spk_nonblock) {
            // uac_state_process(uac_cfg->uac_handle);
            // avail_nums = snd_pcm_avail_update(uac_cfg->uac_handle);
            avail_nums = snd_pcm_avail(uac_cfg->uac_handle);
            // DOORLOCK_DBG("spk snd_pcm_avail in frames %d, buffer_size %d\n", avail_nums,
            // uac_cfg->buffer_size);
            if (avail_nums < 0) {
                DOORLOCK_ERR("spk avail_nums %d\n", avail_nums);
                snd_pcm_prepare(uac_cfg->uac_handle);
            }
            // else if (avail_nums >= uac_cfg->buffer_size/2)
            else if (avail_nums >= count) {
                // count = avail_nums>count?count:avail_nums;
                read_len = snd_pcm_readi(uac_cfg->uac_handle, data_buf, count);
                // DOORLOCK_DBG("snd_pcm_readi read_len = %d (Frames)\n", read_len);
                if (read_len == -EAGAIN) {
                    // snd_pcm_wait(uac_cfg->uac_handle, 10);
                    // DOORLOCK_ERR("EGAIN read_len[%d], count[%d]\n", read_len, count);
                } else if (read_len == -EPIPE) {
                    // uac_xrun(uac_cfg->uac_handle);
                    // snd_pcm_prepare(uac_cfg->uac_handle);
                    DOORLOCK_ERR("EPIPE\n");
                } else if (read_len == -ESTRPIPE) {
                    // uac_suspend(uac_cfg->uac_handle);
                    DOORLOCK_ERR("ESTRPIPE\n");
                } else if (read_len < 0) {
                    DOORLOCK_ERR("error from readi: %s, %d\n", snd_strerror(read_len), read_len);
                    // uac_suspend(uac_cfg->uac_handle);
                }

                if (read_len < 0) {
                    // app read too slow
                    snd_pcm_recover(uac_cfg->uac_handle, read_len, 0);
                } else {
                    DOORLOCK_DBG("spk snd_pcm_avail in frames %d, buffer_size %d, read_len %d\n",
                                avail_nums, uac_cfg->buffer_size, read_len);
                }
            } else {
                snd_pcm_wait(uac_cfg->uac_handle, 10);
                // snd_pcm_recover(uac_cfg->uac_handle, read_len, 0);
            }
            // if (avail_nums == 0){
            //	snd_pcm_prepare(uac_cfg->uac_handle);
            //	snd_pcm_start(uac_cfg->uac_handle);
            // }
        } else {
            read_len = snd_pcm_readi(uac_cfg->uac_handle, data_buf, count);
            if (read_len < 0) {
                DOORLOCK_DBG("snd_pcm_readi failed: %s\n", snd_strerror(read_len));
                snd_pcm_prepare(uac_cfg->uac_handle);
                // snd_pcm_recover(uac_cfg->uac_handle, read_len, 0);
            }
        }
    } else {
        DOORLOCK_ERR("uac_dev_fd = %d, spk_is_streaming = %d\n", uac_dev_info->uac_dev_fd,
                    uac_dev_info->spk_is_streaming);
    }
    return read_len * uac_cfg->bytes_per_frame;
}

int uac_dev_transfer_audio_mic_frame(uac_dev_t *uac_dev_info, uint8_t *data_buf, uint32_t data_size)
{
    // DOORLOCK_DBG("mic_is_streaming = %d\n", uac_dev_info->mic_is_streaming);
    int avail_nums = 0;
    int count = 0;
    int write_len = 0;
    uac_alsa_config_t *uac_cfg = &uac_dev_info->mic_alsa_cfg;

    if (uac_dev_info->mic_frame_cnt == 0) {
        DOORLOCK_INFO("mic frame [%d], size = %d, us = %lld\n", uac_dev_info->mic_frame_cnt, data_size,
               get_cur_time_us());
    }
    if ((uac_cfg->uac_handle != NULL) && (uac_dev_info->mic_is_streaming != false)) {
        // DOORLOCK_DBG("try to write data_size = %d (Bytes)\n", data_size);
        count = data_size / uac_cfg->bytes_per_frame;
        if (uac_dev_info->mic_nonblock) {
            avail_nums = snd_pcm_avail(uac_cfg->uac_handle);
            DOORLOCK_DBG("mic snd_pcm_avail in frames %d, buffer_size %d\n", avail_nums,
                        uac_cfg->buffer_size);
            if (avail_nums < 0) {
                DOORLOCK_DBG("mic avail_nums %d\n", avail_nums);
                snd_pcm_prepare(uac_cfg->uac_handle);
            } else if (avail_nums >= uac_cfg->buffer_size / 2)
            // else if (avail_nums >= count)
            {
                write_len = snd_pcm_writei(uac_cfg->uac_handle, data_buf, count);
                if (write_len == -EAGAIN) {
                    // snd_pcm_wait(uac_cfg->uac_handle, 100);
                    DOORLOCK_ERR("EGAIN write_len[%d], count[%d]\n", write_len, count);
                } else if (write_len == -EPIPE) {
                    // uac_xrun(uac_cfg->uac_handle);
                    // snd_pcm_prepare(uac_cfg->uac_handle);
                    DOORLOCK_ERR("EPIPE\n");
                } else if (write_len == -ESTRPIPE) {
                    // uac_suspend(uac_cfg->uac_handle);
                    DOORLOCK_ERR("ESTRPIPE\n");
                } else if (write_len < 0) {
                    DOORLOCK_ERR("error from writei: %s\n", snd_strerror(write_len));
                    // uac_suspend(uac_cfg->uac_handle);
                }

                if (write_len < 0) {
                    // app write too slow
                    snd_pcm_recover(uac_cfg->uac_handle, write_len, 0);
                }
            } else {
                snd_pcm_wait(uac_cfg->uac_handle, 10);
            }
        } else {
            write_len = snd_pcm_writei(uac_cfg->uac_handle, data_buf, count);
            if (write_len < 0) {
                DOORLOCK_DBG("snd_pcm_writei failed: %s\n", snd_strerror(write_len));
                snd_pcm_prepare(uac_cfg->uac_handle);
                // snd_pcm_recover(uac_cfg->uac_handle, write_len, 0);
            }
        }

        uac_dev_info->mic_frame_cnt++;
    } else {
        DOORLOCK_ERR("uac_dev_fd = %d, mic_is_streaming = %d\n", uac_dev_info->uac_dev_fd,
                    uac_dev_info->mic_is_streaming);
    }

    return write_len * uac_cfg->bytes_per_frame;
}

static void *uac_dev_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    uac_dev_t *uac_dev_info = (uac_dev_t *)g_uac_dev_info;

#if !UAC_TEST
    if (uac_dev_info->dev_on_off) {
        uac_dev_info->dev_on_off(uac_dev_info, 1);
    }
    aw_mem_open();

    if (strlen(uac_dev_info->uac_devname) == 0) {
        strcpy(uac_dev_info->uac_devname, "UAC1Gadget");
    }

    if (uac_dev_info->mic) {
        DOORLOCK_INFO("MIC thread create\n");
        if (uac_dev_info->max_mic_frame_size == 0) {
            uac_dev_info->max_mic_frame_size = 1024;
        }
        pthread_create(&uac_dev_info->mic_thread, NULL, uac_dev_mic_thread,
                       (void *)uac_dev_info);
        pthread_setname_np(uac_dev_info->mic_thread, "uac_mic_thread");
    }
    if (uac_dev_info->spk) {
        DOORLOCK_INFO("SPK thread create\n");
        if (uac_dev_info->max_spk_frame_size == 0) {
            uac_dev_info->max_spk_frame_size = 1024;
        }
        pthread_create(&uac_dev_info->spk_thread, NULL, uac_dev_spk_thread,
                       (void *)uac_dev_info);
        pthread_setname_np(uac_dev_info->spk_thread, "uac_spk_thread");
    }

    while (!uac_dev_info->force_exit) {
        usleep(300 * 1000);
    }
#endif
    DOORLOCK_DBG("exit <===\n");
}

int uac_dev_init(uac_dev_t *uac_dev_info)
{
    DOORLOCK_DBG("enter ===>\n");
    g_uac_dev_info = uac_dev_info;

    uac_dev_info->force_exit = false;
    uac_dev_info->mic_is_streaming = false;
    uac_dev_info->mic_is_resource_ok = false;
    uac_dev_info->spk_is_streaming = false;
    uac_dev_info->spk_is_resource_ok = false;
    uac_dev_info->ctrl_interface_set_cur = 0;
    uac_dev_info->mic_frame_cnt = 0;
    uac_dev_info->spk_frame_cnt = 0;

    gen_set_uac_function_script(uac_dev_info);
    pthread_mutex_init(&uac_dev_info->uac_dev_lock, NULL);

#if UAC_CHECK_EVENT_SUPPORT
    uac_dev_info->fast_connect = true;
#endif

    // uac_dev_run(uac_dev_info);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int uac_dev_run(uac_dev_t *uac_dev_info)
{
    DOORLOCK_DBG("enter ===>\n");
    pthread_t uac_thread = 0;
    pthread_create(&uac_thread, NULL, uac_dev_thread, (void *)uac_dev_info);
    pthread_setname_np(uac_thread, "uac_dev_thread");
    DOORLOCK_DBG("exit <===\n");
}

int uac_dev_deinit(uac_dev_t *uac_dev_info)
{
    DOORLOCK_DBG("enter ===>\n");
    if (uac_dev_info->force_exit == false) {
        uac_dev_info->force_exit = true;
        if (uac_dev_info->mic) {
            pthread_join(uac_dev_info->mic_thread, NULL);
        }
        if (uac_dev_info->spk) {
            pthread_join(uac_dev_info->spk_thread, NULL);
        }
    }
    aw_mem_close();
    if (uac_dev_info->dev_on_off) {
        uac_dev_info->dev_on_off(uac_dev_info, 0);
    }

    uac_dev_info->uac_dev_fd = 0;
    pthread_mutex_destroy(&uac_dev_info->uac_dev_lock);
    g_uac_dev_info = NULL;

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int uac_dev_connect(uac_dev_t *uac_dev_info)
{
    DOORLOCK_DBG("enter ===>\n");
    system("/bin/setusbconfig uac1");
    // system("cat /sys/devices/platform/soc/usbc0/usb_device");
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int uac_dev_disconnect(uac_dev_t *uac_dev_info)
{
    DOORLOCK_DBG("enter ===>\n");
    system("/bin/setusbconfig none");
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int uac_dev_force_exit(uac_dev_t *uac_dev_info)
{
    DOORLOCK_DBG("enter ===>\n");
    if (uac_dev_info->force_exit == false) {
        uac_dev_info->force_exit = true;
        if (uac_dev_info->mic) {
            pthread_join(uac_dev_info->mic_thread, NULL);
        }
        if (uac_dev_info->spk) {
            pthread_join(uac_dev_info->spk_thread, NULL);
        }
    }

    // uac_dev_disconnect(uac_dev_info);
    // uac_dev_deinit(uac_dev_info);

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int uac_dev_fast_connect_mic(uac_dev_t *uac_dev_info)
{
    if (uac_dev_info->mic_resource_on_off && uac_dev_info->mic) {
        DOORLOCK_INFO("time to create UACDev Resource mic\n");
#if !UAC_TEST
        if (uac_dev_info->mic_frm_manager.frm_node_cnt == 0) {
            uac_dev_info->mic_frm_manager.frm_node_cnt = 2;
        }
        uac_dev_info->mic_frm_manager.frm_node_memsize = uac_dev_info->max_mic_frame_size;
        frm_manager_init(&uac_dev_info->mic_frm_manager);
        uac_dev_info->mic_resource_on_off(uac_dev_info, 1);
#else
        uac_dev_info->mic_is_resource_ok = 1;
#endif
    }
    return 0;
}

int uac_dev_fast_connect_spk(uac_dev_t *uac_dev_info)
{
    if (uac_dev_info->spk_resource_on_off && uac_dev_info->spk) {
        DOORLOCK_INFO("time to create UACDev Resource spk\n");
#if !UAC_TEST
        if (uac_dev_info->spk_frm_manager.frm_node_cnt == 0) {
            uac_dev_info->spk_frm_manager.frm_node_cnt = 2;
        }
        uac_dev_info->spk_frm_manager.frm_node_memsize = uac_dev_info->max_spk_frame_size;
        frm_manager_init(&uac_dev_info->spk_frm_manager);
        uac_dev_info->spk_resource_on_off(uac_dev_info, 1);
#else
        uac_dev_info->spk_is_resource_ok = 1;
#endif
    }
    return 0;
}
