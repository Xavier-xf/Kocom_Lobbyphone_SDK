
#ifndef MATH_APPROX_H
#define MATH_APPROX_H

#include "arch.h"


static inline spx_int16_t spx_ilog2(spx_uint32_t x)
{
#ifdef __ARM_ASM
	__asm__ volatile (
		"cmp %1, #0 \n\t"
		"moveq %0, #1 \n\t"
		"clz %0, %1 \n\t"
		"rsb %0, %0, #31 \n\t"
		: "=&r"(x)
		: "0"(x)
	);
	return x;
#else
   int r=0;
   if (x>=(spx_int32_t)65536)
   {
      x >>= 16;
      r += 16;
   }
   if (x>=256)
   {
      x >>= 8;
      r += 8;
   }
   if (x>=16)
   {
      x >>= 4;
      r += 4;
   }
   if (x>=4)
   {
      x >>= 2;
      r += 2;
   }
   if (x>=2)
   {
      r += 1;
   }
   return r;
#endif
}

static inline spx_int16_t spx_ilog2_int16(spx_int16_t x)
{
#ifdef __ARM_ASM
	// NOTE, when x <= 0, return 0, so we fake x to be 1 when x le 0.
	__asm__ volatile (
		"cmp %1, #0 \n\t"
		"movle %0, #1 \n\t"
		"clz %0, %1 \n\t"
		"rsb %0, %0, #31 \n\t"
		: "=r"(x)
		: "0"(x)
	);
	return x;

#else

#if 1
   int r=0;   
   if (x>=256)
   {
      x >>= 8;
      r += 8;
   }
   if (x>=16)
   {
      x >>= 4;
      r += 4;
   }
   if (x>=4)
   {
      x >>= 2;
      r += 2;
   }
   if (x>=2)
   {
      r += 1;
   }
   return r;
#else

	float fx;
	unsigned long ix, exp;
	
	if (x <=0)
		return 0;

    fx = (float)x;
    ix = *(unsigned long*)&fx;
    exp = (ix >> 23) & 0xFF;
    return exp - 127;

#endif

#endif
}

static inline spx_int16_t spx_ilog4(spx_uint32_t x)
{
#ifdef __ARM_ASM
	__asm__ volatile (
		"cmp %1, #0 \n\t"
		"moveq %0, #1 \n\t"
		"clz %0, %1 \n\t"
		"rsb %0, %0, #31 \n\t"
		"asr %0, %0, #1 \n\t"
		: "=r"(x)
		: "0"(x)
	);
	return x;
	
#else

   int r=0;
   if (x>=(spx_int32_t)65536)
   {
      x >>= 16;
      r += 8;
   }
   if (x>=256)
   {
      x >>= 8;
      r += 4;
   }
   if (x>=16)
   {
      x >>= 4;
      r += 2;
   }
   if (x>=4)
   {
      r += 1;
   }
   return r;
#endif
}

/* count the leading zeros */
static inline spx_int16_t WebRtcSpl_NormU32(spx_uint32_t a) {

#ifdef __ARM_ASM
	__asm__ volatile (
		"movs %0, %1 \n\t"
		"clzne %0, %1 \n\t"
		: "=r"(a)
		: "0"(a)
	);
	return a;
#else
  int zeros;

  if (a == 0) return 0;

  if (!(0xFFFF0000 & a)) {
    zeros = 16;
  } else {
    zeros = 0;
  }
  if (!(0xFF000000 & (a << zeros))) zeros += 8;
  if (!(0xF0000000 & (a << zeros))) zeros += 4;
  if (!(0xC0000000 & (a << zeros))) zeros += 2;
  if (!(0x80000000 & (a << zeros))) zeros += 1;

  return zeros;
#endif
}

/* exp function, Input Q15, Output Q15 */
static int _exp(int value)
{
	static int table[6] = {32768, 16384, 5461, 1365, 273, 45};//Q15	
	int tmp;
	int sum = QCONST32(1,15);
	int i, shift,sign;
	if(value > 363408)
	{
		return 0x7fffffff;
	}
	else if (value < -363408)
	{
		return 1;
	}
	
	shift = 0;
	sign = 1;
	if(value < 0)
	{
		value = -value;
		sign = -1;
	}
	if(value > 16384)
	{
		while(!(value & 0x20000000))
		{
			value <<= 1;
			shift += 1;
		}
		value >>= 16;
		shift = 16 - shift;
		
	}
	
	value = sign*value;

	tmp = QCONST32(1,15);
	for(i = 0; i < 6; i++)
	{
		tmp = MULT32_32_Q15(tmp, value);
		sum = sum + MULT32_32_Q15(tmp, table[i]);
	}
	
	
	for(i = 0; i < shift; i++)
	{
		sum = MULT32_32_Q15(sum,sum); 
	}

	return sum;
}


/** Generate a pseudo-random number */
static inline spx_word16_t speex_rand(spx_word16_t std, spx_int32_t *seed)
{
   spx_word32_t res;
   *seed = 1664525 * *seed + 1013904223;
   res = MULT16_16(EXTRACT16(SHR32(*seed,16)),std);
   return EXTRACT16(PSHR32(SUB32(res, SHR32(res, 3)),14));
}

/* sqrt(x) ~= 0.22178 + 1.29227*x - 0.77070*x^2 + 0.25723*x^3 (for .25 < x < 1) */
/*#define C0 3634
#define C1 21173
#define C2 -12627
#define C3 4215*/

/* sqrt(x) ~= 0.22178 + 1.29227*x - 0.77070*x^2 + 0.25659*x^3 (for .25 < x < 1) */
#define C0 3634
#define C1 21173
#define C2 -12627
#define C3 4204

static inline spx_word16_t spx_sqrt(spx_word32_t x)
{
   int k;
   spx_word32_t rt;
   k = spx_ilog4(x)-6;
   x = VSHR32(x, (k<<1));
   rt = ADD16(C0, MULT16_16_Q14(x, ADD16(C1, MULT16_16_Q14(x, ADD16(C2, MULT16_16_Q14(x, (C3)))))));
   rt = VSHR32(rt,7-k);
   return rt;
}

/* log(x) ~= -2.18151 + 4.20592*x - 2.88938*x^2 + 0.86535*x^3 (for .5 < x < 1) */


#define A1 16469
#define A2 2242
#define A3 1486

static inline spx_word16_t spx_acos(spx_word16_t x)
{
   int s=0;
   spx_word16_t ret;
   spx_word16_t sq;
   if (x<0)
   {
      s=1;
      x = NEG16(x);
   }
   x = SUB16(16384,x);
   
   x = x >> 1;
   sq = MULT16_16_Q13(x, ADD16(A1, MULT16_16_Q13(x, ADD16(A2, MULT16_16_Q13(x, (A3))))));
   ret = spx_sqrt(SHL32(EXTEND32(sq),13));
   
   /*ret = spx_sqrt(67108864*(-1.6129e-04 + 2.0104e+00*f + 2.7373e-01*f*f + 1.8136e-01*f*f*f));*/
   if (s)
      ret = SUB16(25736,ret);
   return ret;
}


#define K1 8192
#define K2 -4096
#define K3 340
#define K4 -10

static inline spx_word16_t spx_cos(spx_word16_t x)
{
   spx_word16_t x2;

   if (x<12868)
   {
      x2 = MULT16_16_P13(x,x);
      return ADD32(K1, MULT16_16_P13(x2, ADD32(K2, MULT16_16_P13(x2, ADD32(K3, MULT16_16_P13(K4, x2))))));
   } else {
      x = SUB16(25736,x);
      x2 = MULT16_16_P13(x,x);
      return SUB32(-K1, MULT16_16_P13(x2, ADD32(K2, MULT16_16_P13(x2, ADD32(K3, MULT16_16_P13(K4, x2))))));
   }
}

#define L1 32767
#define L2 -7651
#define L3 8277
#define L4 -626

static inline spx_word16_t _spx_cos_pi_2(spx_word16_t x)
{
   spx_word16_t x2;
   
   x2 = MULT16_16_P15(x,x);
   return ADD16(1,MIN16(32766,ADD32(SUB16(L1,x2), MULT16_16_P15(x2, ADD32(L2, MULT16_16_P15(x2, ADD32(L3, MULT16_16_P15(L4, x2))))))));
}

static inline spx_word16_t spx_cos_norm(spx_word32_t x)
{
   x = x&0x0001ffff;
   if (x>SHL32(EXTEND32(1), 16))
      x = SUB32(SHL32(EXTEND32(1), 17),x);
   if (x&0x00007fff)
   {
      if (x<SHL32(EXTEND32(1), 15))
      {
         return _spx_cos_pi_2(EXTRACT16(x));
      } else {
         return NEG32(_spx_cos_pi_2(EXTRACT16(65536-x)));
      }
   } else {
      if (x&0x0000ffff)
         return 0;
      else if (x&0x0001ffff)
         return -32767;
      else
         return 32767;
   }
}

/*
 K0 = 1
 K1 = log(2)
 K2 = 3-4*log(2)
 K3 = 3*log(2) - 2
*/
#define D0 16384
#define D1 11356
#define D2 3726
#define D3 1301
/* Input in Q11 format, output in Q16 */
static inline spx_word32_t spx_exp2(spx_word16_t x)
{
   int integer;
   spx_word16_t frac;
   integer = SHR16(x,11);
   if (integer>14)
      return 0x7fffffff;
   else if (integer < -15)
      return 0;
   frac = SHL16(x-SHL16(integer,11),3);
   frac = ADD16(D0, MULT16_16_Q14(frac, ADD16(D1, MULT16_16_Q14(frac, ADD16(D2 , MULT16_16_Q14(D3,frac))))));
   return VSHR32(EXTEND32(frac), -integer-2);
}

/* Input in Q11 format, output in Q16 */
static inline spx_word32_t spx_exp(spx_word16_t x)
{
   if (x>21290)
      return 0x7fffffff;
   else if (x<-21290)
      return 0;
   else
      return spx_exp2(MULT16_16_P14(23637,x));
}
#define M1 32767
#define M2 -21
#define M3 -11943
#define M4 4936

static inline spx_word16_t spx_atan01(spx_word16_t x)
{
   return MULT16_16_P15(x, ADD32(M1, MULT16_16_P15(x, ADD32(M2, MULT16_16_P15(x, ADD32(M3, MULT16_16_P15(M4, x)))))));
}

#undef M1
#undef M2
#undef M3
#undef M4

/* Input in Q15, output in Q14 */
static inline spx_word16_t spx_atan(spx_word32_t x)
{
   if (x <= 32767)
   {
      return SHR16(spx_atan01(x),1);
   } else {
      int e = spx_ilog2(x);
      if (e>=29)
         return 25736;
      x = DIV32_16(SHL32(EXTEND32(32767),29-e), EXTRACT16(SHR32(x, e-14)));
      return SUB16(25736, SHR16(spx_atan01(x),1));
   }
}



#endif
