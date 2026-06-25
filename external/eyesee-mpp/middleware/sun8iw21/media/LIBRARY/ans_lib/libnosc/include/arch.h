
#ifndef ARCH_H
#define ARCH_H

//#define FLOATING_POINT  //
//#define FIXED_POINT
//#if defined(WIN32)&&!defined(_cplusplus)
#define inline __inline
//#endif

#include "fixed_generic.h"

typedef short spx_int16_t;
typedef unsigned short spx_uint16_t;
typedef int spx_int32_t;
typedef unsigned int spx_uint32_t;
typedef long long spx_word64_t;

typedef spx_int16_t spx_word16_t;
typedef spx_int32_t spx_word32_t;
typedef spx_word32_t spx_mem_t;
typedef spx_word16_t spx_coef_t;
typedef spx_word16_t spx_lsp_t;
typedef spx_word32_t spx_sig_t;

// NOTE! czx: optimize almost the same?
#ifdef __ARM_ASM
#include "fixed_armv7.h"
#endif

#define ABS(x) ((x) < 0 ? (-(x)) : (x))      /**< Absolute integer value. */
#define ABS16(x) ((x) < 0 ? (-(x)) : (x))    /**< Absolute 16-bit value.  */
#define MIN16(a,b) ((a) < (b) ? (a) : (b))   /**< Maximum 16-bit value.   */
#define MAX16(a,b) ((a) > (b) ? (a) : (b))   /**< Maximum 16-bit value.   */
#define ABS32(x) ((x) < 0 ? (-(x)) : (x))    /**< Absolute 32-bit value.  */
#define MIN32(a,b) ((a) < (b) ? (a) : (b))   /**< Maximum 32-bit value.   */
#define MAX32(a,b) ((a) > (b) ? (a) : (b))   /**< Maximum 32-bit value.   */

#define MINU32(a,b) (((spx_uint32_t)(a)) < ((spx_uint32_t)(b)) ? ((spx_uint32_t)a) : ((spx_uint32_t)b))
#define MAXU32(a,b) (((spx_uint32_t)(a)) > ((spx_uint32_t)(b)) ? ((spx_uint32_t)a) : ((spx_uint32_t)b))

#endif
