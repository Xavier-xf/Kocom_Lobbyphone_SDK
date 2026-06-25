#define  SUBFRAME 10
 
enum
{
    kAgcModeUnchanged,
    kAgcModeAdaptiveAnalog,
    kAgcModeAdaptiveDigital,
    kAgcModeFixedDigital
};

/*
 * This function creates an AGC instance, which will contain the state
 * information for one (duplex) channel.
 *
 * Return value             : AGC instance if successful
 *                          : 0 (i.e., a NULL pointer) if unsuccessful
 */
int WebRtcAgc_Create(void **agcInst);

/*
 * This function frees the AGC instance created at the beginning.
 *
 * Input:
 *      - agcInst           : AGC instance.
 *
 * Return value             :  0 - Ok
 *                            -1 - Error
 */
int WebRtcAgc_Free(void *agcInst);

/*
 * This function initializes an AGC instance.
 *
 * Input:
 *      - agcInst           : AGC instance.
 *      - minLevel          : Minimum possible mic level
 *      - maxLevel          : Maximum possible mic level
 *      - agcMode           : 0 - Unchanged
 *                          : 1 - Adaptive Analog Automatic Gain Control -3dBOv
 *                          : 2 - Adaptive Digital Automatic Gain Control -3dBOv
 *                          : 3 - Fixed Digital Gain 0dB
 *      - fs                : Sampling frequency
 *
 * Return value             :  0 - Ok
 *                            -1 - Error
 */
int WebRtcAgc_Init(void *agcInst,
                   spx_word32_t minLevel,
                   spx_word32_t maxLevel,
                   spx_word16_t agcMode,
                   spx_uint32_t fs);
/*
 * This function processes a 10/20ms frame and adjusts (normalizes) the gain
 * both analog and digitally. The gain adjustments are done only during
 * active periods of speech. The input speech length can be either 10ms or
 * 20ms and the output is of the same length. The length of the speech
 * vectors must be given in samples (80/160 when FS=8000, and 160/320 when
 * FS=16000 or FS=32000). The echo parameter can be used to ensure the AGC will
 * not adjust upward in the presence of echo.
 *
 * This function should be called after processing the near-end microphone
 * signal, in any case after any echo cancellation.
 *
 * Input:
 *      - agcInst           : AGC instance
 *      - inNear            : Near-end input speech vector (10 or 20 ms) for
 *                            L band
 *      - inNear_H          : Near-end input speech vector (10 or 20 ms) for
 *                            H band
 *      - samples           : Number of samples in input/output vector
 *      - inMicLevel        : Current microphone volume level
 *      - echo              : Set to 0 if the signal passed to add_mic is
 *                            almost certainly free of echo; otherwise set
 *                            to 1. If you have no information regarding echo
 *                            set to 0.
 *
 * Output:
 *      - outMicLevel       : Adjusted microphone volume level
 *      - out               : Gain-adjusted near-end speech vector (L band)
 *                          : May be the same vector as the input.
 *      - out_H             : Gain-adjusted near-end speech vector (H band)
 *      - saturationWarning : A returned value of 1 indicates a saturation event
 *                            has occurred and the volume cannot be further
 *                            reduced. Otherwise will be set to 0.
 *
 * Return value:
 *                          :  0 - Normal operation.
 *                          : -1 - Error
 */
int WebRtcAgc_Process(void* agcInst,
                      const spx_word16_t* inNear,
                      const spx_word16_t* inNear_H,
                      spx_word16_t samples,
                      spx_word16_t* out,
                      spx_word16_t* out_H,
                      spx_word32_t inMicLevel,
                      spx_word32_t* outMicLevel,
                      spx_word16_t echo,
                      unsigned char* saturationWarning);