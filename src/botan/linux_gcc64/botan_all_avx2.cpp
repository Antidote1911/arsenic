/*
* Botan 2.13.0 Amalgamation
* (C) 1999-2018 The Botan Authors
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include "botan_all.h"
#include "botan_all_internal.h"

#include <immintrin.h>

namespace Botan {

class SIMD_8x32 final
   {
   public:

      SIMD_8x32& operator=(const SIMD_8x32& other) = default;
      SIMD_8x32(const SIMD_8x32& other) = default;

      SIMD_8x32& operator=(SIMD_8x32&& other) = default;
      SIMD_8x32(SIMD_8x32&& other) = default;

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32()
         {
         m_avx2 = _mm256_setzero_si256();
         }

      BOTAN_FUNC_ISA("avx2")
      explicit SIMD_8x32(const uint32_t B[8])
         {
         m_avx2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(B));
         }

      BOTAN_FUNC_ISA("avx2")
      explicit SIMD_8x32(uint32_t B0, uint32_t B1, uint32_t B2, uint32_t B3,
                         uint32_t B4, uint32_t B5, uint32_t B6, uint32_t B7)
         {
         m_avx2 = _mm256_set_epi32(B7, B6, B5, B4, B3, B2, B1, B0);
         }

      BOTAN_FUNC_ISA("avx2")
      static SIMD_8x32 splat(uint32_t B)
         {
         return SIMD_8x32(_mm256_set1_epi32(B));
         }

      BOTAN_FUNC_ISA("avx2")
      static SIMD_8x32 load_le(const uint8_t* in)
         {
         return SIMD_8x32(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(in)));
         }

      BOTAN_FUNC_ISA("avx2")
      static SIMD_8x32 load_be(const uint8_t* in)
         {
         return load_le(in).bswap();
         }

      BOTAN_FUNC_ISA("avx2")
      void store_le(uint8_t out[]) const
         {
         _mm256_storeu_si256(reinterpret_cast<__m256i*>(out), m_avx2);
         }

      BOTAN_FUNC_ISA("avx2")
      void store_be(uint8_t out[]) const
         {
         bswap().store_le(out);
         }

      template<size_t ROT>
      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 rotl() const
         {
         static_assert(ROT > 0 && ROT < 32, "Invalid rotation constant");

         BOTAN_IF_CONSTEXPR(ROT == 8)
            {
            const __m256i shuf_rotl_8 = _mm256_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3,
                                                        14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);

            return SIMD_8x32(_mm256_shuffle_epi8(m_avx2, shuf_rotl_8));
            }
         else BOTAN_IF_CONSTEXPR(ROT == 16)
            {
            const __m256i shuf_rotl_16 = _mm256_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2,
                                                         13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);

            return SIMD_8x32(_mm256_shuffle_epi8(m_avx2, shuf_rotl_16));
            }
         else
            {
            return SIMD_8x32(_mm256_or_si256(_mm256_slli_epi32(m_avx2, static_cast<int>(ROT)),
                                             _mm256_srli_epi32(m_avx2, static_cast<int>(32-ROT))));
            }
         }

      template<size_t ROT>
      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 rotr() const
         {
         return this->rotl<32-ROT>();
         }

      template<size_t ROT1, size_t ROT2, size_t ROT3>
      SIMD_8x32 rho() const
         {
         SIMD_8x32 res;

         const SIMD_8x32 rot1 = this->rotr<ROT1>();
         const SIMD_8x32 rot2 = this->rotr<ROT2>();
         const SIMD_8x32 rot3 = this->rotr<ROT3>();

         return rot1 ^ rot2 ^ rot3;
         }

      SIMD_8x32 operator+(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval += other;
         return retval;
         }

      SIMD_8x32 operator-(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval -= other;
         return retval;
         }

      SIMD_8x32 operator^(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval ^= other;
         return retval;
         }

      SIMD_8x32 operator|(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval |= other;
         return retval;
         }

      SIMD_8x32 operator&(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval &= other;
         return retval;
         }

      BOTAN_FUNC_ISA("avx2")
      void operator+=(const SIMD_8x32& other)
         {
         m_avx2 = _mm256_add_epi32(m_avx2, other.m_avx2);
         }

      BOTAN_FUNC_ISA("avx2")
      void operator-=(const SIMD_8x32& other)
         {
         m_avx2 = _mm256_sub_epi32(m_avx2, other.m_avx2);
         }

      BOTAN_FUNC_ISA("avx2")
      void operator^=(const SIMD_8x32& other)
         {
         m_avx2 = _mm256_xor_si256(m_avx2, other.m_avx2);
         }

      BOTAN_FUNC_ISA("avx2")
      void operator|=(const SIMD_8x32& other)
         {
         m_avx2 = _mm256_or_si256(m_avx2, other.m_avx2);
         }

      BOTAN_FUNC_ISA("avx2")
      void operator&=(const SIMD_8x32& other)
         {
         m_avx2 = _mm256_and_si256(m_avx2, other.m_avx2);
         }

      template<int SHIFT> BOTAN_FUNC_ISA("avx2") SIMD_8x32 shl() const
         {
         return SIMD_8x32(_mm256_slli_epi32(m_avx2, SHIFT));
         }

      template<int SHIFT> BOTAN_FUNC_ISA("avx2")SIMD_8x32 shr() const
         {
         return SIMD_8x32(_mm256_srli_epi32(m_avx2, SHIFT));
         }

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 operator~() const
         {
         return SIMD_8x32(_mm256_xor_si256(m_avx2, _mm256_set1_epi32(0xFFFFFFFF)));
         }

      // (~reg) & other
      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 andc(const SIMD_8x32& other) const
         {
         return SIMD_8x32(_mm256_andnot_si256(m_avx2, other.m_avx2));
         }

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 bswap() const
         {
         const uint8_t BSWAP_MASK[32] = { 3, 2, 1, 0,
                                          7, 6, 5, 4,
                                          11, 10, 9, 8,
                                          15, 14, 13, 12,
                                          19, 18, 17, 16,
                                          23, 22, 21, 20,
                                          27, 26, 25, 24,
                                          31, 30, 29, 28 };

         const __m256i bswap = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(BSWAP_MASK));

         const __m256i output = _mm256_shuffle_epi8(m_avx2, bswap);

         return SIMD_8x32(output);
         }

      BOTAN_FUNC_ISA("avx2")
      static void transpose(SIMD_8x32& B0, SIMD_8x32& B1,
                            SIMD_8x32& B2, SIMD_8x32& B3)
         {
         const __m256i T0 = _mm256_unpacklo_epi32(B0.m_avx2, B1.m_avx2);
         const __m256i T1 = _mm256_unpacklo_epi32(B2.m_avx2, B3.m_avx2);
         const __m256i T2 = _mm256_unpackhi_epi32(B0.m_avx2, B1.m_avx2);
         const __m256i T3 = _mm256_unpackhi_epi32(B2.m_avx2, B3.m_avx2);

         B0.m_avx2 = _mm256_unpacklo_epi64(T0, T1);
         B1.m_avx2 = _mm256_unpackhi_epi64(T0, T1);
         B2.m_avx2 = _mm256_unpacklo_epi64(T2, T3);
         B3.m_avx2 = _mm256_unpackhi_epi64(T2, T3);
         }

      BOTAN_FUNC_ISA("avx2")
      static void transpose(SIMD_8x32& B0, SIMD_8x32& B1,
                            SIMD_8x32& B2, SIMD_8x32& B3,
                            SIMD_8x32& B4, SIMD_8x32& B5,
                            SIMD_8x32& B6, SIMD_8x32& B7)
         {
         transpose(B0, B1, B2, B3);
         transpose(B4, B5, B6, B7);

         swap_tops(B0, B4);
         swap_tops(B1, B5);
         swap_tops(B2, B6);
         swap_tops(B3, B7);
         }

      BOTAN_FUNC_ISA("avx2")
      static void reset_registers()
         {
         _mm256_zeroupper();
         }

      BOTAN_FUNC_ISA("avx2")
      static void zero_registers()
         {
         _mm256_zeroall();
         }

      __m256i BOTAN_FUNC_ISA("avx2") handle() const { return m_avx2; }

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32(__m256i x) : m_avx2(x) {}

   private:

      BOTAN_FUNC_ISA("avx2")
      static void swap_tops(SIMD_8x32& A, SIMD_8x32& B)
         {
         SIMD_8x32 T0 = _mm256_permute2x128_si256(A.handle(), B.handle(), 0 + (2 << 4));
         SIMD_8x32 T1 = _mm256_permute2x128_si256(A.handle(), B.handle(), 1 + (3 << 4));
         A = T0;
         B = T1;
         }

      __m256i m_avx2;
   };

}
#if defined(__GNUG__) && !defined(__clang__)
#pragma GCC target ("avx2")
#endif
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {


#define key_xor(round, B0, B1, B2, B3)                             \
   do {                                                            \
      B0 ^= SIMD_8x32::splat(m_round_key[4*round  ]);              \
      B1 ^= SIMD_8x32::splat(m_round_key[4*round+1]);              \
      B2 ^= SIMD_8x32::splat(m_round_key[4*round+2]);              \
      B3 ^= SIMD_8x32::splat(m_round_key[4*round+3]);              \
   } while(0)

/*
* Serpent's linear transformations
*/
#define transform(B0, B1, B2, B3)                                  \
   do {                                                            \
      B0 = B0.rotl<13>();                                          \
      B2 = B2.rotl<3>();                                           \
      B1 ^= B0 ^ B2;                                               \
      B3 ^= B2 ^ B0.shl<3>();                                      \
      B1 = B1.rotl<1>();                                           \
      B3 = B3.rotl<7>();                                           \
      B0 ^= B1 ^ B3;                                               \
      B2 ^= B3 ^ B1.shl<7>();                                      \
      B0 = B0.rotl<5>();                                           \
      B2 = B2.rotl<22>();                                          \
   } while(0)

#define i_transform(B0, B1, B2, B3)                                \
   do {                                                            \
      B2 = B2.rotr<22>();                                          \
      B0 = B0.rotr<5>();                                           \
      B2 ^= B3 ^ B1.shl<7>();                                      \
      B0 ^= B1 ^ B3;                                               \
      B3 = B3.rotr<7>();                                           \
      B1 = B1.rotr<1>();                                           \
      B3 ^= B2 ^ B0.shl<3>();                                      \
      B1 ^= B0 ^ B2;                                               \
      B2 = B2.rotr<3>();                                           \
      B0 = B0.rotr<13>();                                          \
   } while(0)

BOTAN_FUNC_ISA("avx2")
void Serpent::avx2_encrypt_8(const uint8_t in[128], uint8_t out[128]) const
   {
   SIMD_8x32::reset_registers();

   SIMD_8x32 B0 = SIMD_8x32::load_le(in);
   SIMD_8x32 B1 = SIMD_8x32::load_le(in + 32);
   SIMD_8x32 B2 = SIMD_8x32::load_le(in + 64);
   SIMD_8x32 B3 = SIMD_8x32::load_le(in + 96);

   SIMD_8x32::transpose(B0, B1, B2, B3);

   key_xor( 0,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 1,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 2,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 3,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 4,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 5,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 6,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 7,B0,B1,B2,B3); SBoxE8(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 8,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 9,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(10,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(11,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(12,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(13,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(14,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(15,B0,B1,B2,B3); SBoxE8(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(16,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(17,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(18,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(19,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(20,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(21,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(22,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(23,B0,B1,B2,B3); SBoxE8(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(24,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(25,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(26,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(27,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(28,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(29,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(30,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(31,B0,B1,B2,B3); SBoxE8(B0,B1,B2,B3); key_xor(32,B0,B1,B2,B3);

   SIMD_8x32::transpose(B0, B1, B2, B3);
   B0.store_le(out);
   B1.store_le(out + 32);
   B2.store_le(out + 64);
   B3.store_le(out + 96);

   SIMD_8x32::zero_registers();
   }

BOTAN_FUNC_ISA("avx2")
void Serpent::avx2_decrypt_8(const uint8_t in[128], uint8_t out[128]) const
   {
   SIMD_8x32::reset_registers();

   SIMD_8x32 B0 = SIMD_8x32::load_le(in);
   SIMD_8x32 B1 = SIMD_8x32::load_le(in + 32);
   SIMD_8x32 B2 = SIMD_8x32::load_le(in + 64);
   SIMD_8x32 B3 = SIMD_8x32::load_le(in + 96);

   SIMD_8x32::transpose(B0, B1, B2, B3);

   key_xor(32,B0,B1,B2,B3);  SBoxD8(B0,B1,B2,B3); key_xor(31,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor(30,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(29,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(28,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(27,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(26,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(25,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor(24,B0,B1,B2,B3);

   i_transform(B0,B1,B2,B3); SBoxD8(B0,B1,B2,B3); key_xor(23,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor(22,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(21,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(20,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(19,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(18,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(17,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor(16,B0,B1,B2,B3);

   i_transform(B0,B1,B2,B3); SBoxD8(B0,B1,B2,B3); key_xor(15,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor(14,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(13,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(12,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(11,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(10,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor( 9,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor( 8,B0,B1,B2,B3);

   i_transform(B0,B1,B2,B3); SBoxD8(B0,B1,B2,B3); key_xor( 7,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor( 6,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor( 5,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor( 4,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor( 3,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor( 2,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor( 1,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor( 0,B0,B1,B2,B3);

   SIMD_8x32::transpose(B0, B1, B2, B3);

   B0.store_le(out);
   B1.store_le(out + 32);
   B2.store_le(out + 64);
   B3.store_le(out + 96);

   SIMD_8x32::zero_registers();
   }

#undef key_xor
#undef transform
#undef i_transform

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

inline
void SHACAL2_Fwd(const SIMD_8x32& A, const SIMD_8x32& B, const SIMD_8x32& C, SIMD_8x32& D,
                 const SIMD_8x32& E, const SIMD_8x32& F, const SIMD_8x32& G, SIMD_8x32& H,
                 uint32_t RK)
   {
   H += E.rho<6,11,25>() + ((E & F) ^ (~E & G)) + SIMD_8x32::splat(RK);
   D += H;
   H += A.rho<2,13,22>() + ((A & B) | ((A | B) & C));
   }

inline
void SHACAL2_Rev(const SIMD_8x32& A, const SIMD_8x32& B, const SIMD_8x32& C, SIMD_8x32& D,
                 const SIMD_8x32& E, const SIMD_8x32& F, const SIMD_8x32& G, SIMD_8x32& H,
                 uint32_t RK)
   {
   H -= A.rho<2,13,22>() + ((A & B) | ((A | B) & C));
   D -= H;
   H -= E.rho<6,11,25>() + ((E & F) ^ (~E & G)) + SIMD_8x32::splat(RK);
   }

}

void SHACAL2::avx2_encrypt_8(const uint8_t in[], uint8_t out[]) const
   {
   SIMD_8x32::reset_registers();

   SIMD_8x32 A = SIMD_8x32::load_be(in);
   SIMD_8x32 B = SIMD_8x32::load_be(in+32);
   SIMD_8x32 C = SIMD_8x32::load_be(in+64);
   SIMD_8x32 D = SIMD_8x32::load_be(in+96);

   SIMD_8x32 E = SIMD_8x32::load_be(in+128);
   SIMD_8x32 F = SIMD_8x32::load_be(in+160);
   SIMD_8x32 G = SIMD_8x32::load_be(in+192);
   SIMD_8x32 H = SIMD_8x32::load_be(in+224);

   SIMD_8x32::transpose(A, B, C, D, E, F, G, H);

   for(size_t r = 0; r != 64; r += 8)
      {
      SHACAL2_Fwd(A, B, C, D, E, F, G, H, m_RK[r+0]);
      SHACAL2_Fwd(H, A, B, C, D, E, F, G, m_RK[r+1]);
      SHACAL2_Fwd(G, H, A, B, C, D, E, F, m_RK[r+2]);
      SHACAL2_Fwd(F, G, H, A, B, C, D, E, m_RK[r+3]);
      SHACAL2_Fwd(E, F, G, H, A, B, C, D, m_RK[r+4]);
      SHACAL2_Fwd(D, E, F, G, H, A, B, C, m_RK[r+5]);
      SHACAL2_Fwd(C, D, E, F, G, H, A, B, m_RK[r+6]);
      SHACAL2_Fwd(B, C, D, E, F, G, H, A, m_RK[r+7]);
      }

   SIMD_8x32::transpose(A, B, C, D, E, F, G, H);

   A.store_be(out);
   B.store_be(out+32);
   C.store_be(out+64);
   D.store_be(out+96);

   E.store_be(out+128);
   F.store_be(out+160);
   G.store_be(out+192);
   H.store_be(out+224);

   SIMD_8x32::zero_registers();
   }

void SHACAL2::avx2_decrypt_8(const uint8_t in[], uint8_t out[]) const
   {
   SIMD_8x32::reset_registers();

   SIMD_8x32 A = SIMD_8x32::load_be(in);
   SIMD_8x32 B = SIMD_8x32::load_be(in+32);
   SIMD_8x32 C = SIMD_8x32::load_be(in+64);
   SIMD_8x32 D = SIMD_8x32::load_be(in+96);

   SIMD_8x32 E = SIMD_8x32::load_be(in+128);
   SIMD_8x32 F = SIMD_8x32::load_be(in+160);
   SIMD_8x32 G = SIMD_8x32::load_be(in+192);
   SIMD_8x32 H = SIMD_8x32::load_be(in+224);

   SIMD_8x32::transpose(A, B, C, D, E, F, G, H);

   for(size_t r = 0; r != 64; r += 8)
      {
      SHACAL2_Rev(B, C, D, E, F, G, H, A, m_RK[63-r]);
      SHACAL2_Rev(C, D, E, F, G, H, A, B, m_RK[62-r]);
      SHACAL2_Rev(D, E, F, G, H, A, B, C, m_RK[61-r]);
      SHACAL2_Rev(E, F, G, H, A, B, C, D, m_RK[60-r]);
      SHACAL2_Rev(F, G, H, A, B, C, D, E, m_RK[59-r]);
      SHACAL2_Rev(G, H, A, B, C, D, E, F, m_RK[58-r]);
      SHACAL2_Rev(H, A, B, C, D, E, F, G, m_RK[57-r]);
      SHACAL2_Rev(A, B, C, D, E, F, G, H, m_RK[56-r]);
      }

   SIMD_8x32::transpose(A, B, C, D, E, F, G, H);

   A.store_be(out);
   B.store_be(out+32);
   C.store_be(out+64);
   D.store_be(out+96);

   E.store_be(out+128);
   F.store_be(out+160);
   G.store_be(out+192);
   H.store_be(out+224);

   SIMD_8x32::zero_registers();
   }

}
