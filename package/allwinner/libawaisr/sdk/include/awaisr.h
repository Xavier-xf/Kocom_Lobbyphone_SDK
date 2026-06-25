#ifndef __AWAISR__
#define __AWAISR__

#ifdef __cplusplus
    extern "C" {
#endif

/*
 * error code
*/
#define AWAISR_SUCCESS                       (0x00000000)
#define ERR_AWAISR_BUSY                      (0x00000001)
#define ERR_AWAISR_NO_MEM                    (0x00000002)
#define ERR_AWAISR_NULL_PTR                  (0x00000004)
#define ERR_AWAISR_SYS_NOTREADY              (0x00000008)
#define ERR_AWAISR_VIP_INIT_FAIL             (0x00000010)
#define ERR_AWAISR_ILLEGAL_PARAM             (0x00000011)
#define ERR_AWAISR_NOT_SUPPORT               (0x00000012)

typedef struct AwaisrConfigParam
{
    /**
    aisr crop width
    */
    int mCropWidth;
    /**
    aisr crop height
    */
    int mCropHeight;
    /**
    overlapping pixels for crop
    */
    int mPx;
    /**
    width of the input image
    */
    int mWidth;
    /**
    height of the input image
    */
    int mHeight;
    /**
    the multiple of the width overfraction
    */
    int mScaleX;
    /**
    the multiple of the height overfraction
    */
    int mScaleY;
    /**
    size of the npu private buffer
    */
    int mNpuBufSize;
} AwaisrConfigParam;

typedef struct AwaisrImgBufInfo
{
    /**
    store img data buffer physical address
    */
    unsigned int mImgPhyBuf[3];

    /**
    store img data buffer virtual address
    */
    unsigned char *mImgVirBuf[3];
} AwaisrImgBufInfo;

typedef struct AiSrCallbackInfo {
    /**
    the private data of the callback function
    */
    void *cookie;
    /**
    callback function pointer
    */
    int (*AwaisrFrameDone)(void *cookie, AwaisrImgBufInfo InputBufInfo, AwaisrImgBufInfo OutputBufInfo);
} AiSrCallbackInfo;

/**
    Open aisr and config params.

    @param configparam
    aisr config params, defined by struct AwaisrConfigParam.
    @return
    0: success
    others: fail, defined by error code
*/
int AwaisrOpen(AwaisrConfigParam configparam);

/**
    Close aisr.

    @param NULL
    @return
    0: success
    others: fail, defined by error code
*/
int AwaisrClose();

/**
    Pass input image data to aisr.

    @param pInputBufInfo
    input yuv buffer info.
    @param pOutputBufInfo
    output yuv buffer info.
    @return
    0: success
    others: fail, defined by error code
*/
int AwaisrSendFrame(AwaisrImgBufInfo nInputBufInfo, AwaisrImgBufInfo nOutputBufInfo);

/**
    Register aisr finish callback.

    @param CallbackInfo
    include the private data and callback functions for registrants.
    @return
    0: success
    others: fail, defined by error code
*/
int AwaisrRegisterCallback(AiSrCallbackInfo CallbackInfo);

/**
    Convert the physical address to the vip buffer of the npu.

    @param pInputBufInfo
    input yuv buffer info.
    @param pOutputBufInfo
    output yuv buffer info.
    @return
    0: success
    others: fail, defined by error code
*/
int AwaisrVipBufferCreate(AwaisrImgBufInfo *pInputBufInfo, AwaisrImgBufInfo *pOutputBufInfo);

/**
    Destroy all vip buffer of the npu.

    @param NULL
    @return
    0: success
    others: fail, defined by error code
*/
int AwaisrVipBufferDestroyAll();

#ifdef __cplusplus
    }
#endif

#endif //#define __AWAISR__
