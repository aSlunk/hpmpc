/* ******************************************** *\
 *
 *
 *
\* ******************************************** */


/* Including headers */
#pragma once
#include <stdlib.h>
#include <x86intrin.h>
#include <stdint.h>
#include <wmmintrin.h>
#include "../config.h"
#ifndef AVX
#define AVX
#endif

#ifndef BITS_PER_REG
#define BITS_PER_REG 256
#endif

#ifdef __VAES__
#define MM_XOR _mm256_xor_si256
#define MM_AES_ENC _mm256_aesenc_epi128
#define MM_AES_DEC _mm256_aesdec_epi128
#define MM_AES_ENC_LAST _mm256_aesenclast_epi128
#define MM_AES_DEC_LAST _mm256_aesdeclast_epi128
#endif

/* Defining 0 and 1 */
#define ZERO _mm256_setzero_si256()
#define ONES _mm256_set1_epi32(-1)

/* Defining macros */
#define REG_SIZE 256
#define CHUNK_SIZE 1024

#define AND(a,b)  _mm256_and_si256(a,b)
#define OR(a,b)   _mm256_or_si256(a,b)
#define XOR(a,b)  _mm256_xor_si256(a,b)
#define ANDN(a,b) _mm256_andnot_si256(a,b)
#define NOT(a)    _mm256_xor_si256(ONES,a)

#define FUNC_AND  _mm256_and_si256_wrapper
#define FUNC_OR   _mm256_or_si256_wrapper
#define FUNC_XOR _mm256_xor_si256_wrapper
#define FUNC_ANDN _mm256_andnot_si256_wrapper
#define FUNC_NOT _mm256_xor_si256_wrapper
#define FUNC_ADD32  _mm256_add_epi32_wrapper
#define FUNC_SUB32 _mm256_sub_epi32_wrapper
#define FUNC_MUL32 _mm256_mullo_epi32_wrapper
#define FUNC_ADD64 _mm256_add_epi64_wrapper
#define FUNC_SUB64 _mm256_sub_epi64_wrapper
#define FUNC_MUL64 _mm256_mullo_epi64_wrapper

// wrapper functions needed for some compilers

inline __m256i _mm256_and_si256_wrapper(__m256i a, __m256i b) {
    return _mm256_and_si256(a, b);
}

inline __m256i _mm256_or_si256_wrapper(__m256i a, __m256i b) {
    return _mm256_or_si256(a, b);
}

inline __m256i _mm256_xor_si256_wrapper(__m256i a, __m256i b) {
    return _mm256_xor_si256(a, b);
}

inline __m256i _mm256_andnot_si256_wrapper(__m256i a, __m256i b) {
    return _mm256_andnot_si256(a, b);
}

inline __m256i _mm256_not_wrapper(__m256i a, __m256i b) {
    return _mm256_xor_si256(ONES, a);
}

inline __m256i _mm256_add_epi32_wrapper(__m256i a, __m256i b) {
    return _mm256_add_epi32(a, b);
}

inline __m256i _mm256_sub_epi32_wrapper(__m256i a, __m256i b) {
    return _mm256_sub_epi32(a, b);
}

inline __m256i _mm256_mullo_epi32_wrapper(__m256i a, __m256i b) {
    return _mm256_mullo_epi32(a, b);
}

inline __m256i _mm256_add_epi64_wrapper(__m256i a, __m256i b) {
    return _mm256_add_epi64(a, b);
}

inline __m256i _mm256_sub_epi64_wrapper(__m256i a, __m256i b) {
    return _mm256_sub_epi64(a, b);
}

inline __m256i _mm256_mullo_epi64_wrapper(__m256i a, __m256i b) {
    return _mm256_mullo_epi64(a, b);
}




#define MUL_SIGNED(a,b,c) _mm256_mullo_epi##c(a,b)
#define ADD_SIGNED(a,b,c) _mm256_add_epi##c(a,b)
#define SUB_SIGNED(a,b,c) _mm256_sub_epi##c(a,b)

/* #define ADD(a,b,c) _mm256_add_epi##c(a,b) */

#define L_SHIFT(a,b,c)  _mm256_slli_epi##c(a,b)
#define R_SHIFT(a,b,c)  _mm256_srli_epi##c(a,b)
#define RA_SHIFT(a,b,c) _mm256_sra_epi##c(a,_mm_set1_epi##c(b))

#define L_ROTATE(a,b,c)                                                 \
  b == 8 && c == 32 ?                                                   \
    _mm256_shuffle_epi8(a,_mm256_set_epi8(14,13,12,15,10,9,8,11,6,5,4,7,2,1,0,3, \
                                          14,13,12,15,10,9,8,11,6,5,4,7,2,1,0,3)) : \
    b == 16 && c == 32 ?                                                \
    _mm256_shuffle_epi8(a,_mm256_set_epi8(13,12,15,14,9,8,11,10,5,4,7,6,1,0,3,2, \
                                          13,12,15,14,9,8,11,10,5,4,7,6,1,0,3,2)) : \
    OR(L_SHIFT(a,b,c),R_SHIFT(a,c-b,c))

#define R_ROTATE(a,b,c)                                                 \
  b == 8 && c == 32 ?                                                   \
    _mm256_shuffle_epi8(a,_mm256_set_epi8(12,15,14,13,8,11,10,9,4,7,6,5,0,3,2,1, \
                                          12,15,14,13,8,11,10,9,4,7,6,5,0,3,2,1)) : \
    b == 16 && c == 32 ?                                                \
    _mm256_shuffle_epi8(a,_mm256_set_epi8(13,12,15,14,9,8,11,10,5,4,7,6,1,0,3,2, \
                                          13,12,15,14,9,8,11,10,5,4,7,6,1,0,3,2)) : \
    OR(R_SHIFT(a,b,c),L_SHIFT(a,c-b,c))


#define LIFT_8(x)  _mm256_set1_epi8(x)
#define LIFT_16(x) _mm256_set1_epi16(x)
#define LIFT_32(x) _mm256_set1_epi32(x)
#define LIFT_64(x) _mm256_set1_epi64x(x)


#define BITMASK(x,n,c) _mm256_sub_epi##c(ZERO, __mm256_and_si256(_mm256_slli_epi##c(x,n), _mm256_set1_epi##c(1)))

#define PACK_8x2_to_16(a,b)  /* TODO: implement with shuffles */
#define PACK_16x2_to_32(a,b) /* TODO: implement with shuffles */
#define PACK_32x2_to_64(a,b) /* TODO: implement with shuffles */


#define DATATYPE __m256i

#define SET_ALL_ONE()  ONES
#define SET_ALL_ZERO() ZERO

/* Note the reverse of the pattern. */
#define PERMUT_16(a,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15,x16) \
  _mm256_shuffle_epi8(a,_mm256_set_epi8(x16,x15,x14,x13,x12,x11,x10,x9,x8,x7,x6,x5,x4,x3,x2,x1,\
                                        x16,x15,x14,x13,x12,x11,x10,x9,x8,x7,x6,x5,x4,x3,x2,x1))
#define PERMUT_4(a,x1,x2,x3,x4) _mm256_shuffle_epi32(a,(x4<<6)|(x3<<4)|(x2<<2)|x1)


#define ORTHOGONALIZE(in,out) orthogonalize(in,out)
#define UNORTHOGONALIZE(in,out) unorthogonalize(in,out)

#define ALLOC(size) aligned_alloc(32,size * sizeof(__m256i))
#define NEW(var) new (std::align_val_t(sizeof(__m256i))) var;



/* Orthogonalization stuffs */
static uint64_t mask_l[6] = {
	0xaaaaaaaaaaaaaaaaUL,
	0xccccccccccccccccUL,
	0xf0f0f0f0f0f0f0f0UL,
	0xff00ff00ff00ff00UL,
	0xffff0000ffff0000UL,
	0xffffffff00000000UL
};

static uint64_t mask_r[6] = {
	0x5555555555555555UL,
	0x3333333333333333UL,
	0x0f0f0f0f0f0f0f0fUL,
	0x00ff00ff00ff00ffUL,
	0x0000ffff0000ffffUL,
	0x00000000ffffffffUL
};


void real_ortho(UINT_TYPE data[]) {
  for (int i = 0; i < LOG2_BITLENGTH; i ++) {
    int nu = (1UL << i);
    for (int j = 0; j < BITLENGTH; j += (2 * nu))
      for (int k = 0; k < nu; k ++) {
        UINT_TYPE u = data[j + k] & mask_l[i];
        UINT_TYPE v = data[j + k] & mask_r[i];
        UINT_TYPE x = data[j + nu + k] & mask_l[i];
        UINT_TYPE y = data[j + nu + k] & mask_r[i];
        data[j + k] = u | (x >> nu);
        data[j + nu + k] = (v << nu) | y;
      }
  }
}

void real_ortho_256x256(__m256i data[]) {

  __m256i mask_l[8] = {
    _mm256_set1_epi64x(0xaaaaaaaaaaaaaaaaUL),
    _mm256_set1_epi64x(0xccccccccccccccccUL),
    _mm256_set1_epi64x(0xf0f0f0f0f0f0f0f0UL),
    _mm256_set1_epi64x(0xff00ff00ff00ff00UL),
    _mm256_set1_epi64x(0xffff0000ffff0000UL),
    _mm256_set1_epi64x(0xffffffff00000000UL),
    _mm256_set_epi64x(0UL,-1UL,0UL,-1UL),
    _mm256_set_epi64x(0UL,0UL,-1UL,-1UL),

  };

  __m256i mask_r[8] = {
    _mm256_set1_epi64x(0x5555555555555555UL),
    _mm256_set1_epi64x(0x3333333333333333UL),
    _mm256_set1_epi64x(0x0f0f0f0f0f0f0f0fUL),
    _mm256_set1_epi64x(0x00ff00ff00ff00ffUL),
    _mm256_set1_epi64x(0x0000ffff0000ffffUL),
    _mm256_set1_epi64x(0x00000000ffffffffUL),
    _mm256_set_epi64x(-1UL,0UL,-1UL,0UL),
    _mm256_set_epi64x(-1UL,-1UL,0UL,0UL),
  };

  for (int i = 0; i < 8; i ++) {
    int nu = (1UL << i);
    for (int j = 0; j < 256; j += (2 * nu))
      for (int k = 0; k < nu; k ++) {
        __m256i u = _mm256_and_si256(data[j + k], mask_l[i]);
        __m256i v = _mm256_and_si256(data[j + k], mask_r[i]);
        __m256i x = _mm256_and_si256(data[j + nu + k], mask_l[i]);
        __m256i y = _mm256_and_si256(data[j + nu + k], mask_r[i]);
        if (i <= 5) {
          data[j + k] = _mm256_or_si256(u, _mm256_srli_epi64(x, nu));
          data[j + nu + k] = _mm256_or_si256(_mm256_slli_epi64(v, nu), y);
        } else if (i == 6) {
          /* Note the "inversion" of srli and slli. */
          data[j + k] = _mm256_or_si256(u, _mm256_slli_si256(x, 8));
          data[j + nu + k] = _mm256_or_si256(_mm256_srli_si256(v, 8), y);
        } else {
          data[j + k] = _mm256_or_si256(u, _mm256_permute2x128_si256( x , x , 1));
          data[j + nu + k] = _mm256_or_si256(_mm256_permute2x128_si256( v , v , 1), y);
        }
      }
  }
}

void orthogonalize_boolean(UINT_TYPE* data, __m256i* out) {
  for (int i = 0; i < DATTYPE; i += BITLENGTH)
    real_ortho(&(data[i]));

  for (int i = 0; i < BITLENGTH; i++) {
    #if BITLENGTH == 64
      out[i] = _mm256_set_epi64x(data[192+i], data[128+i], data[64+i], data[i]);
    #elif BITLENGTH == 32
      out[i] = _mm256_set_epi32(data[224+i], data[192+i], data[160+i], data[128+i], data[96+i], data[64+i], data[32+i], data[i]);
    #elif BITLENGTH == 16
      out[i] = _mm256_set_epi16(data[240+i], data[224+i], data[208+i], data[192+i], data[176+i], data[160+i], data[144+i], data[128+i], data[112+i], data[96+i], data[80+i], data[64+i], data[48+i], data[32+i], data[16+i], data[i]);
    #endif
  }
}

void unorthogonalize_boolean(__m256i *in, UINT_TYPE* data) {
  for (int i = 0; i < BITLENGTH; i++) {
    alignas(32) UINT_TYPE tmp[DATTYPE/BITLENGTH];
    _mm256_store_si256 ((__m256i*)tmp, in[i]);
    for (int j = 0; j < DATTYPE/BITLENGTH; j++)
      data[i+j*BITLENGTH] = tmp[j];
  }

  for (int i = 0; i < DATTYPE; i += BITLENGTH)
    real_ortho(&(data[i]));
}

void orthogonalize_arithmetic_full(UINT_TYPE *in, __m256i *out) {
  for (int i = 0; i < 256; i++)
#if BITLENGTH == 64
    out[i] = _mm256_set_epi64x(in[i*4+3], in[i*4+2], in[i*4+1], in[i*4]);
#elif BITLENGTH == 32
    out[i] = _mm256_set_epi32(in[i*8+7], in[i*8+6], in[i*8+5], in[i*8+4],
                              in[i*8+3], in[i*8+2], in[i*8+1], in[i*8]);
#elif BITLENGTH == 16
    out[i] = _mm256_set_epi16(in[i*16+15], in[i*16+14], in[i*16+13], in[i*16+12],
                              in[i*16+11], in[i*16+10], in[i*16+9], in[i*16+8],
                              in[i*16+7], in[i*16+6], in[i*16+5], in[i*16+4],
                              in[i*16+3], in[i*16+2], in[i*16+1], in[i*16]);
#endif
}

void unorthogonalize_arithmetic_full(__m256i *in, UINT_TYPE *out) {
  for (int i = 0; i < 256; i++)
    _mm256_store_si256 ((__m256i*)&(out[i*(DATTYPE/BITLENGTH)]), in[i]);
}

