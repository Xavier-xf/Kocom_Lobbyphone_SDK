#ifndef __UAC_H__
#define __UAC_H__

#include "../../utils/option/include/option.h"

#ifdef __plusplus
extern "C"{
#endif

#define SOUND_CARD_MIC                  "Capture1Mic"
#define SOUND_CARD_PLAYBACKRATEDMIX     "hw:audiocodec"
#define SOUND_CARD_CAPTURE1MICPLUSAEC   "Capture1MicPlusAec"
#define SOUND_CARD_UAC                  "hw:UAC1Gadget"
#define SOUND_MIXER_AUDIOCODEC          "hw:audiocodec"
#define SOUND_MIXER_SNDDAUDIO0          "hw:snddaudio0"
#define SOUND_CARD_SNDDAUDIO0           "I2SRTX"
#define DEFAULT_POINT_NUM_PERFRAME      160
#define DEFAULT_AGC_FLOAT_TARGETDB      0
#define DEFAULT_AGC_FLOAT_MAXGAINDB     30
#define DEFAULT_AGC_SAMPLE_LEN          1024
#define DEFAULT_ANS_MODE                1
#define DEFAULT_PLAYBACK_VOLUME         80  //value range [0, 100]

int uac_enable(uvc_demo_config *config);
int uac_disable(void);

#ifdef __plusplus
}
#endif

#endif
