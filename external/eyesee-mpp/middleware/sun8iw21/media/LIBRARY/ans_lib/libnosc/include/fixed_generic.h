
#ifndef FIXED_GENERIC_H
#define FIXED_GENERIC_H

#define QCONST16(x,bits) ((spx_word16_t)(.5+(x)*(((spx_word32_t)1)<<(bits))))
#define QCONST32(x,bits) ((spx_word32_t)(.5+(x)*(((spx_word32_t)1)<<(bits))))
#define QCONST64(x,bits) ((spx_word64_t)(.5+(x)*(((spx_word64_t)1)<<(bits))))

#define MAX_32		(int)0x7fffffffL
#define NEG16(x) (-(x))
#define NEG32(x) (-(x))
#define EXTRACT16(x) ((spx_word16_t)(x))
#define EXTEND32(x) ((spx_word32_t)(x))
#define SHR16(a,shift) ((a) >> (shift))
#define SHL16(a,shift) ((a) << (shift))
#define SH16(a, shift) (((shift)>=0) ? ((a) << (shift)) : ((a) >> (-(shift))))
#define SHLU16(a,shift) (((spx_uint16_t)(a))<<(shift))
#define SHRU16(a,shift) (((spx_uint16_t)(a))>>(shift))
#define SHR32(a,shift) ((a) >> (shift))
#define SHL32(a,shift) ((a) << (shift))
#define SH32(a,shift) (((shift)>=0) ? ((a) << (shift)) : ((a) >> (-(shift))))
#define SHU32(a,shift) (((shift)>=0) ? (((spx_uint32_t)(a)) << (shift)) : (((spx_uint32_t)(a)) >> (-(shift))))
#define SHRU32(a,shift) (((spx_uint32_t)(a)) >> (shift))
#define SHLU32(a,shift) (((spx_uint32_t)(a)) << (shift))
#define PSHR16(a,shift) (SHR16((a)+((1<<((shift))>>1)),shift))
#define PSHR32(a,shift) (SHR32((a)+((EXTEND32(1)<<((shift))>>1)),shift))
#define VSHR32(a, shift) (((shift)>0) ? SHR32(a, shift) : SHL32(a, -(shift)))
#define SATURATE16(x,a) (((x)>(a) ? (a) : (x)<-(a) ? -(a) : (x)))
#define SATURATE32(x,a) (((x)>(a) ? (a) : (x)<-(a) ? -(a) : (x)))

#define SHR(a,shift) ((a) >> (shift))
#define SHL(a,shift) ((spx_word32_t)(a) << (shift))
#define PSHR(a,shift) (SHR((a)+((EXTEND32(1)<<((shift))>>1)),shift))
#define SATURATE(x,a) (((x)>(a) ? (a) : (x)<-(a) ? -(a) : (x)))


#define ADD16(a,b) ((spx_word16_t)((spx_word16_t)(a)+(spx_word16_t)(b)))
#define SUB16(a,b) ((spx_word16_t)(a)-(spx_word16_t)(b))
#define ADD32(a,b) ((spx_word32_t)(a)+(spx_word32_t)(b))
#define SUB32(a,b) ((spx_word32_t)(a)-(spx_word32_t)(b))


/* result fits in 16 bits */

#define MULT16_U32_Q15(a, b)	(spx_word32_t)(((spx_word32_t)((spx_word16_t)(a))*(spx_word64_t)((spx_uint32_t)(b))) >> 15)
#define MULTU16_U32_Q15(a, b)	(spx_uint32_t)(((spx_uint32_t)((spx_uint16_t)(a))*(spx_word64_t)((spx_uint32_t)(b))) >> 15)	
#define MULTU32_U32_Q15(a,b)	(spx_word32_t)(((spx_word64_t)((spx_uint32_t)(a))*(spx_word64_t)((spx_uint32_t)(b))) >> 15)
#define MULTU32_U32_Q16(a,b)	(spx_word32_t)(((spx_word64_t)((spx_uint32_t)(a))*(spx_word64_t)((spx_uint32_t)(b))) >> 16)
#define MULTU32_U32_QS(a,b,shift)	(spx_uint32_t)(((spx_word64_t)((spx_uint32_t)(a))*(spx_word64_t)((spx_uint32_t)(b))) >> shift)
#define MULTU32_U32(a,b)	(spx_word64_t)((spx_word64_t)((spx_uint32_t)(a))*(spx_word64_t)((spx_uint32_t)(b)))
#define MULTU32_32_Q15(a,b)		(spx_word32_t)(((spx_word64_t)((spx_uint32_t)(a))*(spx_word64_t)((spx_word32_t)(b))) >> 15)
#define MULT16_16_16(a,b)     ((((spx_word16_t)(a))*((spx_word16_t)(b))))
#define MULT32_32_32(a, b)		  ((spx_word32_t)((spx_word32_t)(a)*(spx_word32_t)(b)))
#define MULT32_32(a, b)		  ((spx_word32_t)((spx_word32_t)(a)*(spx_word32_t)(b)))
#define MULTU32_U32_U32(a,b)  ((spx_uint32_t)((spx_uint32_t)(a)*(spx_uint32_t)(b)))
#define MULT32_32_Q16(a, b)	  (spx_word32_t)(((spx_word64_t)(a)*(spx_word64_t)(b)) >> 16)	
#define MULT32_32_Q14(a, b)	  (spx_word32_t)(((spx_word64_t)(a)*(spx_word64_t)(b)) >> 14)
#define MULT32_32_Q15(a, b)	  (spx_word32_t)(((spx_word64_t)((spx_word32_t)(a))*(spx_word64_t)((spx_word32_t)(b))) >> 15)
#define MULT32_U32_Q14(a, b)	  (spx_word32_t)(((spx_word64_t)(a)*(spx_uint32_t)(b)) >> 14)	

#define MULT32_16(a, b)		((spx_int32_t)(((spx_int32_t)(a))*((spx_int16_t)(b))))
/* (spx_word32_t)(spx_word16_t) gives TI compiler a hint that it's 16x16->32 multiply */
#define MULT16_16(a,b)     (spx_word32_t)((((spx_word32_t)(spx_word16_t)(a))*((spx_word32_t)(spx_word16_t)(b))))
#define MULT16_U16(a, b)	((spx_int32_t)(((spx_int32_t)(spx_int16_t)(a))*((spx_uint32_t)(spx_uint16_t)(b))))
#define MULTU16_U16(a, b)	((spx_uint32_t)(((spx_uint32_t)(spx_uint16_t)(a))*((spx_uint32_t)(spx_uint16_t)(b))))
#define MULTU16_U16_Q15(a, b)	((spx_uint32_t)(((spx_uint32_t)(spx_uint16_t)(a))*((spx_uint32_t)(spx_uint16_t)(b))) >> 15)
#define MULTU32_U16(a, b)	((spx_uint32_t)(((spx_uint32_t)(a))*((spx_uint32_t)(spx_uint16_t)(b))))

#define MAC16_16(c,a,b) (ADD32((c),MULT16_16((a),(b))))
#define MULT16_32_Q12(a,b) ADD32(MULT16_16((a),SHR((b),12)), SHR(MULT16_16((a),((b)&0x00000fff)),12))
#define MULT16_32_Q13(a,b) ADD32(MULT16_16((a),SHR((b),13)), SHR(MULT16_16((a),((b)&0x00001fff)),13))
#define MULT16_32_Q14(a,b) ADD32(MULT16_16((a),SHR((b),14)), SHR(MULT16_16((a),((b)&0x00003fff)),14))

#define MULT16_32_Q11(a,b) ADD32(MULT16_16((a),SHR((b),11)), SHR(MULT16_16((a),((b)&0x000007ff)),11))
#define MAC16_32_Q11(c,a,b) ADD32(c,ADD32(MULT16_16((a),SHR((b),11)), SHR(MULT16_16((a),((b)&0x000007ff)),11)))

#define MULT16_32_P15(a,b) ADD32(MULT16_16((a),SHR((b),15)), PSHR(MULT16_16((a),((b)&0x00007fff)),15))
#define MULT16_32_Q15(a,b) ADD32(MULT16_16((a),SHR((b),15)), SHR(MULT16_16((a),((b)&0x00007fff)),15))
#define MAC16_32_Q15(c,a,b) ADD32(c,ADD32(MULT16_16((a),SHR((b),15)), SHR(MULT16_16((a),((b)&0x00007fff)),15)))
#define MAC16_32_Q16(c,a,b) ADD32(c,ADD32(MULT16_16((a),SHR((b),16)), SHR(MULT16_U16((a),((b)&0x0000ffff)),16)))
#define MACU16_32_Q16(c,a,b) ADD32(c,ADD32(MULT16_U16(SHR((b),16),(a)), SHR(MULTU16_U16((a),(spx_uint16_t)((b)&0x0000ffff)),16)))

#define MAC16_16_Q11(c,a,b)     (ADD32((c),SHR(MULT16_16((a),(b)),11)))
#define MAC16_16_Q13(c,a,b)     (ADD32((c),SHR(MULT16_16((a),(b)),13)))
#define MAC16_16_P13(c,a,b)     (ADD32((c),SHR(ADD32(4096,MULT16_16((a),(b))),13)))

#define MULT16_16_Q11_32(a,b) (SHR(MULT16_16((a),(b)),11))
#define MULT16_16_Q13(a,b) (SHR(MULT16_16((a),(b)),13))
#define MULT16_16_Q14(a,b) (SHR(MULT16_16((a),(b)),14))
#define MULT16_16_Q15(a,b) (SHR(MULT16_16((a),(b)),15))
#define MULT16_16_Q7(a,b)	(SHR(MULT16_16((a),(b)),7))

#define MULT16_16_P13(a,b) (SHR(ADD32(4096,MULT16_16((a),(b))),13))
#define MULT16_16_P14(a,b) (SHR(ADD32(8192,MULT16_16((a),(b))),14))
#define MULT16_16_P15(a,b) (SHR(ADD32(16384,MULT16_16((a),(b))),15))

#define MUL_16_32_R15(a,bh,bl) ADD32(MULT16_16((a),(bh)), SHR(MULT16_16((a),(bl)),15))

#define DIV32_16(a,b) ((spx_word16_t)(((spx_word32_t)(a))/((spx_word16_t)(b))))
#define DIV32_16_P(a,b) ((spx_word16_t)(((spx_uint32_t)(a))/((spx_uint32_t)(b))))
#define PDIV32_16(a,b) ((spx_word16_t)(((spx_word32_t)(a)+((spx_word16_t)(b)>>1))/((spx_word16_t)(b))))
#define DIV32(a,b) (((spx_word32_t)(a))/((spx_word32_t)(b)))
#define DIV32_P(a,b) (((spx_uint32_t)(a))/((spx_uint32_t)(b)))
#define DIV64(a,b) (((spx_word64_t)(a))/((spx_word32_t)(b)))
#define DIV64_P(a,b) (((spx_word64_t)(a))/((spx_uint32_t)(b)))
#define DIV64_64(a,b) (((spx_word64_t)(a))/((spx_word64_t)(b)))
#define PDIV32(a,b) (((spx_word32_t)(a)+((spx_word16_t)(b)>>1))/((spx_word32_t)(b)))



#define DIV32_16_Z(a, b) (((b) == 0) ? (0x7fff) : DIV32_16(a,b))
#define DIV32_16_R32(a,b) (((b) == 0) ? (0x7fffffff) : ((spx_word32_t)(((spx_word32_t)(a))/((spx_word16_t)(b)))))

#define MULT32_32_QV(a, b, Q)	MULT32_32_Q##Q(a, b)
#endif
