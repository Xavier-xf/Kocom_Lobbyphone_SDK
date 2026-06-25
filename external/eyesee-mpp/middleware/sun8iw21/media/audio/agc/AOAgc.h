
#ifndef _AOAGC_H_
#define _AOAGC_H_

#include <stdbool.h>

#include <mm_comm_aio.h>
#include <alsa_interface.h>

typedef struct AOAgc
{
    int mSampleRate;
    int mChnNum;
    int mBitWidth;
    /**
      process input pcm data, AOAgc outputs all processed pcm data. So if pFrm->mpAddr is not enough, AOAgc will realloc
      a new buffer to replace pFrm->mpAddr, and free old buffer.

      @param bDrainFlag
        true: get all processed data and left input pcm data.
        false: get processed data only.

      @return
        0: success, even process zero pcm data, if no exception occur, we think is success.
        -1: fail
    */
    int (*mpAOAgcProcess)(struct AOAgc *pThiz, AUDIO_FRAME_S *pFrm, bool bDrainFlag);

    /**
      clear all inner data, restore to original state.

      @return
        0: success
        -1: fail
    */
    int (*mpAOAgcClearData)(struct AOAgc *pThiz);
    /**
      get agclib inner input pcm data.

      @return
        0: get data info success
        -1: get data info fail.
    */
    int (*mpAOAgcGetInputPcmData)(struct AOAgc *pThiz, char **ppInputPcmData, int *pInputPcmDataLen);
    /**
      update config param.

      @return
        0: success
        -1: fail.
    */
    int (*mpAOAgcUpdateConfig)(struct AOAgc *pThiz, AGC_FLOAT_CONFIG_S *pAgcConfig);
    /**
      delete instance of AOAgc or its derived class.
    */
    void (*mpAOAgcDelete)(struct AOAgc *pThiz);
} AOAgc;

#endif  /* _AOAGC_H_ */

