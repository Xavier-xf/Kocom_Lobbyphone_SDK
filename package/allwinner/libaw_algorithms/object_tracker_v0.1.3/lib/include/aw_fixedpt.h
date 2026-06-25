#ifndef _AW_FIXEDPT_H
#define _AW_FIXEDPT_H

#include "aw_macro.h"
#include <stdint.h>
#include <errno.h>
#include <stddef.h>
#include <math.h>

//interface
#ifdef __cplusplus
extern "C" {
#endif

#ifdef FIXEDPT

	//#ifdef FIXEDPT
#define fixpt_16 short int
#define fixpt_32 int

#define ufixpt_16 unsigned short int
#define FIX_P 15
#define FIX_Q 10
#define FIX_SQRT_SHIFT 10  // should be even number

static const fixpt_16 fix16_maximum = 0x7FFF; /*!< the maximum value of fix8_t */
static const fixpt_16 fix16_minimum = 0x8000; /*!< the minimum value of fix8_t */
static const fixpt_16 fix16_overflow = 0x8000; /*!< the value used to indicate overflows when FIXMATH_NO_OVERFLOW is not specified */
												   /* convert to and from floating point */
#define INT_TOFIX(a, q)  ((fixpt_16) ( (a) >> (q) ))  // Not work currently
#define FLT_TOFIX(a, q)  ((fixpt_16) ( (a)*(float) (1<<(q)) ))
#define FLT_TOFIX32(a, q)   ((fixpt_32) ( (a)*(float) (1<<(q)) ))
#define DOUBLE_TOFIX(a, q)  ((fixpt_16) ( (a)*(float) (1<<(q)) ))
#define FIX_TOFLT(a, q)  (  (float)(a) / (float)(1<<(q)) )
#define FIXADD(a, b)     ((a) + (b))
#define FIXSUB(a, b)     ((a) - (b))
#define FIXMUL(a, b, q)  ((int)((a)*(b))>>(q))
#define FIXDIV(a, b, q)  (((a)<<(q))/(b))
#define LONGFIXMUL(a, b)    ((a)*(b))

#define _QLN_E    2.71828182845904523536
#define QLN_E	  DOUBLE_TOFIX(_QLN_E)
#define FIXABS(x) (((x) < 0) ? (-x) : (x))

	static inline fixpt_16 int_2fix(int a) {
		fixpt_16 b;
		if (a > fix16_maximum)
			b = fix16_maximum;
		else
			b = (fixpt_16)a;
		return b;
	}

	static inline fixpt_16  inv_sqrt(fixpt_16 a) {
		int sqt = pow((a << FIX_SQRT_SHIFT) + 1, 0.5);
		int inv_sqt = FIXDIV(1, sqt, FIX_P);
		return inv_sqt >> (FIX_Q - (FIX_P - FIX_SQRT_SHIFT / 2));
	}

	static inline void float_array_to_fixedpt(float *in, fixpt_16 *out, int n) {
		int i;
		for (i = 0; i < n; i++) {
			out[i] = FLT_TOFIX(in[i], FIX_Q);
		}
	}

	static inline void float_array_to_fixedpt32(float *in, fixpt_32 *out, int n) {
		int i;
		for (i = 0; i < n; i++) {
			out[i] = FLT_TOFIX32(in[i], FIX_Q);
		}
	}

	static inline void float_array_tofixedpt_trans(float *in, fixpt_16 *out, int n, int trans) {
		int i;
		for (i = 0; i < n; i++) {
			out[i] = FLT_TOFIX(in[i] + trans, FIX_Q);
		}
	}

	fixpt_16 logfix(ufixpt_16 x, fixpt_16 precision);

	//#endif // FIXEDPT

#endif

#ifdef __cplusplus
}
#endif

#endif // !FIXEDPT_H