

#ifndef PSEUDOFLOAT_H
#define PSEUDOFLOAT_H

#include <math.h>
#include "arch.h"
#include "os_support.h"
#include "math_approx.h"

typedef struct {
   spx_int16_t m;
   spx_int16_t e;
} spx_float_t;

static const spx_float_t FLOAT_ZERO = {0,0};
static const spx_float_t FLOAT_ONE = {16384,-14};
static const spx_float_t FLOAT_HALF = {16384,-15};
static const spx_float_t FLOAT_MIN = {28147,-56};

#define MIN(a,b) ((a)<(b)?(a):(b))
static inline spx_float_t PSEUDOFLOAT(spx_int32_t x)
{
   int e=0;
   int sign=0;
   if (x<0)
   {
      sign = 1;
      x = -x;
   }
   if (x==0)
   {
      spx_float_t r = {0,0};
      return r;
   }
   e = spx_ilog2(ABS32(x))-14;
   x = VSHR32(x, e);
   if (sign)
   {
      spx_float_t r;
      r.m = -x;
      r.e = e;
      return r;
   }
   else      
   {
      spx_float_t r;
      r.m = x;
      r.e = e;
      return r;
   }
}


static inline spx_float_t FLOAT_ADD(spx_float_t a, spx_float_t b)
{
   spx_float_t r;
   if (a.m==0)
      return b;
   else if (b.m==0)
      return a;
   if ((a).e > (b).e) 
   {
      r.m = ((a).m>>1) + ((b).m>>MIN(15,(a).e-(b).e+1));
      r.e = (a).e+1;
   }
   else 
   {
      r.m = ((b).m>>1) + ((a).m>>MIN(15,(b).e-(a).e+1));
      r.e = (b).e+1;
   }
   if (r.m>0)
   {
      if (r.m<16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   } else {
      if (r.m>-16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   }
   /*printf ("%f + %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;
}

static inline spx_float_t FLOAT_ADD_P(spx_float_t a, spx_float_t b)
{
   spx_float_t r;
   if (a.m==0)
      return b;
   else if (b.m==0)
      return a;
   if ((a).e > (b).e) 
   {
      r.m = ((a).m>>1) + ((b).m>>MIN(15,(a).e-(b).e+1));
      r.e = (a).e+1;
   }
   else 
   {
      r.m = ((b).m>>1) + ((a).m>>MIN(15,(b).e-(a).e+1));
      r.e = (b).e+1;
   }
   
   if (r.m<16384)
   {
	   r.m<<=1;
	   r.e-=1;
   }
  /*printf ("%f + %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;
}

static inline spx_float_t FLOAT_SUB(spx_float_t a, spx_float_t b)
{
   spx_float_t r;
   if (a.m==0)
      return b;
   else if (b.m==0)
      return a;
   if ((a).e > (b).e)
   {
      r.m = ((a).m>>1) - ((b).m>>MIN(15,(a).e-(b).e+1));
      r.e = (a).e+1;
   }
   else 
   {
      r.m = ((a).m>>MIN(15,(b).e-(a).e+1)) - ((b).m>>1);
      r.e = (b).e+1;
   }
   if (r.m>0)
   {
      if (r.m<16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   } else {
      if (r.m>-16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   }
   /*printf ("%f + %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;
}

static inline int FLOAT_LT(spx_float_t a, spx_float_t b)
{
   if (a.m==0)
      return b.m>0;
   else if (b.m==0)
      return a.m<0;   
   if ((a).e > (b).e)
      return ((a).m>>1) < ((b).m>>MIN(15,(a).e-(b).e+1));
   else 
      return ((b).m>>1) > ((a).m>>MIN(15,(b).e-(a).e+1));

}

static inline int FLOAT_GT(spx_float_t a, spx_float_t b)
{
   return FLOAT_LT(b,a);
}

static inline spx_float_t FLOAT_MULT(spx_float_t a, spx_float_t b)
{
   spx_float_t r;
   r.m = (spx_int16_t)((spx_int32_t)(a).m*(b).m>>15);
   r.e = (a).e+(b).e+15;
   if (r.m>0)
   {
      if (r.m<16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   } else {
      if (r.m>-16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   }
   /*printf ("%f * %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;   
}

// inputs are positive 
static inline spx_float_t FLOAT_MULT_P(spx_float_t a, spx_float_t b)
{
   spx_float_t r;
   r.m = (spx_int16_t)((spx_int32_t)(a).m*(b).m>>15);
   r.e = (a).e+(b).e+15;
   
   if (r.m<16384)
   {
	   r.m<<=1;
	   r.e-=1;
   }
   /*printf ("%f * %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;   
}

static inline spx_float_t FLOAT_MUL16(spx_float_t a, spx_word16_t b)
{
   spx_float_t r;
   r.m = (spx_int16_t)((spx_int32_t)(a).m*(b)>>15);
   r.e = (a).e;
   if (r.m>0)
   {
      if (r.m<16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   } else {
      if (r.m>-16384)
      {
         r.m<<=1;
         r.e-=1;
      }
   }
   /*printf ("%f * %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;   
}

static inline spx_float_t FLOAT_MUL16_P(spx_float_t a, spx_word16_t b)
{
   spx_float_t r;
   r.m = (spx_int16_t)((spx_int32_t)(a).m*(b)>>15);
   r.e = (a).e;
   
   if (r.m<16384)
   {
	   r.m<<=1;
	   r.e-=1;
   }
   /*printf ("%f * %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;   
}


static inline spx_float_t FLOAT_MULT_V(spx_float_t a, spx_float_t b)
{
	int e1, tmp32;
	spx_float_t r;

	tmp32 = (spx_int32_t)(a).m*(spx_int32_t)(b).m;
	e1 = spx_ilog2(ABS32(tmp32));
	tmp32 = VSHR32(tmp32, e1-14);

	r.m = (spx_int16_t)SATURATE16(tmp32,32767);
	r.e = (a).e+(b).e+(e1-14);


   /*printf ("%f * %f = %f\n", REALFLOAT(a), REALFLOAT(b), REALFLOAT(r));*/
   return r;   
}


static inline spx_float_t FLOAT_AMULT(spx_float_t a, spx_float_t b)
{
   spx_float_t r;
   r.m = (spx_int16_t)((spx_int32_t)(a).m*(b).m>>15);
   r.e = (a).e+(b).e+15;
   return r;   
}


static inline spx_float_t FLOAT_SHL(spx_float_t a, int b)
{
   //spx_float_t r;
   //r.m = a.m;
   a.e = a.e+b;
   return a;
}

static inline spx_int16_t FLOAT_EXTRACT16(spx_float_t a)
{
   if (a.e<0)
      return EXTRACT16((EXTEND32(a.m)+(EXTEND32(1)<<(-a.e-1)))>>-a.e);
   else
      return a.m<<a.e;
}

static inline spx_int32_t FLOAT_EXTRACT32(spx_float_t a)
{
   if (a.e<0)
      return (EXTEND32(a.m)+(EXTEND32(1)<<(-a.e-1)))>>-a.e;
   else
      return EXTEND32(a.m)<<a.e;
}

static inline spx_int32_t FLOAT_MUL32(spx_float_t a, spx_word32_t b)
{
   return VSHR32(MULT16_32_Q15(a.m, b),-a.e-15);
}

static inline spx_float_t FLOAT_MUL32U(spx_word32_t a, spx_word32_t b)
{
   int e1, e2;
   spx_float_t r;
   if (a==0 || b==0)
   {
      return FLOAT_ZERO;
   }
   e1 = spx_ilog2(ABS32(a));
   a = VSHR32(a, e1-14);
   e2 = spx_ilog2(ABS32(b));
   b = VSHR32(b, e2-14);
   //r.m = SATURATE16(MULT16_16_Q15(a,b),32767);
   r.m = MULT16_16_Q15(a,b);
   r.e = e1+e2-13;
   return r;
}

static inline spx_float_t FLOAT_MUL16U(spx_word16_t a, spx_word16_t b)
{
   int e1, e2;
   spx_float_t r;
   if (a==0 || b==0)
   {
      return FLOAT_ZERO;
   }
   e1 = spx_ilog2_int16(ABS16(a));
   a = SHL16(a, 14-e1);
   e2 = spx_ilog2_int16(ABS16(b));
   b = SHL16(b, 14-e2);
   //r.m = SATURATE16(MULT16_16_Q15(a,b),32767);
   r.m = MULT16_16_Q15(a,b);
   r.e = e1+e2-13;
   return r;
}

static inline spx_float_t FLOAT_POWER16U(spx_word16_t a)
{
   int e1;
   spx_float_t r;
   if (a==0)
   {
      return FLOAT_ZERO;
   }
   e1 = spx_ilog2_int16(ABS16(a));
   //a = VSHR32(a, e1-14); 
   a = SHL16(a, 14-e1);
   r.m = MULT16_16_Q15(a,a);
   r.e = e1 + e1 -13;
   return r;
}

/* Do NOT attempt to divide by a negative number */
static inline spx_float_t FLOAT_DIV32_FLOAT(spx_word32_t a, spx_float_t b)
{
   int e=0;
   spx_float_t r;
   if (a==0)
   {
      return FLOAT_ZERO;
   }
   e = spx_ilog2(ABS32(a))-spx_ilog2_int16(b.m-1)-15;
   a = VSHR32(a, e);
   if (ABS32(a)>=SHL32(EXTEND32(b.m-1),15))
   {
      a >>= 1;
      e++;
   }
   r.m = DIV32_16_P(a,b.m);
   r.e = e-b.e;
   return r;
}


/* Do NOT attempt to divide by a negative number */
static inline spx_float_t FLOAT_DIV32(spx_word32_t a, spx_word32_t b)
{
   int e0=0,e=0;
   spx_float_t r;
   if (a==0)
   {
      return FLOAT_ZERO;
   }
   if (b>32767)
   {
      e0 = spx_ilog2(b)-14;
      b = VSHR32(b, e0);
      e0 = -e0;
   }
   e = spx_ilog2(ABS32(a))-spx_ilog2(b-1)-15;
   a = VSHR32(a, e);
   if (ABS32(a)>=SHL32(EXTEND32(b-1),15))
   {
      a >>= 1;
      e++;
   }
   e += e0;
   r.m = DIV32_16_P(a,b);
   r.e = e;
   return r;
}

/* Do NOT attempt to divide by a negative number */
static inline spx_float_t FLOAT_DIVU(spx_float_t a, spx_float_t b)
{
   int e=0;
   spx_int32_t num;
   spx_float_t r;
   if (b.m<=0)
   {
      speex_warning_int("Attempted to divide by", b.m);
      return FLOAT_ONE;
   }
   num = a.m;
   a.m = ABS16(a.m);
   while (a.m >= b.m)
   {
      e++;
      a.m >>= 1;
   }
   num = num << (15-e);
   r.m = DIV32_16(num,b.m);
   r.e = a.e-b.e-15+e;
   return r;
}

/* Do NOT attempt to divide by a negative number */
static inline spx_float_t FLOAT_DIVU_P(spx_float_t a, spx_float_t b)
{
   int e=0;
   spx_int32_t num;
   spx_float_t r;
   if (b.m<=0)
   {
      speex_warning_int("Attempted to divide by", b.m);
      return FLOAT_ONE;
   }
   num = a.m;
   a.m = ABS16(a.m);

   while (a.m >= b.m)
   {
      e++;
      a.m >>= 1;
   }
   num <<= (15-e);
   r.m = DIV32_16_P(num,b.m);
   r.e = a.e-b.e-15+e;
   return r;
}

static inline spx_word16_t FLOAT_DIV_16(spx_float_t a, spx_float_t b)
{
   int e=0;
//   spx_word16_t c;
   spx_int32_t num;
   spx_float_t r;
   if (b.m<=0)
   {
      speex_warning_int("Attempted to divide by", b.m);
      return 0;
   }
   num = a.m;
   a.m = ABS16(a.m);

   while (a.m >= b.m)
   {
      e++;
      a.m >>= 1;
   }
   num = num << (15-e);
   r.m = DIV32_16_P(num,b.m);
   r.e = a.e-b.e+e;
//   r.e = ((r.e)>15 ? 15 : (r.e));
//   c = (spx_word16_t)MIN16(FLOAT_EXTRACT32(r), 32767);
   return ((spx_word16_t)MIN16(FLOAT_EXTRACT32(r), 32767));
}

static inline spx_float_t FLOAT_SQRT(spx_float_t a)
{
   spx_float_t r;
   spx_int32_t m;
   m = SHL32(EXTEND32(a.m), 14);
   r.e = a.e - 14;
   if (r.e & 1)
   {
      r.e -= 1;
      m <<= 1;
   }
   r.e >>= 1;
   r.m = spx_sqrt(m);
   return r;
}



#endif
