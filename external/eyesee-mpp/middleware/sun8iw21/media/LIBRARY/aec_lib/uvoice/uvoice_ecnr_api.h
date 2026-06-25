
/**
 *  @internal
 *  @file ecnr.h
 *
 *  Prototypes of the Echo Cancellation and Noise Reduction (ECNR) API functions.
 *
 *  This header file contains all prototypes of the API functions of the ECNR module.
 */
/*======================================================================================*/
/** @addtogroup DEF
*  @{*/
#ifndef _UVOICEECNR_DEF_20220503_H_
#define _UVOICEECNR_DEF_20220503_H_


#ifdef __cplusplus
extern "C"
{
#endif


/*-----------------------------------------------------*/
/*   Error codes - success must ALWAYS equate to 0     */
/*-----------------------------------------------------*/
typedef enum uvoice_ecnr_status{
    UV_ECNR_GENER_ERROR = -1,
    UV_ECNR_UNINIT_ERROR = -2,
    UV_ECNR_NULLPTR_ERROR = -3,
    UV_ECNR_PARAMETER_ERROR = -4,
    UV_ECNR_VERIFY_ERROR = -5,
    UV_ECNR_PARAMETER_WARNING = 1,
    UV_ECNR_FILE_OPEN_FAILED = -6,
    UV_ECNR_OK = 0,
}uvoice_ecnr_status;

/*--------------------------------------------------------------------------------------*/
/*      Define identification strings                                                   */
/*--------------------------------------------------------------------------------------*/
//  typedef enum _UvEcnr_ParamterId {
//                                   uv_ecnr_mode_stereo=1,
//                                   uv_ecnr_mode_mono_phone,
//                                   uv_ecnr_mode_mono_recog,
//  } UvEcnr_ParamterId;

typedef enum _UvEcnr_InputMode_{
    uvoice_ecnr_mode_1mic1ref = 0,
    uvoice_ecnr_mode_1mic2ref,
    uvoice_ecnr_mode_2mic1ref,
    uvoice_ecnr_mode_2mic2ref,
    uvoice_ecnr_mode_3mic1ref,
    uvoice_ecnr_mode_3mic2ref,
    uvoice_ecnr_mode_4mic1ref,
    uvoice_ecnr_mode_4mic2ref,
    uvoice_ecnr_mode_1mic0ref,
    uvoice_ecnr_mode_2mic0ref,
}UvEcnr_InputMode;

typedef enum _UvEcnr_OutputMode_{
    uvoice_ecnr_mode_phone = 0,
    uvoice_ecnr_mode_asr,
}UvEcnr_OutputMode;


#define MAX_MICIN_NUM 8
#define MAX_REFIN_NUM 4

#define UVOICE_ECNR_FRAMELEN 160

typedef struct {
    short *audioin[MAX_MICIN_NUM];
    short *audioref[MAX_REFIN_NUM];
}uv_ecnr_audio_buf;

/*-----------------------------------------------------------------------------*/
/*       Function prototypes                                                   */
/*-----------------------------------------------------------------------------*/

/**
 * Get ECNR version.
 *
 * @return
 *      Returns a pointer to the version
 */
const char* UvEcnr_GetVersion(void);

/*
 * UvEcnr_Create 完成后，可调用该函数检查授权状态
 * return值：
 * 0：授权成功
 * 1：授权参数出错
 * 4：设备已经激活过
 * 5：不正确的授权码
 * 6：授权码使用次数超限
 * 7：授权码已被其他设备占用
 * 其他：其他错误
 */
int UvEcnr_GetAuthError();

/** Creates a new ECNR state
 * @param
 *      in_mod   -I : Audio mode of input audio signal (UvEcnr_InputMode)
 *      out_mode -I : ECNR process mode (for asr or phone, check UvEcnr_OutputMode define)
 *      sample_rate -I : Sample rate of input audio
 *      param    -I : A pointer of struct uv_activate_param, to check license, default is NULL
 * @return Returns a pointer TO Newly-created ECNR engine
 */
void* UvEcnr_Create(UvEcnr_InputMode in_mod, UvEcnr_OutputMode out_mode, int sample_rate, void* param);

/**
 *  Initializes an ECNR instance.
 *
 * @param
 *      uvoiceEcnr    -IO : Pointer to the ECNR instance
 * @return
 *      Returns error code
 */
uvoice_ecnr_status UvEcnr_Init(void* hdl);

/**
 * Run the ECNR process on one frame of data.
 *
 * @param
 *      hdl          -I : Pointer to the ECNR instance
 *      audio        -I : In buffer containing one frame of recording signals (for mic and reference audio),
 *                        each channel must be contain 160 short samples (or 320 bytes), 
			  arranged in block-wise fashion
 *      ppOut        -O : Pointer to the buffer containing one frame of ECNR processed signal 
			  (this buffer is allocated by the engine)
 * @return
 *      Returns error code
 */
uvoice_ecnr_status UvEcnr_Process(void* hdl,uv_ecnr_audio_buf *audio, signed short** ppOut);

/**
 * This function releases the memory allocated by ECNR state.
 *
 * @param
 *     hdl -I: Pointer to the ECNR instance
 */
void UvEcnr_Destroy(void* hdl);

/**
 * This function reset the internal ECNR state.
 *
 * @param
 *     hdl -I: Pointer to the ECNR instance
 */
uvoice_ecnr_status UvEcnr_Reset(void* hdl);


#ifdef __cplusplus
}
#endif

#endif
/**@}*/
