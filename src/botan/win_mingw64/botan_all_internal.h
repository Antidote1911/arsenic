/*
* Botan 2.13.0 Amalgamation
* (C) 1999-2018 The Botan Authors
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_AMALGAMATION_INTERNAL_H_
#define BOTAN_AMALGAMATION_INTERNAL_H_

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>


namespace Botan {

/**
* If top bit of arg is set, return ~0. Otherwise return 0.
*/
template<typename T>
inline T expand_top_bit(T a)
   {
   return static_cast<T>(0) - (a >> (sizeof(T)*8-1));
   }

/**
* If arg is zero, return ~0. Otherwise return 0
*/
template<typename T>
inline T ct_is_zero(T x)
   {
   return expand_top_bit<T>(~x & (x - 1));
   }

/**
* Power of 2 test. T should be an unsigned integer type
* @param arg an integer value
* @return true iff arg is 2^n for some n > 0
*/
template<typename T>
inline constexpr bool is_power_of_2(T arg)
   {
   return (arg != 0) && (arg != 1) && ((arg & static_cast<T>(arg-1)) == 0);
   }

/**
* Return the index of the highest set bit
* T is an unsigned integer type
* @param n an integer value
* @return index of the highest set bit in n
*/
template<typename T>
inline size_t high_bit(T n)
   {
   size_t hb = 0;

   for(size_t s = 8*sizeof(T) / 2; s > 0; s /= 2)
      {
      const size_t z = s * ((~ct_is_zero(n >> s)) & 1);
      hb += z;
      n >>= z;
      }

   hb += n;

   return hb;
   }

/**
* Return the number of significant bytes in n
* @param n an integer value
* @return number of significant bytes in n
*/
template<typename T>
inline size_t significant_bytes(T n)
   {
   size_t b = 0;

   for(size_t s = 8*sizeof(n) / 2; s >= 8; s /= 2)
      {
      const size_t z = s * (~ct_is_zero(n >> s) & 1);
      b += z/8;
      n >>= z;
      }

   b += (n != 0);

   return b;
   }

/**
* Count the trailing zero bits in n
* @param n an integer value
* @return maximum x st 2^x divides n
*/
template<typename T>
inline size_t ctz(T n)
   {
   /*
   * If n == 0 then this function will compute 8*sizeof(T)-1, so
   * initialize lb to 1 if n == 0 to produce the expected result.
   */
   size_t lb = ct_is_zero(n) & 1;

   for(size_t s = 8*sizeof(T) / 2; s > 0; s /= 2)
      {
      const T mask = (static_cast<T>(1) << s) - 1;
      const size_t z = s * (ct_is_zero(n & mask) & 1);
      lb += z;
      n >>= z;
      }

   return lb;
   }

template<typename T>
uint8_t ceil_log2(T x)
   {
   static_assert(sizeof(T) < 32, "Abnormally large scalar");

   if(x >> (sizeof(T)*8-1))
      return sizeof(T)*8;

   uint8_t result = 0;
   T compare = 1;

   while(compare < x)
      {
      compare <<= 1;
      result++;
      }

   return result;
   }

// Potentially variable time ctz used for OCB
inline size_t var_ctz32(uint32_t n)
   {
#if defined(BOTAN_BUILD_COMPILER_IS_GCC) || defined(BOTAN_BUILD_COMPILER_IS_CLANG)
   if(n == 0)
      return 32;
   return __builtin_ctz(n);
#else
   return ctz<uint32_t>(n);
#endif
   }

}

namespace Botan {

/**
* Perform encoding using the base provided
* @param base object giving access to the encodings specifications
* @param output an array of at least base.encode_max_output bytes
* @param input is some binary data
* @param input_length length of input in bytes
* @param input_consumed is an output parameter which says how many
*        bytes of input were actually consumed. If less than
*        input_length, then the range input[consumed:length]
*        should be passed in later along with more input.
* @param final_inputs true iff this is the last input, in which case
         padding chars will be applied if needed
* @return number of bytes written to output
*/
template <class Base>
size_t base_encode(Base&& base,
                   char output[],
                   const uint8_t input[],
                   size_t input_length,
                   size_t& input_consumed,
                   bool final_inputs)
   {
   input_consumed = 0;

   const size_t encoding_bytes_in = base.encoding_bytes_in();
   const size_t encoding_bytes_out = base.encoding_bytes_out();

   size_t input_remaining = input_length;
   size_t output_produced = 0;

   while(input_remaining >= encoding_bytes_in)
      {
      base.encode(output + output_produced, input + input_consumed);

      input_consumed += encoding_bytes_in;
      output_produced += encoding_bytes_out;
      input_remaining -= encoding_bytes_in;
      }

   if(final_inputs && input_remaining)
      {
      std::vector<uint8_t> remainder(encoding_bytes_in, 0);
      for(size_t i = 0; i != input_remaining; ++i)
         { remainder[i] = input[input_consumed + i]; }

      base.encode(output + output_produced, remainder.data());

      const size_t bits_consumed = base.bits_consumed();
      const size_t remaining_bits_before_padding = base.remaining_bits_before_padding();

      size_t empty_bits = 8 * (encoding_bytes_in - input_remaining);
      size_t index = output_produced + encoding_bytes_out - 1;
      while(empty_bits >= remaining_bits_before_padding)
         {
         output[index--] = '=';
         empty_bits -= bits_consumed;
         }

      input_consumed += input_remaining;
      output_produced += encoding_bytes_out;
      }

   return output_produced;
   }


template <typename Base>
std::string base_encode_to_string(Base&& base, const uint8_t input[], size_t input_length)
   {
   const size_t output_length = base.encode_max_output(input_length);
   std::string output(output_length, 0);

   size_t consumed = 0;
   size_t produced = 0;

   if(output_length > 0)
      {
      produced = base_encode(base, &output.front(),
                                   input, input_length,
                                   consumed, true);
      }

   BOTAN_ASSERT_EQUAL(consumed, input_length, "Consumed the entire input");
   BOTAN_ASSERT_EQUAL(produced, output.size(), "Produced expected size");

   return output;
   }

/**
* Perform decoding using the base provided
* @param base object giving access to the encodings specifications
* @param output an array of at least Base::decode_max_output bytes
* @param input some base input
* @param input_length length of input in bytes
* @param input_consumed is an output parameter which says how many
*        bytes of input were actually consumed. If less than
*        input_length, then the range input[consumed:length]
*        should be passed in later along with more input.
* @param final_inputs true iff this is the last input, in which case
         padding is allowed
* @param ignore_ws ignore whitespace on input; if false, throw an
                   exception if whitespace is encountered
* @return number of bytes written to output
*/
template <typename Base>
size_t base_decode(Base&& base,
                   uint8_t output[],
                   const char input[],
                   size_t input_length,
                   size_t& input_consumed,
                   bool final_inputs,
                   bool ignore_ws = true)
   {
   const size_t decoding_bytes_in = base.decoding_bytes_in();
   const size_t decoding_bytes_out = base.decoding_bytes_out();

   uint8_t* out_ptr = output;
   std::vector<uint8_t> decode_buf(decoding_bytes_in, 0);
   size_t decode_buf_pos = 0;
   size_t final_truncate = 0;

   clear_mem(output, base.decode_max_output(input_length));

   for(size_t i = 0; i != input_length; ++i)
      {
      const uint8_t bin = base.lookup_binary_value(input[i]);

      if(base.check_bad_char(bin, input[i], ignore_ws)) // May throw Invalid_Argument
         {
         decode_buf[decode_buf_pos] = bin;
         ++decode_buf_pos;
         }

      /*
      * If we're at the end of the input, pad with 0s and truncate
      */
      if(final_inputs && (i == input_length - 1))
         {
         if(decode_buf_pos)
            {
            for(size_t j = decode_buf_pos; j < decoding_bytes_in; ++j)
               { decode_buf[j] = 0; }

            final_truncate = decoding_bytes_in - decode_buf_pos;
            decode_buf_pos = decoding_bytes_in;
            }
         }

      if(decode_buf_pos == decoding_bytes_in)
         {
         base.decode(out_ptr, decode_buf.data());

         out_ptr += decoding_bytes_out;
         decode_buf_pos = 0;
         input_consumed = i+1;
         }
      }

   while(input_consumed < input_length &&
         base.lookup_binary_value(input[input_consumed]) == 0x80)
      {
      ++input_consumed;
      }

   size_t written = (out_ptr - output) - base.bytes_to_remove(final_truncate);

   return written;
   }

template<typename Base>
size_t base_decode_full(Base&& base, uint8_t output[], const char input[], size_t input_length, bool ignore_ws)
   {
   size_t consumed = 0;
   const size_t written = base_decode(base, output, input, input_length, consumed, true, ignore_ws);

   if(consumed != input_length)
      {
      throw Invalid_Argument(base.name() + " decoding failed, input did not have full bytes");
      }

   return written;
   }

template<typename Vector, typename Base>
Vector base_decode_to_vec(Base&& base,
                          const char input[],
                          size_t input_length,
                          bool ignore_ws)
   {
   const size_t output_length = base.decode_max_output(input_length);
   Vector bin(output_length);

   const size_t written =
      base_decode_full(base, bin.data(), input, input_length, ignore_ws);

   bin.resize(written);
   return bin;
   }

}

#if defined(BOTAN_HAS_VALGRIND)
  #include <valgrind/memcheck.h>
#endif

namespace Botan {

namespace CT {

/**
* Use valgrind to mark the contents of memory as being undefined.
* Valgrind will accept operations which manipulate undefined values,
* but will warn if an undefined value is used to decided a conditional
* jump or a load/store address. So if we poison all of our inputs we
* can confirm that the operations in question are truly const time
* when compiled by whatever compiler is in use.
*
* Even better, the VALGRIND_MAKE_MEM_* macros work even when the
* program is not run under valgrind (though with a few cycles of
* overhead, which is unfortunate in final binaries as these
* annotations tend to be used in fairly important loops).
*
* This approach was first used in ctgrind (https://github.com/agl/ctgrind)
* but calling the valgrind mecheck API directly works just as well and
* doesn't require a custom patched valgrind.
*/
template<typename T>
inline void poison(const T* p, size_t n)
   {
#if defined(BOTAN_HAS_VALGRIND)
   VALGRIND_MAKE_MEM_UNDEFINED(p, n * sizeof(T));
#else
   BOTAN_UNUSED(p);
   BOTAN_UNUSED(n);
#endif
   }

template<typename T>
inline void unpoison(const T* p, size_t n)
   {
#if defined(BOTAN_HAS_VALGRIND)
   VALGRIND_MAKE_MEM_DEFINED(p, n * sizeof(T));
#else
   BOTAN_UNUSED(p);
   BOTAN_UNUSED(n);
#endif
   }

template<typename T>
inline void unpoison(T& p)
   {
#if defined(BOTAN_HAS_VALGRIND)
   VALGRIND_MAKE_MEM_DEFINED(&p, sizeof(T));
#else
   BOTAN_UNUSED(p);
#endif
   }

/**
* A Mask type used for constant-time operations. A Mask<T> always has value
* either 0 (all bits cleared) or ~0 (all bits set). All operations in a Mask<T>
* are intended to compile to code which does not contain conditional jumps.
* This must be verified with tooling (eg binary disassembly or using valgrind)
* since you never know what a compiler might do.
*/
template<typename T>
class Mask
   {
   public:
      static_assert(std::is_unsigned<T>::value, "CT::Mask only defined for unsigned integer types");

      Mask(const Mask<T>& other) = default;
      Mask<T>& operator=(const Mask<T>& other) = default;

      /**
      * Derive a Mask from a Mask of a larger type
      */
      template<typename U>
      Mask(Mask<U> o) : m_mask(static_cast<T>(o.value()))
         {
         static_assert(sizeof(U) > sizeof(T), "sizes ok");
         }

      /**
      * Return a Mask<T> with all bits set
      */
      static Mask<T> set()
         {
         return Mask<T>(static_cast<T>(~0));
         }

      /**
      * Return a Mask<T> with all bits cleared
      */
      static Mask<T> cleared()
         {
         return Mask<T>(0);
         }

      /**
      * Return a Mask<T> which is set if v is != 0
      */
      static Mask<T> expand(T v)
         {
         return ~Mask<T>::is_zero(v);
         }

      /**
      * Return a Mask<T> which is set if m is set
      */
      template<typename U>
      static Mask<T> expand(Mask<U> m)
         {
         static_assert(sizeof(U) < sizeof(T), "sizes ok");
         return ~Mask<T>::is_zero(m.value());
         }

      /**
      * Return a Mask<T> which is set if v is == 0 or cleared otherwise
      */
      static Mask<T> is_zero(T x)
         {
         return Mask<T>(ct_is_zero<T>(x));
         }

      /**
      * Return a Mask<T> which is set if x == y
      */
      static Mask<T> is_equal(T x, T y)
         {
         return Mask<T>::is_zero(static_cast<T>(x ^ y));
         }

      /**
      * Return a Mask<T> which is set if x < y
      */
      static Mask<T> is_lt(T x, T y)
         {
         return Mask<T>(expand_top_bit<T>(x^((x^y) | ((x-y)^x))));
         }

      /**
      * Return a Mask<T> which is set if x > y
      */
      static Mask<T> is_gt(T x, T y)
         {
         return Mask<T>::is_lt(y, x);
         }

      /**
      * Return a Mask<T> which is set if x <= y
      */
      static Mask<T> is_lte(T x, T y)
         {
         return ~Mask<T>::is_gt(x, y);
         }

      /**
      * Return a Mask<T> which is set if x >= y
      */
      static Mask<T> is_gte(T x, T y)
         {
         return ~Mask<T>::is_lt(x, y);
         }

      /**
      * AND-combine two masks
      */
      Mask<T>& operator&=(Mask<T> o)
         {
         m_mask &= o.value();
         return (*this);
         }

      /**
      * XOR-combine two masks
      */
      Mask<T>& operator^=(Mask<T> o)
         {
         m_mask ^= o.value();
         return (*this);
         }

      /**
      * OR-combine two masks
      */
      Mask<T>& operator|=(Mask<T> o)
         {
         m_mask |= o.value();
         return (*this);
         }

      /**
      * AND-combine two masks
      */
      friend Mask<T> operator&(Mask<T> x, Mask<T> y)
         {
         return Mask<T>(x.value() & y.value());
         }

      /**
      * XOR-combine two masks
      */
      friend Mask<T> operator^(Mask<T> x, Mask<T> y)
         {
         return Mask<T>(x.value() ^ y.value());
         }

      /**
      * OR-combine two masks
      */
      friend Mask<T> operator|(Mask<T> x, Mask<T> y)
         {
         return Mask<T>(x.value() | y.value());
         }

      /**
      * Negate this mask
      */
      Mask<T> operator~() const
         {
         return Mask<T>(~value());
         }

      /**
      * Return x if the mask is set, or otherwise zero
      */
      T if_set_return(T x) const
         {
         return m_mask & x;
         }

      /**
      * Return x if the mask is cleared, or otherwise zero
      */
      T if_not_set_return(T x) const
         {
         return ~m_mask & x;
         }

      /**
      * If this mask is set, return x, otherwise return y
      */
      T select(T x, T y) const
         {
         // (x & value()) | (y & ~value())
         return static_cast<T>(y ^ (value() & (x ^ y)));
         }

      T select_and_unpoison(T x, T y) const
         {
         T r = this->select(x, y);
         CT::unpoison(r);
         return r;
         }

      /**
      * If this mask is set, return x, otherwise return y
      */
      Mask<T> select_mask(Mask<T> x, Mask<T> y) const
         {
         return Mask<T>(select(x.value(), y.value()));
         }

      /**
      * Conditionally set output to x or y, depending on if mask is set or
      * cleared (resp)
      */
      void select_n(T output[], const T x[], const T y[], size_t len) const
         {
         for(size_t i = 0; i != len; ++i)
            output[i] = this->select(x[i], y[i]);
         }

      /**
      * If this mask is set, zero out buf, otherwise do nothing
      */
      void if_set_zero_out(T buf[], size_t elems)
         {
         for(size_t i = 0; i != elems; ++i)
            {
            buf[i] = this->if_not_set_return(buf[i]);
            }
         }

      /**
      * Return the value of the mask, unpoisoned
      */
      T unpoisoned_value() const
         {
         T r = value();
         CT::unpoison(r);
         return r;
         }

      /**
      * Return true iff this mask is set
      */
      bool is_set() const
         {
         return unpoisoned_value() != 0;
         }

      /**
      * Return the underlying value of the mask
      */
      T value() const
         {
         return m_mask;
         }

   private:
      Mask(T m) : m_mask(m) {}

      T m_mask;
   };

template<typename T>
inline Mask<T> conditional_copy_mem(T cnd,
                                    T* to,
                                    const T* from0,
                                    const T* from1,
                                    size_t elems)
   {
   const auto mask = CT::Mask<T>::expand(cnd);
   mask.select_n(to, from0, from1, elems);
   return mask;
   }

template<typename T>
inline void conditional_swap(bool cnd, T& x, T& y)
   {
   const auto swap = CT::Mask<T>::expand(cnd);

   T t0 = swap.select(y, x);
   T t1 = swap.select(x, y);
   x = t0;
   y = t1;
   }

template<typename T>
inline void conditional_swap_ptr(bool cnd, T& x, T& y)
   {
   uintptr_t xp = reinterpret_cast<uintptr_t>(x);
   uintptr_t yp = reinterpret_cast<uintptr_t>(y);

   conditional_swap<uintptr_t>(cnd, xp, yp);

   x = reinterpret_cast<T>(xp);
   y = reinterpret_cast<T>(yp);
   }

/**
* If bad_mask is unset, return in[delim_idx:input_length] copied to
* new buffer. If bad_mask is set, return an all zero vector of
* unspecified length.
*/
secure_vector<uint8_t> copy_output(CT::Mask<uint8_t> bad_input,
                                   const uint8_t input[],
                                   size_t input_length,
                                   size_t delim_idx);

secure_vector<uint8_t> strip_leading_zeros(const uint8_t in[], size_t length);

inline secure_vector<uint8_t> strip_leading_zeros(const secure_vector<uint8_t>& in)
   {
   return strip_leading_zeros(in.data(), in.size());
   }

}

}

namespace Botan {

class donna128 final
   {
   public:
      donna128(uint64_t ll = 0, uint64_t hh = 0) { l = ll; h = hh; }

      donna128(const donna128&) = default;
      donna128& operator=(const donna128&) = default;

      friend donna128 operator>>(const donna128& x, size_t shift)
         {
         donna128 z = x;
         if(shift > 0)
            {
            const uint64_t carry = z.h << (64 - shift);
            z.h = (z.h >> shift);
            z.l = (z.l >> shift) | carry;
            }
         return z;
         }

      friend donna128 operator<<(const donna128& x, size_t shift)
         {
         donna128 z = x;
         if(shift > 0)
            {
            const uint64_t carry = z.l >> (64 - shift);
            z.l = (z.l << shift);
            z.h = (z.h << shift) | carry;
            }
         return z;
         }

      friend uint64_t operator&(const donna128& x, uint64_t mask)
         {
         return x.l & mask;
         }

      uint64_t operator&=(uint64_t mask)
         {
         h = 0;
         l &= mask;
         return l;
         }

      donna128& operator+=(const donna128& x)
         {
         l += x.l;
         h += x.h;

         const uint64_t carry = (l < x.l);
         h += carry;
         return *this;
         }

      donna128& operator+=(uint64_t x)
         {
         l += x;
         const uint64_t carry = (l < x);
         h += carry;
         return *this;
         }

      uint64_t lo() const { return l; }
      uint64_t hi() const { return h; }
   private:
      uint64_t h = 0, l = 0;
   };

inline donna128 operator*(const donna128& x, uint64_t y)
   {
   BOTAN_ARG_CHECK(x.hi() == 0, "High 64 bits of donna128 set to zero during multiply");

   uint64_t lo = 0, hi = 0;
   mul64x64_128(x.lo(), y, &lo, &hi);
   return donna128(lo, hi);
   }

inline donna128 operator*(uint64_t y, const donna128& x)
   {
   return x * y;
   }

inline donna128 operator+(const donna128& x, const donna128& y)
   {
   donna128 z = x;
   z += y;
   return z;
   }

inline donna128 operator+(const donna128& x, uint64_t y)
   {
   donna128 z = x;
   z += y;
   return z;
   }

inline donna128 operator|(const donna128& x, const donna128& y)
   {
   return donna128(x.lo() | y.lo(), x.hi() | y.hi());
   }

inline uint64_t carry_shift(const donna128& a, size_t shift)
   {
   return (a >> shift).lo();
   }

inline uint64_t combine_lower(const donna128& a, size_t s1,
                              const donna128& b, size_t s2)
   {
   donna128 z = (a >> s1) | (b << s2);
   return z.lo();
   }

#if defined(BOTAN_TARGET_HAS_NATIVE_UINT128)
inline uint64_t carry_shift(const uint128_t a, size_t shift)
   {
   return static_cast<uint64_t>(a >> shift);
   }

inline uint64_t combine_lower(const uint128_t a, size_t s1,
                              const uint128_t b, size_t s2)
   {
   return static_cast<uint64_t>((a >> s1) | (b << s2));
   }
#endif

}

namespace Botan {

/**
* An element of the field \\Z/(2^255-19)
*/
class FE_25519
   {
   public:
      ~FE_25519() { secure_scrub_memory(m_fe, sizeof(m_fe)); }

      /**
      * Zero element
      */
      FE_25519(int init = 0)
         {
         if(init != 0 && init != 1)
            throw Invalid_Argument("Invalid FE_25519 initial value");
         clear_mem(m_fe, 10);
         m_fe[0] = init;
         }

      FE_25519(std::initializer_list<int32_t> x)
         {
         if(x.size() != 10)
            throw Invalid_Argument("Invalid FE_25519 initializer list");
         copy_mem(m_fe, x.begin(), 10);
         }

      FE_25519(int64_t h0, int64_t h1, int64_t h2, int64_t h3, int64_t h4,
               int64_t h5, int64_t h6, int64_t h7, int64_t h8, int64_t h9)
         {
         m_fe[0] = static_cast<int32_t>(h0);
         m_fe[1] = static_cast<int32_t>(h1);
         m_fe[2] = static_cast<int32_t>(h2);
         m_fe[3] = static_cast<int32_t>(h3);
         m_fe[4] = static_cast<int32_t>(h4);
         m_fe[5] = static_cast<int32_t>(h5);
         m_fe[6] = static_cast<int32_t>(h6);
         m_fe[7] = static_cast<int32_t>(h7);
         m_fe[8] = static_cast<int32_t>(h8);
         m_fe[9] = static_cast<int32_t>(h9);
         }

      FE_25519(const FE_25519& other) = default;
      FE_25519& operator=(const FE_25519& other) = default;

      FE_25519(FE_25519&& other) = default;
      FE_25519& operator=(FE_25519&& other) = default;

      void from_bytes(const uint8_t b[32]);
      void to_bytes(uint8_t b[32]) const;

      bool is_zero() const
         {
         uint8_t s[32];
         to_bytes(s);

         uint8_t sum = 0;
         for(size_t i = 0; i != 32; ++i)
            { sum |= s[i]; }

         // TODO avoid ternary here
         return (sum == 0) ? 1 : 0;
         }

      /*
      return 1 if f is in {1,3,5,...,q-2}
      return 0 if f is in {0,2,4,...,q-1}
      */
      bool is_negative() const
         {
         // TODO could avoid most of the to_bytes computation here
         uint8_t s[32];
         to_bytes(s);
         return s[0] & 1;
         }

      static FE_25519 add(const FE_25519& a, const FE_25519& b)
         {
         FE_25519 z;
         for(size_t i = 0; i != 10; ++i)
            { z[i] = a[i] + b[i]; }
         return z;
         }

      static FE_25519 sub(const FE_25519& a, const FE_25519& b)
         {
         FE_25519 z;
         for(size_t i = 0; i != 10; ++i)
            { z[i] = a[i] - b[i]; }
         return z;
         }

      static FE_25519 negate(const FE_25519& a)
         {
         FE_25519 z;
         for(size_t i = 0; i != 10; ++i)
            { z[i] = -a[i]; }
         return z;
         }

      static FE_25519 mul(const FE_25519& a, const FE_25519& b);
      static FE_25519 sqr_iter(const FE_25519& a, size_t iter);
      static FE_25519 sqr(const FE_25519& a) { return sqr_iter(a, 1); }
      static FE_25519 sqr2(const FE_25519& a);
      static FE_25519 pow_22523(const FE_25519& a);
      static FE_25519 invert(const FE_25519& a);

      // TODO remove
      int32_t operator[](size_t i) const { return m_fe[i]; }
      int32_t& operator[](size_t i) { return m_fe[i]; }

   private:

      int32_t m_fe[10];
   };

typedef FE_25519 fe;

/*
fe means field element.
Here the field is
An element t, entries t[0]...t[9], represents the integer
t[0]+2^26 t[1]+2^51 t[2]+2^77 t[3]+2^102 t[4]+...+2^230 t[9].
Bounds on each t[i] vary depending on context.
*/

inline void fe_frombytes(fe& x, const uint8_t* b)
   {
   x.from_bytes(b);
   }

inline void fe_tobytes(uint8_t* b, const fe& x)
   {
   x.to_bytes(b);
   }

inline void fe_copy(fe& a, const fe& b)
   {
   a = b;
   }

inline int fe_isnonzero(const fe& x)
   {
   return x.is_zero() ? 0 : 1;
   }

inline int fe_isnegative(const fe& x)
   {
   return x.is_negative();
   }


inline void fe_0(fe& x)
   {
   x = FE_25519();
   }

inline void fe_1(fe& x)
   {
   x = FE_25519(1);
   }

inline void fe_add(fe& x, const fe& a, const fe& b)
   {
   x = FE_25519::add(a, b);
   }

inline void fe_sub(fe& x, const fe& a, const fe& b)
   {
   x = FE_25519::sub(a, b);
   }

inline void fe_neg(fe& x, const fe& z)
   {
   x = FE_25519::negate(z);
   }

inline void fe_mul(fe& x, const fe& a, const fe& b)
   {
   x = FE_25519::mul(a, b);
   }

inline void fe_sq(fe& x, const fe& z)
   {
   x = FE_25519::sqr(z);
   }

inline void fe_sq_iter(fe& x, const fe& z, size_t iter)
   {
   x = FE_25519::sqr_iter(z, iter);
   }

inline void fe_sq2(fe& x, const fe& z)
   {
   x = FE_25519::sqr2(z);
   }

inline void fe_invert(fe& x, const fe& z)
   {
   x = FE_25519::invert(z);
   }

inline void fe_pow22523(fe& x, const fe& y)
   {
   x = FE_25519::pow_22523(y);
   }

}

namespace Botan {

inline uint64_t load_3(const uint8_t in[3])
   {
   return static_cast<uint64_t>(in[0]) |
      (static_cast<uint64_t>(in[1]) << 8) |
      (static_cast<uint64_t>(in[2]) << 16);
   }

inline uint64_t load_4(const uint8_t* in)
   {
   return load_le<uint32_t>(in, 0);
   }

template<size_t S, int64_t MUL=1>
inline void carry(int64_t& h0, int64_t& h1)
   {
   static_assert(S > 0 && S < 64, "Shift in range");

   const int64_t X1 = (static_cast<int64_t>(1) << S);
   const int64_t X2 = (static_cast<int64_t>(1) << (S - 1));
   int64_t c = (h0 + X2)  >> S;
   h1 += c * MUL;
   h0 -= c * X1;
   }

template<size_t S>
inline void carry0(int64_t& h0, int64_t& h1)
   {
   static_assert(S > 0 && S < 64, "Shift in range");

   const int64_t X1 = (static_cast<int64_t>(1) << S);
   int64_t c = h0 >> S;
   h1 += c;
   h0 -= c * X1;
   }

template<size_t S>
inline void carry0(int32_t& h0, int32_t& h1)
   {
   static_assert(S > 0 && S < 32, "Shift in range");

   const int32_t X1 = (static_cast<int64_t>(1) << S);
   int32_t c = h0 >> S;
   h1 += c;
   h0 -= c * X1;
   }

inline void redc_mul(int64_t& s1,
                     int64_t& s2,
                     int64_t& s3,
                     int64_t& s4,
                     int64_t& s5,
                     int64_t& s6,
                     int64_t& X)
   {
   s1 += X * 666643;
   s2 += X * 470296;
   s3 += X * 654183;
   s4 -= X * 997805;
   s5 += X * 136657;
   s6 -= X * 683901;
   X = 0;
   }

/*
ge means group element.

Here the group is the set of pairs (x,y) of field elements (see fe.h)
satisfying -x^2 + y^2 = 1 + d x^2y^2
where d = -121665/121666.

Representations:
  ge_p3 (extended): (X:Y:Z:T) satisfying x=X/Z, y=Y/Z, XY=ZT
*/

typedef struct
   {
   fe X;
   fe Y;
   fe Z;
   fe T;
   } ge_p3;

int ge_frombytes_negate_vartime(ge_p3*, const uint8_t*);
void ge_scalarmult_base(uint8_t out[32], const uint8_t in[32]);

void ge_double_scalarmult_vartime(uint8_t out[32],
                                  const uint8_t a[],
                                  const ge_p3* A,
                                  const uint8_t b[]);

/*
The set of scalars is \Z/l
where l = 2^252 + 27742317777372353535851937790883648493.
*/

void sc_reduce(uint8_t*);
void sc_muladd(uint8_t*, const uint8_t*, const uint8_t*, const uint8_t*);

}

namespace Botan {

/**
* No_Filesystem_Access Exception
*/
class BOTAN_PUBLIC_API(2,0) No_Filesystem_Access final : public Exception
   {
   public:
      No_Filesystem_Access() : Exception("No filesystem access enabled.")
         {}
   };

BOTAN_TEST_API bool has_filesystem_impl();

BOTAN_TEST_API std::vector<std::string> get_files_recursive(const std::string& dir);

}

namespace Botan {

class BigInt;
class Modular_Reducer;

class Montgomery_Params;

class Montgomery_Exponentation_State;

/*
* Precompute for calculating values g^x mod p
*/
std::shared_ptr<const Montgomery_Exponentation_State>
monty_precompute(std::shared_ptr<const Montgomery_Params> params_p,
                 const BigInt& g,
                 size_t window_bits,
                 bool const_time = true);

/*
* Return g^k mod p
*/
BigInt monty_execute(const Montgomery_Exponentation_State& precomputed_state,
                     const BigInt& k, size_t max_k_bits);

/*
* Return g^k mod p taking variable time depending on k
* @warning only use this if k is public
*/
BigInt monty_execute_vartime(const Montgomery_Exponentation_State& precomputed_state,
                             const BigInt& k);

/**
* Return (x^z1 * y^z2) % p
*/
BigInt monty_multi_exp(std::shared_ptr<const Montgomery_Params> params_p,
                       const BigInt& x,
                       const BigInt& z1,
                       const BigInt& y,
                       const BigInt& z2);

}

namespace Botan {

#if (BOTAN_MP_WORD_BITS == 32)
  typedef uint64_t dword;
  #define BOTAN_HAS_MP_DWORD

#elif (BOTAN_MP_WORD_BITS == 64)
  #if defined(BOTAN_TARGET_HAS_NATIVE_UINT128)
    typedef uint128_t dword;
    #define BOTAN_HAS_MP_DWORD
  #else
    // No native 128 bit integer type; use mul64x64_128 instead
  #endif

#else
  #error BOTAN_MP_WORD_BITS must be 32 or 64
#endif

#if defined(BOTAN_USE_GCC_INLINE_ASM)

  #if defined(BOTAN_TARGET_ARCH_IS_X86_32) && (BOTAN_MP_WORD_BITS == 32)
    #define BOTAN_MP_USE_X86_32_ASM
  #elif defined(BOTAN_TARGET_ARCH_IS_X86_64) && (BOTAN_MP_WORD_BITS == 64)
    #define BOTAN_MP_USE_X86_64_ASM
  #endif

#endif

/*
* Word Multiply/Add
*/
inline word word_madd2(word a, word b, word* c)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(R"(
      mull %[b]
      addl %[c],%[a]
      adcl $0,%[carry]
      )"
      : [a]"=a"(a), [b]"=rm"(b), [carry]"=&d"(*c)
      : "0"(a), "1"(b), [c]"g"(*c) : "cc");

   return a;

#elif defined(BOTAN_MP_USE_X86_64_ASM)
      asm(R"(
         mulq %[b]
         addq %[c],%[a]
         adcq $0,%[carry]
      )"
      : [a]"=a"(a), [b]"=rm"(b), [carry]"=&d"(*c)
      : "0"(a), "1"(b), [c]"g"(*c) : "cc");

   return a;

#elif defined(BOTAN_HAS_MP_DWORD)
   const dword s = static_cast<dword>(a) * b + *c;
   *c = static_cast<word>(s >> BOTAN_MP_WORD_BITS);
   return static_cast<word>(s);
#else
   static_assert(BOTAN_MP_WORD_BITS == 64, "Unexpected word size");

   word hi = 0, lo = 0;

   mul64x64_128(a, b, &lo, &hi);

   lo += *c;
   hi += (lo < *c); // carry?

   *c = hi;
   return lo;
#endif
   }

/*
* Word Multiply/Add
*/
inline word word_madd3(word a, word b, word c, word* d)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(R"(
      mull %[b]

      addl %[c],%[a]
      adcl $0,%[carry]

      addl %[d],%[a]
      adcl $0,%[carry]
      )"
      : [a]"=a"(a), [b]"=rm"(b), [carry]"=&d"(*d)
      : "0"(a), "1"(b), [c]"g"(c), [d]"g"(*d) : "cc");

   return a;

#elif defined(BOTAN_MP_USE_X86_64_ASM)
   asm(R"(
      mulq %[b]
      addq %[c],%[a]
      adcq $0,%[carry]
      addq %[d],%[a]
      adcq $0,%[carry]
      )"
      : [a]"=a"(a), [b]"=rm"(b), [carry]"=&d"(*d)
      : "0"(a), "1"(b), [c]"g"(c), [d]"g"(*d) : "cc");

   return a;

#elif defined(BOTAN_HAS_MP_DWORD)
   const dword s = static_cast<dword>(a) * b + c + *d;
   *d = static_cast<word>(s >> BOTAN_MP_WORD_BITS);
   return static_cast<word>(s);
#else
   static_assert(BOTAN_MP_WORD_BITS == 64, "Unexpected word size");

   word hi = 0, lo = 0;

   mul64x64_128(a, b, &lo, &hi);

   lo += c;
   hi += (lo < c); // carry?

   lo += *d;
   hi += (lo < *d); // carry?

   *d = hi;
   return lo;
#endif
   }

}

namespace Botan {

#if defined(BOTAN_MP_USE_X86_32_ASM)

#define ADDSUB2_OP(OPERATION, INDEX)                     \
        ASM("movl 4*" #INDEX "(%[y]), %[carry]")         \
        ASM(OPERATION " %[carry], 4*" #INDEX "(%[x])")   \

#define ADDSUB3_OP(OPERATION, INDEX)                     \
        ASM("movl 4*" #INDEX "(%[x]), %[carry]")         \
        ASM(OPERATION " 4*" #INDEX "(%[y]), %[carry]")   \
        ASM("movl %[carry], 4*" #INDEX "(%[z])")         \

#define LINMUL_OP(WRITE_TO, INDEX)                       \
        ASM("movl 4*" #INDEX "(%[x]),%%eax")             \
        ASM("mull %[y]")                                 \
        ASM("addl %[carry],%%eax")                       \
        ASM("adcl $0,%%edx")                             \
        ASM("movl %%edx,%[carry]")                       \
        ASM("movl %%eax, 4*" #INDEX "(%[" WRITE_TO "])")

#define MULADD_OP(IGNORED, INDEX)                        \
        ASM("movl 4*" #INDEX "(%[x]),%%eax")             \
        ASM("mull %[y]")                                 \
        ASM("addl %[carry],%%eax")                       \
        ASM("adcl $0,%%edx")                             \
        ASM("addl 4*" #INDEX "(%[z]),%%eax")             \
        ASM("adcl $0,%%edx")                             \
        ASM("movl %%edx,%[carry]")                       \
        ASM("movl %%eax, 4*" #INDEX " (%[z])")

#define ADD_OR_SUBTRACT(CORE_CODE)     \
        ASM("rorl %[carry]")           \
        CORE_CODE                      \
        ASM("sbbl %[carry],%[carry]")  \
        ASM("negl %[carry]")

#elif defined(BOTAN_MP_USE_X86_64_ASM)

#define ADDSUB2_OP(OPERATION, INDEX)                     \
        ASM("movq 8*" #INDEX "(%[y]), %[carry]")         \
        ASM(OPERATION " %[carry], 8*" #INDEX "(%[x])")   \

#define ADDSUB3_OP(OPERATION, INDEX)                     \
        ASM("movq 8*" #INDEX "(%[x]), %[carry]")         \
        ASM(OPERATION " 8*" #INDEX "(%[y]), %[carry]")   \
        ASM("movq %[carry], 8*" #INDEX "(%[z])")         \

#define LINMUL_OP(WRITE_TO, INDEX)                       \
        ASM("movq 8*" #INDEX "(%[x]),%%rax")             \
        ASM("mulq %[y]")                                 \
        ASM("addq %[carry],%%rax")                       \
        ASM("adcq $0,%%rdx")                             \
        ASM("movq %%rdx,%[carry]")                       \
        ASM("movq %%rax, 8*" #INDEX "(%[" WRITE_TO "])")

#define MULADD_OP(IGNORED, INDEX)                        \
        ASM("movq 8*" #INDEX "(%[x]),%%rax")             \
        ASM("mulq %[y]")                                 \
        ASM("addq %[carry],%%rax")                       \
        ASM("adcq $0,%%rdx")                             \
        ASM("addq 8*" #INDEX "(%[z]),%%rax")             \
        ASM("adcq $0,%%rdx")                             \
        ASM("movq %%rdx,%[carry]")                       \
        ASM("movq %%rax, 8*" #INDEX " (%[z])")

#define ADD_OR_SUBTRACT(CORE_CODE)     \
        ASM("rorq %[carry]")           \
        CORE_CODE                      \
        ASM("sbbq %[carry],%[carry]")  \
        ASM("negq %[carry]")

#endif

#if defined(ADD_OR_SUBTRACT)

#define ASM(x) x "\n\t"

#define DO_8_TIMES(MACRO, ARG) \
        MACRO(ARG, 0) \
        MACRO(ARG, 1) \
        MACRO(ARG, 2) \
        MACRO(ARG, 3) \
        MACRO(ARG, 4) \
        MACRO(ARG, 5) \
        MACRO(ARG, 6) \
        MACRO(ARG, 7)

#endif

/*
* Word Addition
*/
inline word word_add(word x, word y, word* carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      ADD_OR_SUBTRACT(ASM("adcl %[y],%[x]"))
      : [x]"=r"(x), [carry]"=r"(*carry)
      : "0"(x), [y]"rm"(y), "1"(*carry)
      : "cc");
   return x;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      ADD_OR_SUBTRACT(ASM("adcq %[y],%[x]"))
      : [x]"=r"(x), [carry]"=r"(*carry)
      : "0"(x), [y]"rm"(y), "1"(*carry)
      : "cc");
   return x;

#else
   word z = x + y;
   word c1 = (z < x);
   z += *carry;
   *carry = c1 | (z < *carry);
   return z;
#endif
   }

/*
* Eight Word Block Addition, Two Argument
*/
inline word word8_add2(word x[8], const word y[8], word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB2_OP, "adcl"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), "0"(carry)
      : "cc", "memory");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB2_OP, "adcq"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), "0"(carry)
      : "cc", "memory");
   return carry;

#else
   x[0] = word_add(x[0], y[0], &carry);
   x[1] = word_add(x[1], y[1], &carry);
   x[2] = word_add(x[2], y[2], &carry);
   x[3] = word_add(x[3], y[3], &carry);
   x[4] = word_add(x[4], y[4], &carry);
   x[5] = word_add(x[5], y[5], &carry);
   x[6] = word_add(x[6], y[6], &carry);
   x[7] = word_add(x[7], y[7], &carry);
   return carry;
#endif
   }

/*
* Eight Word Block Addition, Three Argument
*/
inline word word8_add3(word z[8], const word x[8],
                       const word y[8], word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB3_OP, "adcl"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), [z]"r"(z), "0"(carry)
      : "cc", "memory");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB3_OP, "adcq"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), [z]"r"(z), "0"(carry)
      : "cc", "memory");
   return carry;

#else
   z[0] = word_add(x[0], y[0], &carry);
   z[1] = word_add(x[1], y[1], &carry);
   z[2] = word_add(x[2], y[2], &carry);
   z[3] = word_add(x[3], y[3], &carry);
   z[4] = word_add(x[4], y[4], &carry);
   z[5] = word_add(x[5], y[5], &carry);
   z[6] = word_add(x[6], y[6], &carry);
   z[7] = word_add(x[7], y[7], &carry);
   return carry;
#endif
   }

/*
* Word Subtraction
*/
inline word word_sub(word x, word y, word* carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      ADD_OR_SUBTRACT(ASM("sbbl %[y],%[x]"))
      : [x]"=r"(x), [carry]"=r"(*carry)
      : "0"(x), [y]"rm"(y), "1"(*carry)
      : "cc");
   return x;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      ADD_OR_SUBTRACT(ASM("sbbq %[y],%[x]"))
      : [x]"=r"(x), [carry]"=r"(*carry)
      : "0"(x), [y]"rm"(y), "1"(*carry)
      : "cc");
   return x;

#else
   word t0 = x - y;
   word c1 = (t0 > x);
   word z = t0 - *carry;
   *carry = c1 | (z > t0);
   return z;
#endif
   }

/*
* Eight Word Block Subtraction, Two Argument
*/
inline word word8_sub2(word x[8], const word y[8], word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB2_OP, "sbbl"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), "0"(carry)
      : "cc", "memory");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB2_OP, "sbbq"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), "0"(carry)
      : "cc", "memory");
   return carry;

#else
   x[0] = word_sub(x[0], y[0], &carry);
   x[1] = word_sub(x[1], y[1], &carry);
   x[2] = word_sub(x[2], y[2], &carry);
   x[3] = word_sub(x[3], y[3], &carry);
   x[4] = word_sub(x[4], y[4], &carry);
   x[5] = word_sub(x[5], y[5], &carry);
   x[6] = word_sub(x[6], y[6], &carry);
   x[7] = word_sub(x[7], y[7], &carry);
   return carry;
#endif
   }

/*
* Eight Word Block Subtraction, Two Argument
*/
inline word word8_sub2_rev(word x[8], const word y[8], word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB3_OP, "sbbl"))
      : [carry]"=r"(carry)
      : [x]"r"(y), [y]"r"(x), [z]"r"(x), "0"(carry)
      : "cc", "memory");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB3_OP, "sbbq"))
      : [carry]"=r"(carry)
      : [x]"r"(y), [y]"r"(x), [z]"r"(x), "0"(carry)
      : "cc", "memory");
   return carry;

#else
   x[0] = word_sub(y[0], x[0], &carry);
   x[1] = word_sub(y[1], x[1], &carry);
   x[2] = word_sub(y[2], x[2], &carry);
   x[3] = word_sub(y[3], x[3], &carry);
   x[4] = word_sub(y[4], x[4], &carry);
   x[5] = word_sub(y[5], x[5], &carry);
   x[6] = word_sub(y[6], x[6], &carry);
   x[7] = word_sub(y[7], x[7], &carry);
   return carry;
#endif
   }

/*
* Eight Word Block Subtraction, Three Argument
*/
inline word word8_sub3(word z[8], const word x[8],
                       const word y[8], word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB3_OP, "sbbl"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), [z]"r"(z), "0"(carry)
      : "cc", "memory");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      ADD_OR_SUBTRACT(DO_8_TIMES(ADDSUB3_OP, "sbbq"))
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"r"(y), [z]"r"(z), "0"(carry)
      : "cc", "memory");
   return carry;

#else
   z[0] = word_sub(x[0], y[0], &carry);
   z[1] = word_sub(x[1], y[1], &carry);
   z[2] = word_sub(x[2], y[2], &carry);
   z[3] = word_sub(x[3], y[3], &carry);
   z[4] = word_sub(x[4], y[4], &carry);
   z[5] = word_sub(x[5], y[5], &carry);
   z[6] = word_sub(x[6], y[6], &carry);
   z[7] = word_sub(x[7], y[7], &carry);
   return carry;
#endif
   }

/*
* Eight Word Block Linear Multiplication
*/
inline word word8_linmul2(word x[8], word y, word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      DO_8_TIMES(LINMUL_OP, "x")
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"rm"(y), "0"(carry)
      : "cc", "%eax", "%edx");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      DO_8_TIMES(LINMUL_OP, "x")
      : [carry]"=r"(carry)
      : [x]"r"(x), [y]"rm"(y), "0"(carry)
      : "cc", "%rax", "%rdx");
   return carry;

#else
   x[0] = word_madd2(x[0], y, &carry);
   x[1] = word_madd2(x[1], y, &carry);
   x[2] = word_madd2(x[2], y, &carry);
   x[3] = word_madd2(x[3], y, &carry);
   x[4] = word_madd2(x[4], y, &carry);
   x[5] = word_madd2(x[5], y, &carry);
   x[6] = word_madd2(x[6], y, &carry);
   x[7] = word_madd2(x[7], y, &carry);
   return carry;
#endif
   }

/*
* Eight Word Block Linear Multiplication
*/
inline word word8_linmul3(word z[8], const word x[8], word y, word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      DO_8_TIMES(LINMUL_OP, "z")
      : [carry]"=r"(carry)
      : [z]"r"(z), [x]"r"(x), [y]"rm"(y), "0"(carry)
      : "cc", "%eax", "%edx");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)
   asm(
      DO_8_TIMES(LINMUL_OP, "z")
      : [carry]"=r"(carry)
      : [z]"r"(z), [x]"r"(x), [y]"rm"(y), "0"(carry)
      : "cc", "%rax", "%rdx");
   return carry;

#else
   z[0] = word_madd2(x[0], y, &carry);
   z[1] = word_madd2(x[1], y, &carry);
   z[2] = word_madd2(x[2], y, &carry);
   z[3] = word_madd2(x[3], y, &carry);
   z[4] = word_madd2(x[4], y, &carry);
   z[5] = word_madd2(x[5], y, &carry);
   z[6] = word_madd2(x[6], y, &carry);
   z[7] = word_madd2(x[7], y, &carry);
   return carry;
#endif
   }

/*
* Eight Word Block Multiply/Add
*/
inline word word8_madd3(word z[8], const word x[8], word y, word carry)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(
      DO_8_TIMES(MULADD_OP, "")
      : [carry]"=r"(carry)
      : [z]"r"(z), [x]"r"(x), [y]"rm"(y), "0"(carry)
      : "cc", "%eax", "%edx");
   return carry;

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(
      DO_8_TIMES(MULADD_OP, "")
      : [carry]"=r"(carry)
      : [z]"r"(z), [x]"r"(x), [y]"rm"(y), "0"(carry)
      : "cc", "%rax", "%rdx");
   return carry;

#else
   z[0] = word_madd3(x[0], y, z[0], &carry);
   z[1] = word_madd3(x[1], y, z[1], &carry);
   z[2] = word_madd3(x[2], y, z[2], &carry);
   z[3] = word_madd3(x[3], y, z[3], &carry);
   z[4] = word_madd3(x[4], y, z[4], &carry);
   z[5] = word_madd3(x[5], y, z[5], &carry);
   z[6] = word_madd3(x[6], y, z[6], &carry);
   z[7] = word_madd3(x[7], y, z[7], &carry);
   return carry;
#endif
   }

/*
* Multiply-Add Accumulator
* (w2,w1,w0) += x * y
*/
inline void word3_muladd(word* w2, word* w1, word* w0, word x, word y)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   word z0 = 0, z1 = 0;

   asm("mull %[y]"
        : "=a"(z0),"=d"(z1)
        : "a"(x), [y]"rm"(y)
        : "cc");

   asm(R"(
       addl %[z0],%[w0]
       adcl %[z1],%[w1]
       adcl $0,%[w2]
       )"
       : [w0]"=r"(*w0), [w1]"=r"(*w1), [w2]"=r"(*w2)
       : [z0]"r"(z0), [z1]"r"(z1), "0"(*w0), "1"(*w1), "2"(*w2)
       : "cc");

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   word z0 = 0, z1 = 0;

   asm("mulq %[y]"
        : "=a"(z0),"=d"(z1)
        : "a"(x), [y]"rm"(y)
        : "cc");

   asm(R"(
       addq %[z0],%[w0]
       adcq %[z1],%[w1]
       adcq $0,%[w2]
       )"
       : [w0]"=r"(*w0), [w1]"=r"(*w1), [w2]"=r"(*w2)
       : [z0]"r"(z0), [z1]"r"(z1), "0"(*w0), "1"(*w1), "2"(*w2)
       : "cc");

#else
   word carry = *w0;
   *w0 = word_madd2(x, y, &carry);
   *w1 += carry;
   *w2 += (*w1 < carry);
#endif
   }

/*
* 3-word addition
* (w2,w1,w0) += x
*/
inline void word3_add(word* w2, word* w1, word* w0, word x)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)
   asm(R"(
      addl %[x],%[w0]
      adcl $0,%[w1]
      adcl $0,%[w2]
      )"
      : [w0]"=r"(*w0), [w1]"=r"(*w1), [w2]"=r"(*w2)
      : [x]"r"(x), "0"(*w0), "1"(*w1), "2"(*w2)
      : "cc");

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   asm(R"(
      addq %[x],%[w0]
      adcq $0,%[w1]
      adcq $0,%[w2]
      )"
      : [w0]"=r"(*w0), [w1]"=r"(*w1), [w2]"=r"(*w2)
      : [x]"r"(x), "0"(*w0), "1"(*w1), "2"(*w2)
      : "cc");

#else
   *w0 += x;
   word c1 = (*w0 < x);
   *w1 += c1;
   word c2 = (*w1 < c1);
   *w2 += c2;
#endif
   }

/*
* Multiply-Add Accumulator
* (w2,w1,w0) += 2 * x * y
*/
inline void word3_muladd_2(word* w2, word* w1, word* w0, word x, word y)
   {
#if defined(BOTAN_MP_USE_X86_32_ASM)

   word z0 = 0, z1 = 0;

   asm("mull %[y]"
        : "=a"(z0),"=d"(z1)
        : "a"(x), [y]"rm"(y)
        : "cc");

   asm(R"(
      addl %[z0],%[w0]
      adcl %[z1],%[w1]
      adcl $0,%[w2]

      addl %[z0],%[w0]
      adcl %[z1],%[w1]
      adcl $0,%[w2]
      )"
      : [w0]"=r"(*w0), [w1]"=r"(*w1), [w2]"=r"(*w2)
      : [z0]"r"(z0), [z1]"r"(z1), "0"(*w0), "1"(*w1), "2"(*w2)
      : "cc");

#elif defined(BOTAN_MP_USE_X86_64_ASM)

   word z0 = 0, z1 = 0;

   asm("mulq %[y]"
        : "=a"(z0),"=d"(z1)
        : "a"(x), [y]"rm"(y)
        : "cc");

   asm(R"(
      addq %[z0],%[w0]
      adcq %[z1],%[w1]
      adcq $0,%[w2]

      addq %[z0],%[w0]
      adcq %[z1],%[w1]
      adcq $0,%[w2]
      )"
      : [w0]"=r"(*w0), [w1]"=r"(*w1), [w2]"=r"(*w2)
      : [z0]"r"(z0), [z1]"r"(z1), "0"(*w0), "1"(*w1), "2"(*w2)
      : "cc");

#else
   word carry = 0;
   x = word_madd2(x, y, &carry);
   y = carry;

   word top = (y >> (BOTAN_MP_WORD_BITS-1));
   y <<= 1;
   y |= (x >> (BOTAN_MP_WORD_BITS-1));
   x <<= 1;

   carry = 0;
   *w0 = word_add(*w0, x, &carry);
   *w1 = word_add(*w1, y, &carry);
   *w2 = word_add(*w2, top, &carry);
#endif
   }

#if defined(ASM)
  #undef ASM
  #undef DO_8_TIMES
  #undef ADD_OR_SUBTRACT
  #undef ADDSUB2_OP
  #undef ADDSUB3_OP
  #undef LINMUL_OP
  #undef MULADD_OP
#endif

}

namespace Botan {

const word MP_WORD_MAX = ~static_cast<word>(0);

/*
* If cond == 0, does nothing.
* If cond > 0, swaps x[0:size] with y[0:size]
* Runs in constant time
*/
inline void bigint_cnd_swap(word cnd, word x[], word y[], size_t size)
   {
   const auto mask = CT::Mask<word>::expand(cnd);

   for(size_t i = 0; i != size; ++i)
      {
      const word a = x[i];
      const word b = y[i];
      x[i] = mask.select(b, a);
      y[i] = mask.select(a, b);
      }
   }

inline word bigint_cnd_add(word cnd, word x[], word x_size,
                           const word y[], size_t y_size)
   {
   BOTAN_ASSERT(x_size >= y_size, "Expected sizes");

   const auto mask = CT::Mask<word>::expand(cnd);

   word carry = 0;

   const size_t blocks = y_size - (y_size % 8);
   word z[8] = { 0 };

   for(size_t i = 0; i != blocks; i += 8)
      {
      carry = word8_add3(z, x + i, y + i, carry);
      mask.select_n(x + i, z, x + i, 8);
      }

   for(size_t i = blocks; i != y_size; ++i)
      {
      z[0] = word_add(x[i], y[i], &carry);
      x[i] = mask.select(z[0], x[i]);
      }

   for(size_t i = y_size; i != x_size; ++i)
      {
      z[0] = word_add(x[i], 0, &carry);
      x[i] = mask.select(z[0], x[i]);
      }

   return mask.if_set_return(carry);
   }

/*
* If cond > 0 adds x[0:size] and y[0:size] and returns carry
* Runs in constant time
*/
inline word bigint_cnd_add(word cnd, word x[], const word y[], size_t size)
   {
   return bigint_cnd_add(cnd, x, size, y, size);
   }

/*
* If cond > 0 subtracts x[0:size] and y[0:size] and returns borrow
* Runs in constant time
*/
inline word bigint_cnd_sub(word cnd,
                           word x[], size_t x_size,
                           const word y[], size_t y_size)
   {
   BOTAN_ASSERT(x_size >= y_size, "Expected sizes");

   const auto mask = CT::Mask<word>::expand(cnd);

   word carry = 0;

   const size_t blocks = y_size - (y_size % 8);
   word z[8] = { 0 };

   for(size_t i = 0; i != blocks; i += 8)
      {
      carry = word8_sub3(z, x + i, y + i, carry);
      mask.select_n(x + i, z, x + i, 8);
      }

   for(size_t i = blocks; i != y_size; ++i)
      {
      z[0] = word_sub(x[i], y[i], &carry);
      x[i] = mask.select(z[0], x[i]);
      }

   for(size_t i = y_size; i != x_size; ++i)
      {
      z[0] = word_sub(x[i], 0, &carry);
      x[i] = mask.select(z[0], x[i]);
      }

   return mask.if_set_return(carry);
   }

/*
* If cond > 0 adds x[0:size] and y[0:size] and returns carry
* Runs in constant time
*/
inline word bigint_cnd_sub(word cnd, word x[], const word y[], size_t size)
   {
   return bigint_cnd_sub(cnd, x, size, y, size);
   }


/*
* Equivalent to
*   bigint_cnd_add( mask, x, y, size);
*   bigint_cnd_sub(~mask, x, y, size);
*
* Mask must be either 0 or all 1 bits
*/
inline void bigint_cnd_add_or_sub(CT::Mask<word> mask, word x[], const word y[], size_t size)
   {
   const size_t blocks = size - (size % 8);

   word carry = 0;
   word borrow = 0;

   word t0[8] = { 0 };
   word t1[8] = { 0 };

   for(size_t i = 0; i != blocks; i += 8)
      {
      carry = word8_add3(t0, x + i, y + i, carry);
      borrow = word8_sub3(t1, x + i, y + i, borrow);

      for(size_t j = 0; j != 8; ++j)
         x[i+j] = mask.select(t0[j], t1[j]);
      }

   for(size_t i = blocks; i != size; ++i)
      {
      const word a = word_add(x[i], y[i], &carry);
      const word s = word_sub(x[i], y[i], &borrow);

      x[i] = mask.select(a, s);
      }
   }

/*
* Equivalent to
*   bigint_cnd_add( mask, x, size, y, size);
*   bigint_cnd_sub(~mask, x, size, z, size);
*
* Mask must be either 0 or all 1 bits
*
* Returns the carry or borrow resp
*/
inline word bigint_cnd_addsub(CT::Mask<word> mask, word x[],
                              const word y[], const word z[],
                              size_t size)
   {
   const size_t blocks = size - (size % 8);

   word carry = 0;
   word borrow = 0;

   word t0[8] = { 0 };
   word t1[8] = { 0 };

   for(size_t i = 0; i != blocks; i += 8)
      {
      carry = word8_add3(t0, x + i, y + i, carry);
      borrow = word8_sub3(t1, x + i, z + i, borrow);

      for(size_t j = 0; j != 8; ++j)
         x[i+j] = mask.select(t0[j], t1[j]);
      }

   for(size_t i = blocks; i != size; ++i)
      {
      t0[0] = word_add(x[i], y[i], &carry);
      t1[0] = word_sub(x[i], z[i], &borrow);
      x[i] = mask.select(t0[0], t1[0]);
      }

   return mask.select(carry, borrow);
   }

/*
* 2s complement absolute value
* If cond > 0 sets x to ~x + 1
* Runs in constant time
*/
inline void bigint_cnd_abs(word cnd, word x[], size_t size)
   {
   const auto mask = CT::Mask<word>::expand(cnd);

   word carry = mask.if_set_return(1);
   for(size_t i = 0; i != size; ++i)
      {
      const word z = word_add(~x[i], 0, &carry);
      x[i] = mask.select(z, x[i]);
      }
   }

/**
* Two operand addition with carry out
*/
inline word bigint_add2_nc(word x[], size_t x_size, const word y[], size_t y_size)
   {
   word carry = 0;

   BOTAN_ASSERT(x_size >= y_size, "Expected sizes");

   const size_t blocks = y_size - (y_size % 8);

   for(size_t i = 0; i != blocks; i += 8)
      carry = word8_add2(x + i, y + i, carry);

   for(size_t i = blocks; i != y_size; ++i)
      x[i] = word_add(x[i], y[i], &carry);

   for(size_t i = y_size; i != x_size; ++i)
      x[i] = word_add(x[i], 0, &carry);

   return carry;
   }

/**
* Three operand addition with carry out
*/
inline word bigint_add3_nc(word z[],
                           const word x[], size_t x_size,
                           const word y[], size_t y_size)
   {
   if(x_size < y_size)
      { return bigint_add3_nc(z, y, y_size, x, x_size); }

   word carry = 0;

   const size_t blocks = y_size - (y_size % 8);

   for(size_t i = 0; i != blocks; i += 8)
      carry = word8_add3(z + i, x + i, y + i, carry);

   for(size_t i = blocks; i != y_size; ++i)
      z[i] = word_add(x[i], y[i], &carry);

   for(size_t i = y_size; i != x_size; ++i)
      z[i] = word_add(x[i], 0, &carry);

   return carry;
   }

/**
* Two operand addition
* @param x the first operand (and output)
* @param x_size size of x
* @param y the second operand
* @param y_size size of y (must be >= x_size)
*/
inline void bigint_add2(word x[], size_t x_size,
                        const word y[], size_t y_size)
   {
   x[x_size] += bigint_add2_nc(x, x_size, y, y_size);
   }

/**
* Three operand addition
*/
inline void bigint_add3(word z[],
                        const word x[], size_t x_size,
                        const word y[], size_t y_size)
   {
   z[x_size > y_size ? x_size : y_size] +=
      bigint_add3_nc(z, x, x_size, y, y_size);
   }

/**
* Two operand subtraction
*/
inline word bigint_sub2(word x[], size_t x_size,
                        const word y[], size_t y_size)
   {
   word borrow = 0;

   BOTAN_ASSERT(x_size >= y_size, "Expected sizes");

   const size_t blocks = y_size - (y_size % 8);

   for(size_t i = 0; i != blocks; i += 8)
      borrow = word8_sub2(x + i, y + i, borrow);

   for(size_t i = blocks; i != y_size; ++i)
      x[i] = word_sub(x[i], y[i], &borrow);

   for(size_t i = y_size; i != x_size; ++i)
      x[i] = word_sub(x[i], 0, &borrow);

   return borrow;
   }

/**
* Two operand subtraction, x = y - x; assumes y >= x
*/
inline void bigint_sub2_rev(word x[], const word y[], size_t y_size)
   {
   word borrow = 0;

   const size_t blocks = y_size - (y_size % 8);

   for(size_t i = 0; i != blocks; i += 8)
      borrow = word8_sub2_rev(x + i, y + i, borrow);

   for(size_t i = blocks; i != y_size; ++i)
      x[i] = word_sub(y[i], x[i], &borrow);

   BOTAN_ASSERT(borrow == 0, "y must be greater than x");
   }

/**
* Three operand subtraction
*/
inline word bigint_sub3(word z[],
                        const word x[], size_t x_size,
                        const word y[], size_t y_size)
   {
   word borrow = 0;

   BOTAN_ASSERT(x_size >= y_size, "Expected sizes");

   const size_t blocks = y_size - (y_size % 8);

   for(size_t i = 0; i != blocks; i += 8)
      borrow = word8_sub3(z + i, x + i, y + i, borrow);

   for(size_t i = blocks; i != y_size; ++i)
      z[i] = word_sub(x[i], y[i], &borrow);

   for(size_t i = y_size; i != x_size; ++i)
      z[i] = word_sub(x[i], 0, &borrow);

   return borrow;
   }

/**
* Return abs(x-y), ie if x >= y, then compute z = x - y
* Otherwise compute z = y - x
* No borrow is possible since the result is always >= 0
*
* Returns ~0 if x >= y or 0 if x < y
* @param z output array of at least N words
* @param x input array of N words
* @param y input array of N words
* @param N length of x and y
* @param ws array of at least 2*N words
*/
inline CT::Mask<word>
bigint_sub_abs(word z[],
               const word x[], const word y[], size_t N,
               word ws[])
   {
   // Subtract in both direction then conditional copy out the result

   word* ws0 = ws;
   word* ws1 = ws + N;

   word borrow0 = 0;
   word borrow1 = 0;

   const size_t blocks = N - (N % 8);

   for(size_t i = 0; i != blocks; i += 8)
      {
      borrow0 = word8_sub3(ws0 + i, x + i, y + i, borrow0);
      borrow1 = word8_sub3(ws1 + i, y + i, x + i, borrow1);
      }

   for(size_t i = blocks; i != N; ++i)
      {
      ws0[i] = word_sub(x[i], y[i], &borrow0);
      ws1[i] = word_sub(y[i], x[i], &borrow1);
      }

   return CT::conditional_copy_mem(borrow0, z, ws1, ws0, N);
   }

/*
* Shift Operations
*/
inline void bigint_shl1(word x[], size_t x_size, size_t x_words,
                        size_t word_shift, size_t bit_shift)
   {
   copy_mem(x + word_shift, x, x_words);
   clear_mem(x, word_shift);

   const auto carry_mask = CT::Mask<word>::expand(bit_shift);
   const size_t carry_shift = carry_mask.if_set_return(BOTAN_MP_WORD_BITS - bit_shift);

   word carry = 0;
   for(size_t i = word_shift; i != x_size; ++i)
      {
      const word w = x[i];
      x[i] = (w << bit_shift) | carry;
      carry = carry_mask.if_set_return(w >> carry_shift);
      }
   }

inline void bigint_shr1(word x[], size_t x_size,
                        size_t word_shift, size_t bit_shift)
   {
   const size_t top = x_size >= word_shift ? (x_size - word_shift) : 0;

   copy_mem(x, x + word_shift, top);
   clear_mem(x + top, std::min(word_shift, x_size));

   const auto carry_mask = CT::Mask<word>::expand(bit_shift);
   const size_t carry_shift = carry_mask.if_set_return(BOTAN_MP_WORD_BITS - bit_shift);

   word carry = 0;

   for(size_t i = 0; i != top; ++i)
      {
      const word w = x[top - i - 1];
      x[top-i-1] = (w >> bit_shift) | carry;
      carry = carry_mask.if_set_return(w << carry_shift);
      }
   }

inline void bigint_shl2(word y[], const word x[], size_t x_size,
                        size_t word_shift, size_t bit_shift)
   {
   copy_mem(y + word_shift, x, x_size);

   const auto carry_mask = CT::Mask<word>::expand(bit_shift);
   const size_t carry_shift = carry_mask.if_set_return(BOTAN_MP_WORD_BITS - bit_shift);

   word carry = 0;
   for(size_t i = word_shift; i != x_size + word_shift + 1; ++i)
      {
      const word w = y[i];
      y[i] = (w << bit_shift) | carry;
      carry = carry_mask.if_set_return(w >> carry_shift);
      }
   }

inline void bigint_shr2(word y[], const word x[], size_t x_size,
                        size_t word_shift, size_t bit_shift)
   {
   const size_t new_size = x_size < word_shift ? 0 : (x_size - word_shift);

   copy_mem(y, x + word_shift, new_size);

   const auto carry_mask = CT::Mask<word>::expand(bit_shift);
   const size_t carry_shift = carry_mask.if_set_return(BOTAN_MP_WORD_BITS - bit_shift);

   word carry = 0;
   for(size_t i = new_size; i > 0; --i)
      {
      word w = y[i-1];
      y[i-1] = (w >> bit_shift) | carry;
      carry = carry_mask.if_set_return(w << carry_shift);
      }
   }

/*
* Linear Multiply - returns the carry
*/
inline word BOTAN_WARN_UNUSED_RESULT bigint_linmul2(word x[], size_t x_size, word y)
   {
   const size_t blocks = x_size - (x_size % 8);

   word carry = 0;

   for(size_t i = 0; i != blocks; i += 8)
      carry = word8_linmul2(x + i, y, carry);

   for(size_t i = blocks; i != x_size; ++i)
      x[i] = word_madd2(x[i], y, &carry);

   return carry;
   }

inline void bigint_linmul3(word z[], const word x[], size_t x_size, word y)
   {
   const size_t blocks = x_size - (x_size % 8);

   word carry = 0;

   for(size_t i = 0; i != blocks; i += 8)
      carry = word8_linmul3(z + i, x + i, y, carry);

   for(size_t i = blocks; i != x_size; ++i)
      z[i] = word_madd2(x[i], y, &carry);

   z[x_size] = carry;
   }

/**
* Compare x and y
* Return -1 if x < y
* Return 0 if x == y
* Return 1 if x > y
*/
inline int32_t bigint_cmp(const word x[], size_t x_size,
                          const word y[], size_t y_size)
   {
   static_assert(sizeof(word) >= sizeof(uint32_t), "Size assumption");

   const word LT = static_cast<word>(-1);
   const word EQ = 0;
   const word GT = 1;

   const size_t common_elems = std::min(x_size, y_size);

   word result = EQ; // until found otherwise

   for(size_t i = 0; i != common_elems; i++)
      {
      const auto is_eq = CT::Mask<word>::is_equal(x[i], y[i]);
      const auto is_lt = CT::Mask<word>::is_lt(x[i], y[i]);

      result = is_eq.select(result, is_lt.select(LT, GT));
      }

   if(x_size < y_size)
      {
      word mask = 0;
      for(size_t i = x_size; i != y_size; i++)
         mask |= y[i];

      // If any bits were set in high part of y, then x < y
      result = CT::Mask<word>::is_zero(mask).select(result, LT);
      }
   else if(y_size < x_size)
      {
      word mask = 0;
      for(size_t i = y_size; i != x_size; i++)
         mask |= x[i];

      // If any bits were set in high part of x, then x > y
      result = CT::Mask<word>::is_zero(mask).select(result, GT);
      }

   CT::unpoison(result);
   BOTAN_DEBUG_ASSERT(result == LT || result == GT || result == EQ);
   return static_cast<int32_t>(result);
   }

/**
* Compare x and y
* Return ~0 if x[0:x_size] < y[0:y_size] or 0 otherwise
* If lt_or_equal is true, returns ~0 also for x == y
*/
inline CT::Mask<word>
bigint_ct_is_lt(const word x[], size_t x_size,
                const word y[], size_t y_size,
                bool lt_or_equal = false)
   {
   const size_t common_elems = std::min(x_size, y_size);

   auto is_lt = CT::Mask<word>::expand(lt_or_equal);

   for(size_t i = 0; i != common_elems; i++)
      {
      const auto eq = CT::Mask<word>::is_equal(x[i], y[i]);
      const auto lt = CT::Mask<word>::is_lt(x[i], y[i]);
      is_lt = eq.select_mask(is_lt, lt);
      }

   if(x_size < y_size)
      {
      word mask = 0;
      for(size_t i = x_size; i != y_size; i++)
         mask |= y[i];
      // If any bits were set in high part of y, then is_lt should be forced true
      is_lt |= CT::Mask<word>::expand(mask);
      }
   else if(y_size < x_size)
      {
      word mask = 0;
      for(size_t i = y_size; i != x_size; i++)
         mask |= x[i];

      // If any bits were set in high part of x, then is_lt should be false
      is_lt &= CT::Mask<word>::is_zero(mask);
      }

   return is_lt;
   }

inline CT::Mask<word>
bigint_ct_is_eq(const word x[], size_t x_size,
                const word y[], size_t y_size)
   {
   const size_t common_elems = std::min(x_size, y_size);

   word diff = 0;

   for(size_t i = 0; i != common_elems; i++)
      {
      diff |= (x[i] ^ y[i]);
      }

   // If any bits were set in high part of x/y, then they are not equal
   if(x_size < y_size)
      {
      for(size_t i = x_size; i != y_size; i++)
         diff |= y[i];
      }
   else if(y_size < x_size)
      {
      for(size_t i = y_size; i != x_size; i++)
         diff |= x[i];
      }

   return CT::Mask<word>::is_zero(diff);
   }

/**
* Set z to abs(x-y), ie if x >= y, then compute z = x - y
* Otherwise compute z = y - x
* No borrow is possible since the result is always >= 0
*
* Return the relative size of x vs y (-1, 0, 1)
*
* @param z output array of max(x_size,y_size) words
* @param x input param
* @param x_size length of x
* @param y input param
* @param y_size length of y
*/
inline int32_t
bigint_sub_abs(word z[],
               const word x[], size_t x_size,
               const word y[], size_t y_size)
   {
   const int32_t relative_size = bigint_cmp(x, x_size, y, y_size);

   // Swap if relative_size == -1
   const bool need_swap = relative_size < 0;
   CT::conditional_swap_ptr(need_swap, x, y);
   CT::conditional_swap(need_swap, x_size, y_size);

   /*
   * We know at this point that x >= y so if y_size is larger than
   * x_size, we are guaranteed they are just leading zeros which can
   * be ignored
   */
   y_size = std::min(x_size, y_size);

   bigint_sub3(z, x, x_size, y, y_size);

   return relative_size;
   }

/**
* Set t to t-s modulo mod
*
* @param t first integer
* @param s second integer
* @param mod the modulus
* @param mod_sw size of t, s, and mod
* @param ws workspace of size mod_sw
*/
inline void
bigint_mod_sub(word t[], const word s[], const word mod[], size_t mod_sw, word ws[])
   {
   // is t < s or not?
   const auto is_lt = bigint_ct_is_lt(t, mod_sw, s, mod_sw);

   // ws = p - s
   const word borrow = bigint_sub3(ws, mod, mod_sw, s, mod_sw);

   // Compute either (t - s) or (t + (p - s)) depending on mask
   const word carry = bigint_cnd_addsub(is_lt, t, ws, s, mod_sw);

   BOTAN_DEBUG_ASSERT(borrow == 0 && carry == 0);
   BOTAN_UNUSED(carry, borrow);
   }

template<size_t N>
inline void bigint_mod_sub_n(word t[], const word s[], const word mod[], word ws[])
   {
   // is t < s or not?
   const auto is_lt = bigint_ct_is_lt(t, N, s, N);

   // ws = p - s
   const word borrow = bigint_sub3(ws, mod, N, s, N);

   // Compute either (t - s) or (t + (p - s)) depending on mask
   const word carry = bigint_cnd_addsub(is_lt, t, ws, s, N);

   BOTAN_DEBUG_ASSERT(borrow == 0 && carry == 0);
   BOTAN_UNUSED(carry, borrow);
   }

/**
* Compute ((n1<<bits) + n0) / d
*/
inline word bigint_divop(word n1, word n0, word d)
   {
   if(d == 0)
      throw Invalid_Argument("bigint_divop divide by zero");

#if defined(BOTAN_HAS_MP_DWORD)
   return ((static_cast<dword>(n1) << BOTAN_MP_WORD_BITS) | n0) / d;
#else

   word high = n1 % d;
   word quotient = 0;

   for(size_t i = 0; i != BOTAN_MP_WORD_BITS; ++i)
      {
      const word high_top_bit = high >> (BOTAN_MP_WORD_BITS-1);

      high <<= 1;
      high |= (n0 >> (BOTAN_MP_WORD_BITS-1-i)) & 1;
      quotient <<= 1;

      if(high_top_bit || high >= d)
         {
         high -= d;
         quotient |= 1;
         }
      }

   return quotient;
#endif
   }

/**
* Compute ((n1<<bits) + n0) % d
*/
inline word bigint_modop(word n1, word n0, word d)
   {
   if(d == 0)
      throw Invalid_Argument("bigint_modop divide by zero");

#if defined(BOTAN_HAS_MP_DWORD)
   return ((static_cast<dword>(n1) << BOTAN_MP_WORD_BITS) | n0) % d;
#else
   word z = bigint_divop(n1, n0, d);
   word dummy = 0;
   z = word_madd2(z, d, &dummy);
   return (n0-z);
#endif
   }

/*
* Comba Multiplication / Squaring
*/
void bigint_comba_mul4(word z[8], const word x[4], const word y[4]);
void bigint_comba_mul6(word z[12], const word x[6], const word y[6]);
void bigint_comba_mul8(word z[16], const word x[8], const word y[8]);
void bigint_comba_mul9(word z[18], const word x[9], const word y[9]);
void bigint_comba_mul16(word z[32], const word x[16], const word y[16]);
void bigint_comba_mul24(word z[48], const word x[24], const word y[24]);

void bigint_comba_sqr4(word out[8], const word in[4]);
void bigint_comba_sqr6(word out[12], const word in[6]);
void bigint_comba_sqr8(word out[16], const word in[8]);
void bigint_comba_sqr9(word out[18], const word in[9]);
void bigint_comba_sqr16(word out[32], const word in[16]);
void bigint_comba_sqr24(word out[48], const word in[24]);

/**
* Montgomery Reduction
* @param z integer to reduce, of size exactly 2*(p_size+1).
           Output is in the first p_size+1 words, higher
           words are set to zero.
* @param p modulus
* @param p_size size of p
* @param p_dash Montgomery value
* @param workspace array of at least 2*(p_size+1) words
* @param ws_size size of workspace in words
*/
void bigint_monty_redc(word z[],
                       const word p[], size_t p_size,
                       word p_dash,
                       word workspace[],
                       size_t ws_size);

/*
* High Level Multiplication/Squaring Interfaces
*/

void bigint_mul(word z[], size_t z_size,
                const word x[], size_t x_size, size_t x_sw,
                const word y[], size_t y_size, size_t y_sw,
                word workspace[], size_t ws_size);

void bigint_sqr(word z[], size_t z_size,
                const word x[], size_t x_size, size_t x_sw,
                word workspace[], size_t ws_size);

}

namespace Botan {

/*
* Each of these functions makes the following assumptions:
*
* z_size >= 2*(p_size + 1)
* ws_size >= z_size
*/

void bigint_monty_redc_4(word z[], const word p[], word p_dash, word ws[]);
void bigint_monty_redc_6(word z[], const word p[], word p_dash, word ws[]);
void bigint_monty_redc_8(word z[], const word p[], word p_dash, word ws[]);
void bigint_monty_redc_16(word z[], const word p[], word p_dash, word ws[]);
void bigint_monty_redc_24(word z[], const word p[], word p_dash, word ws[]);
void bigint_monty_redc_32(word z[], const word p[], word p_dash, word ws[]);


}

namespace Botan {

namespace OS {

/*
* This header is internal (not installed) and these functions are not
* intended to be called by applications. However they are given public
* visibility (using BOTAN_TEST_API macro) for the tests. This also probably
* allows them to be overridden by the application on ELF systems, but
* this hasn't been tested.
*/

/**
* @return process ID assigned by the operating system.
* On Unix and Windows systems, this always returns a result
* On IncludeOS it returns 0 since there is no process ID to speak of
* in a unikernel.
*/
uint32_t BOTAN_TEST_API get_process_id();

/**
* Test if we are currently running with elevated permissions
* eg setuid, setgid, or with POSIX caps set.
*/
bool running_in_privileged_state();

/**
* @return CPU processor clock, if available
*
* On Windows, calls QueryPerformanceCounter.
*
* Under GCC or Clang on supported platforms the hardware cycle counter is queried.
* Currently supported processors are x86, PPC, Alpha, SPARC, IA-64, S/390x, and HP-PA.
* If no CPU cycle counter is available on this system, returns zero.
*/
uint64_t BOTAN_TEST_API get_cpu_cycle_counter();

size_t BOTAN_TEST_API get_cpu_total();
size_t BOTAN_TEST_API get_cpu_available();

/**
* Return the ELF auxiliary vector cooresponding to the given ID.
* This only makes sense on Unix-like systems and is currently
* only supported on Linux, Android, and FreeBSD.
*
* Returns zero if not supported on the current system or if
* the id provided is not known.
*/
unsigned long get_auxval(unsigned long id);

/*
* @return best resolution timestamp available
*
* The epoch and update rate of this clock is arbitrary and depending
* on the hardware it may not tick at a constant rate.
*
* Uses hardware cycle counter, if available.
* On POSIX platforms clock_gettime is used with a monotonic timer
* As a final fallback std::chrono::high_resolution_clock is used.
*/
uint64_t BOTAN_TEST_API get_high_resolution_clock();

/**
* @return system clock (reflecting wall clock) with best resolution
* available, normalized to nanoseconds resolution.
*/
uint64_t BOTAN_TEST_API get_system_timestamp_ns();

/**
* @return maximum amount of memory (in bytes) Botan could/should
* hyptothetically allocate for the memory poool. Reads environment
* variable "BOTAN_MLOCK_POOL_SIZE", set to "0" to disable pool.
*/
size_t get_memory_locking_limit();

/**
* Return the size of a memory page, if that can be derived on the
* current system. Otherwise returns some default value (eg 4096)
*/
size_t system_page_size();

/**
* Read the value of an environment variable, setting it to value_out if it
* exists.  Returns false and sets value_out to empty string if no such variable
* is set. If the process seems to be running in a privileged state (such as
* setuid) then always returns false and does not examine the environment.
*/
bool read_env_variable(std::string& value_out, const std::string& var_name);

/**
* Read the value of an environment variable and convert it to an
* integer. If not set or conversion fails, returns the default value.
*
* If the process seems to be running in a privileged state (such as setuid)
* then always returns nullptr, similiar to glibc's secure_getenv.
*/
size_t read_env_variable_sz(const std::string& var_name, size_t def_value = 0);

/**
* Request count pages of RAM which are locked into memory using mlock,
* VirtualLock, or some similar OS specific API. Free it with free_locked_pages.
*
* Returns an empty list on failure. This function is allowed to return fewer
* than count pages.
*
* The contents of the allocated pages are undefined.
*
* Each page is preceded by and followed by a page which is marked
* as noaccess, such that accessing it will cause a crash. This turns
* out of bound reads/writes into crash events.
*
* @param count requested number of locked pages
*/
std::vector<void*> allocate_locked_pages(size_t count);

/**
* Free memory allocated by allocate_locked_pages
* @param pages a list of pages returned by allocate_locked_pages
*/
void free_locked_pages(const std::vector<void*>& pages);

/**
* Set the MMU to prohibit access to this page
*/
void page_prohibit_access(void* page);

/**
* Set the MMU to allow R/W access to this page
*/
void page_allow_access(void* page);


/**
* Run a probe instruction to test for support for a CPU instruction.
* Runs in system-specific env that catches illegal instructions; this
* function always fails if the OS doesn't provide this.
* Returns value of probe_fn, if it could run.
* If error occurs, returns negative number.
* This allows probe_fn to indicate errors of its own, if it wants.
* For example the instruction might not only be only available on some
* CPUs, but also buggy on some subset of these - the probe function
* can test to make sure the instruction works properly before
* indicating that the instruction is available.
*
* @warning on Unix systems uses signal handling in a way that is not
* thread safe. It should only be called in a single-threaded context
* (ie, at static init time).
*
* If probe_fn throws an exception the result is undefined.
*
* Return codes:
* -1 illegal instruction detected
*/
int BOTAN_TEST_API run_cpu_instruction_probe(std::function<int ()> probe_fn);

/**
* Represents a terminal state
*/
class BOTAN_UNSTABLE_API Echo_Suppression
   {
   public:
      /**
      * Reenable echo on this terminal. Can be safely called
      * multiple times. May throw if an error occurs.
      */
      virtual void reenable_echo() = 0;

      /**
      * Implicitly calls reenable_echo, but swallows/ignored all
      * errors which would leave the terminal in an invalid state.
      */
      virtual ~Echo_Suppression() = default;
   };

/**
* Suppress echo on the terminal
* Returns null if this operation is not supported on the current system.
*/
std::unique_ptr<Echo_Suppression> BOTAN_UNSTABLE_API suppress_echo_on_terminal();

}

}

namespace Botan {

/**
* Returns the allowed padding schemes when using the given
* algorithm (key type) for creating digital signatures.
*
* @param algo the algorithm for which to look up supported padding schemes
* @return a vector of supported padding schemes
*/
BOTAN_TEST_API const std::vector<std::string> get_sig_paddings(const std::string algo);

/**
* Returns true iff the given padding scheme is valid for the given
* signature algorithm (key type).
*
* @param algo the signature algorithm to be used
* @param padding the padding scheme to be used
*/
bool sig_algo_and_pad_ok(const std::string algo, const std::string padding);

}

namespace Botan {

namespace PK_Ops {

class Encryption_with_EME : public Encryption
   {
   public:
      size_t max_input_bits() const override;

      secure_vector<uint8_t> encrypt(const uint8_t msg[], size_t msg_len,
                                  RandomNumberGenerator& rng) override;

      ~Encryption_with_EME() = default;
   protected:
      explicit Encryption_with_EME(const std::string& eme);
   private:
      virtual size_t max_raw_input_bits() const = 0;

      virtual secure_vector<uint8_t> raw_encrypt(const uint8_t msg[], size_t len,
                                              RandomNumberGenerator& rng) = 0;
      std::unique_ptr<EME> m_eme;
   };

class Decryption_with_EME : public Decryption
   {
   public:
      secure_vector<uint8_t> decrypt(uint8_t& valid_mask,
                                  const uint8_t msg[], size_t msg_len) override;

      ~Decryption_with_EME() = default;
   protected:
      explicit Decryption_with_EME(const std::string& eme);
   private:
      virtual secure_vector<uint8_t> raw_decrypt(const uint8_t msg[], size_t len) = 0;
      std::unique_ptr<EME> m_eme;
   };

class Verification_with_EMSA : public Verification
   {
   public:
      ~Verification_with_EMSA() = default;

      void update(const uint8_t msg[], size_t msg_len) override;
      bool is_valid_signature(const uint8_t sig[], size_t sig_len) override;

      bool do_check(const secure_vector<uint8_t>& msg,
                    const uint8_t sig[], size_t sig_len);

      std::string hash_for_signature() { return m_hash; }

   protected:
      explicit Verification_with_EMSA(const std::string& emsa);

      /**
      * Get the maximum message size in bits supported by this public key.
      * @return maximum message in bits
      */
      virtual size_t max_input_bits() const = 0;

      /**
      * @return boolean specifying if this signature scheme uses
      * a message prefix returned by message_prefix()
      */
      virtual bool has_prefix() { return false; }

      /**
      * @return the message prefix if this signature scheme uses
      * a message prefix, signaled via has_prefix()
      */
      virtual secure_vector<uint8_t> message_prefix() const { throw Invalid_State("No prefix"); }

      /**
      * @return boolean specifying if this key type supports message
      * recovery and thus if you need to call verify() or verify_mr()
      */
      virtual bool with_recovery() const = 0;

      /*
      * Perform a signature check operation
      * @param msg the message
      * @param msg_len the length of msg in bytes
      * @param sig the signature
      * @param sig_len the length of sig in bytes
      * @returns if signature is a valid one for message
      */
      virtual bool verify(const uint8_t[], size_t,
                          const uint8_t[], size_t)
         {
         throw Invalid_State("Message recovery required");
         }

      /*
      * Perform a signature operation (with message recovery)
      * Only call this if with_recovery() returns true
      * @param msg the message
      * @param msg_len the length of msg in bytes
      * @returns recovered message
      */
      virtual secure_vector<uint8_t> verify_mr(const uint8_t[], size_t)
         {
         throw Invalid_State("Message recovery not supported");
         }

      std::unique_ptr<EMSA> clone_emsa() const { return std::unique_ptr<EMSA>(m_emsa->clone()); }

   private:
      std::unique_ptr<EMSA> m_emsa;
      const std::string m_hash;
      bool m_prefix_used;
   };

class Signature_with_EMSA : public Signature
   {
   public:
      void update(const uint8_t msg[], size_t msg_len) override;

      secure_vector<uint8_t> sign(RandomNumberGenerator& rng) override;
   protected:
      explicit Signature_with_EMSA(const std::string& emsa);
      ~Signature_with_EMSA() = default;

      std::string hash_for_signature() { return m_hash; }

      /**
      * @return boolean specifying if this signature scheme uses
      * a message prefix returned by message_prefix()
      */
      virtual bool has_prefix() { return false; }

      /**
      * @return the message prefix if this signature scheme uses
      * a message prefix, signaled via has_prefix()
      */
      virtual secure_vector<uint8_t> message_prefix() const { throw Invalid_State("No prefix"); }

      std::unique_ptr<EMSA> clone_emsa() const { return std::unique_ptr<EMSA>(m_emsa->clone()); }

   private:

      /**
      * Get the maximum message size in bits supported by this public key.
      * @return maximum message in bits
      */
      virtual size_t max_input_bits() const = 0;

      bool self_test_signature(const std::vector<uint8_t>& msg,
                               const std::vector<uint8_t>& sig) const;

      virtual secure_vector<uint8_t> raw_sign(const uint8_t msg[], size_t msg_len,
                                           RandomNumberGenerator& rng) = 0;

      std::unique_ptr<EMSA> m_emsa;
      const std::string m_hash;
      bool m_prefix_used;
   };

class Key_Agreement_with_KDF : public Key_Agreement
   {
   public:
      secure_vector<uint8_t> agree(size_t key_len,
                                const uint8_t other_key[], size_t other_key_len,
                                const uint8_t salt[], size_t salt_len) override;

   protected:
      explicit Key_Agreement_with_KDF(const std::string& kdf);
      ~Key_Agreement_with_KDF() = default;
   private:
      virtual secure_vector<uint8_t> raw_agree(const uint8_t w[], size_t w_len) = 0;
      std::unique_ptr<KDF> m_kdf;
   };

class KEM_Encryption_with_KDF : public KEM_Encryption
   {
   public:
      void kem_encrypt(secure_vector<uint8_t>& out_encapsulated_key,
                       secure_vector<uint8_t>& out_shared_key,
                       size_t desired_shared_key_len,
                       Botan::RandomNumberGenerator& rng,
                       const uint8_t salt[],
                       size_t salt_len) override;

   protected:
      virtual void raw_kem_encrypt(secure_vector<uint8_t>& out_encapsulated_key,
                                   secure_vector<uint8_t>& raw_shared_key,
                                   Botan::RandomNumberGenerator& rng) = 0;

      explicit KEM_Encryption_with_KDF(const std::string& kdf);
      ~KEM_Encryption_with_KDF() = default;
   private:
      std::unique_ptr<KDF> m_kdf;
   };

class KEM_Decryption_with_KDF : public KEM_Decryption
   {
   public:
      secure_vector<uint8_t> kem_decrypt(const uint8_t encap_key[],
                                      size_t len,
                                      size_t desired_shared_key_len,
                                      const uint8_t salt[],
                                      size_t salt_len) override;

   protected:
      virtual secure_vector<uint8_t>
      raw_kem_decrypt(const uint8_t encap_key[], size_t len) = 0;

      explicit KEM_Decryption_with_KDF(const std::string& kdf);
      ~KEM_Decryption_with_KDF() = default;
   private:
      std::unique_ptr<KDF> m_kdf;
   };

}

}

namespace Botan {

class Modular_Reducer;

class PointGFp_Base_Point_Precompute final
   {
   public:
      PointGFp_Base_Point_Precompute(const PointGFp& base_point,
                                     const Modular_Reducer& mod_order);

      PointGFp mul(const BigInt& k,
                   RandomNumberGenerator& rng,
                   const BigInt& group_order,
                   std::vector<BigInt>& ws) const;
   private:
      const PointGFp& m_base_point;
      const Modular_Reducer& m_mod_order;

      enum { WINDOW_BITS = 3 };
      enum { WINDOW_SIZE = (1 << WINDOW_BITS) - 1 };

      const size_t m_p_words;
      const size_t m_T_size;

      /*
      * This is a table of T_size * 3*p_word words
      */
      std::vector<word> m_W;
   };

class PointGFp_Var_Point_Precompute final
   {
   public:
      PointGFp_Var_Point_Precompute(const PointGFp& point,
                                    RandomNumberGenerator& rng,
                                    std::vector<BigInt>& ws);

      PointGFp mul(const BigInt& k,
                   RandomNumberGenerator& rng,
                   const BigInt& group_order,
                   std::vector<BigInt>& ws) const;
   private:
      const CurveGFp m_curve;
      const size_t m_p_words;
      const size_t m_window_bits;

      /*
      * Table of 2^window_bits * 3*2*p_word words
      * Kept in locked vector since the base point might be sensitive
      * (normally isn't in most protocols but hard to say anything
      * categorically.)
      */
      secure_vector<word> m_T;
   };

class PointGFp_Multi_Point_Precompute final
   {
   public:
      PointGFp_Multi_Point_Precompute(const PointGFp& g1,
                                      const PointGFp& g2);

      /*
      * Return (g1*k1 + g2*k2)
      * Not constant time, intended to use with public inputs
      */
      PointGFp multi_exp(const BigInt& k1,
                         const BigInt& k2) const;
   private:
      std::vector<PointGFp> m_M;
   };

}

namespace Botan {

template<typename T>
inline void prefetch_readonly(const T* addr, size_t length)
   {
#if defined(__GNUG__)
   const size_t Ts_per_cache_line = CPUID::cache_line_size() / sizeof(T);

   for(size_t i = 0; i <= length; i += Ts_per_cache_line)
      __builtin_prefetch(addr + i, 0);
#endif
   }

template<typename T>
inline void prefetch_readwrite(const T* addr, size_t length)
   {
#if defined(__GNUG__)
   const size_t Ts_per_cache_line = CPUID::cache_line_size() / sizeof(T);

   for(size_t i = 0; i <= length; i += Ts_per_cache_line)
      __builtin_prefetch(addr + i, 1);
#endif
   }

}

namespace Botan {

class BigInt;
class Modular_Reducer;
class Montgomery_Params;
class RandomNumberGenerator;

/**
* Perform Lucas primality test
* @see FIPS 186-4 C.3.3
*
* @warning it is possible to construct composite integers which pass
* this test alone.
*
* @param n the positive integer to test
* @param mod_n a pre-created Modular_Reducer for n
* @return true if n seems probably prime, false if n is composite
*/
bool BOTAN_TEST_API is_lucas_probable_prime(const BigInt& n, const Modular_Reducer& mod_n);

/**
* Perform Bailie-PSW primality test
*
* This is a combination of Miller-Rabin with base 2 and a Lucas test. No known
* composite integer passes both tests, though it is conjectured that infinitely
* many composite counterexamples exist.
*
* @param n the positive integer to test
* @param mod_n a pre-created Modular_Reducer for n
* @return true if n seems probably prime, false if n is composite
*/
bool BOTAN_TEST_API is_bailie_psw_probable_prime(const BigInt& n, const Modular_Reducer& mod_n);

/**
* Perform Bailie-PSW primality test
*
* This is a combination of Miller-Rabin with base 2 and a Lucas test. No known
* composite integer passes both tests, though it is conjectured that infinitely
* many composite counterexamples exist.
*
* @param n the positive integer to test
* @return true if n seems probably prime, false if n is composite
*/
bool is_bailie_psw_probable_prime(const BigInt& n);

/**
* Return required number of Miller-Rabin tests in order to
* reach the specified probability of error.
*
* @param n_bits the bit-length of the integer being tested
* @param prob chance of false positive is bounded by 1/2**prob
* @param random is set if (and only if) the integer was randomly generated by us
*        and thus cannot have been maliciously constructed.
*/
size_t miller_rabin_test_iterations(size_t n_bits, size_t prob, bool random);

/**
* Perform a single Miller-Rabin test with specified base
*
* @param n the positive integer to test
* @param mod_n a pre-created Modular_Reducer for n
* @param monty_n Montgomery parameters for n
* @param a the base to check
* @return result of primality test
*/
bool passes_miller_rabin_test(const BigInt& n,
                              const Modular_Reducer& mod_n,
                              const std::shared_ptr<Montgomery_Params>& monty_n,
                              const BigInt& a);

/**
* Perform t iterations of a Miller-Rabin primality test with random bases
*
* @param n the positive integer to test
* @param mod_n a pre-created Modular_Reducer for n
* @param rng a random number generator
* @param t number of tests to perform
*
* @return result of primality test
*/
bool BOTAN_TEST_API is_miller_rabin_probable_prime(const BigInt& n,
                                                   const Modular_Reducer& mod_n,
                                                   RandomNumberGenerator& rng,
                                                   size_t t);

}

namespace Botan {

/**
* Round up
* @param n a non-negative integer
* @param align_to the alignment boundary
* @return n rounded up to a multiple of align_to
*/
inline size_t round_up(size_t n, size_t align_to)
   {
   BOTAN_ARG_CHECK(align_to != 0, "align_to must not be 0");

   if(n % align_to)
      n += align_to - (n % align_to);
   return n;
   }

/**
* Round down
* @param n an integer
* @param align_to the alignment boundary
* @return n rounded down to a multiple of align_to
*/
template<typename T>
inline constexpr T round_down(T n, T align_to)
   {
   return (align_to == 0) ? n : (n - (n % align_to));
   }

/**
* Clamp
*/
inline size_t clamp(size_t n, size_t lower_bound, size_t upper_bound)
   {
   if(n < lower_bound)
      return lower_bound;
   if(n > upper_bound)
      return upper_bound;
   return n;
   }

}

namespace Botan {

class BOTAN_PUBLIC_API(2,0) Integer_Overflow_Detected final : public Exception
   {
   public:
      Integer_Overflow_Detected(const std::string& file, int line) :
         Exception("Integer overflow detected at " + file + ":" + std::to_string(line))
         {}

      ErrorType error_type() const noexcept override { return ErrorType::InternalError; }
   };

inline size_t checked_add(size_t x, size_t y, const char* file, int line)
   {
   // TODO: use __builtin_x_overflow on GCC and Clang
   size_t z = x + y;
   if(z < x)
      {
      throw Integer_Overflow_Detected(file, line);
      }
   return z;
   }

#define BOTAN_CHECKED_ADD(x,y) checked_add(x,y,__FILE__,__LINE__)

}
#define SBoxE1(B0, B1, B2, B3)                    \
   do {                                           \
      B3 ^= B0;                                   \
      auto B4 = B1;                               \
      B1 &= B3;                                   \
      B4 ^= B2;                                   \
      B1 ^= B0;                                   \
      B0 |= B3;                                   \
      B0 ^= B4;                                   \
      B4 ^= B3;                                   \
      B3 ^= B2;                                   \
      B2 |= B1;                                   \
      B2 ^= B4;                                   \
      B4 = ~B4;                                   \
      B4 |= B1;                                   \
      B1 ^= B3;                                   \
      B1 ^= B4;                                   \
      B3 |= B0;                                   \
      B1 ^= B3;                                   \
      B4 ^= B3;                                   \
      B3 = B0;                                    \
      B0 = B1;                                    \
      B1 = B4;                                    \
   } while(0)

#define SBoxE2(B0, B1, B2, B3)                    \
   do {                                           \
      B0 = ~B0;                                   \
      B2 = ~B2;                                   \
      auto B4 = B0;                               \
      B0 &= B1;                                   \
      B2 ^= B0;                                   \
      B0 |= B3;                                   \
      B3 ^= B2;                                   \
      B1 ^= B0;                                   \
      B0 ^= B4;                                   \
      B4 |= B1;                                   \
      B1 ^= B3;                                   \
      B2 |= B0;                                   \
      B2 &= B4;                                   \
      B0 ^= B1;                                   \
      B1 &= B2;                                   \
      B1 ^= B0;                                   \
      B0 &= B2;                                   \
      B4 ^= B0;                                   \
      B0 = B2;                                    \
      B2 = B3;                                    \
      B3 = B1;                                    \
      B1 = B4;                                    \
   } while(0)

#define SBoxE3(B0, B1, B2, B3)                    \
   do {                                           \
      auto B4 = B0;                               \
      B0 &= B2;                                   \
      B0 ^= B3;                                   \
      B2 ^= B1;                                   \
      B2 ^= B0;                                   \
      B3 |= B4;                                   \
      B3 ^= B1;                                   \
      B4 ^= B2;                                   \
      B1 = B3;                                    \
      B3 |= B4;                                   \
      B3 ^= B0;                                   \
      B0 &= B1;                                   \
      B4 ^= B0;                                   \
      B1 ^= B3;                                   \
      B1 ^= B4;                                   \
      B0 = B2;                                    \
      B2 = B1;                                    \
      B1 = B3;                                    \
      B3 = ~B4;                                   \
   } while(0)

#define SBoxE4(B0, B1, B2, B3)                    \
   do {                                           \
      auto B4 = B0;                               \
      B0 |= B3;                                   \
      B3 ^= B1;                                   \
      B1 &= B4;                                   \
      B4 ^= B2;                                   \
      B2 ^= B3;                                   \
      B3 &= B0;                                   \
      B4 |= B1;                                   \
      B3 ^= B4;                                   \
      B0 ^= B1;                                   \
      B4 &= B0;                                   \
      B1 ^= B3;                                   \
      B4 ^= B2;                                   \
      B1 |= B0;                                   \
      B1 ^= B2;                                   \
      B0 ^= B3;                                   \
      B2 = B1;                                    \
      B1 |= B3;                                   \
      B0 ^= B1;                                   \
      B1 = B2;                                    \
      B2 = B3;                                    \
      B3 = B4;                                    \
   } while(0)

#define SBoxE5(B0, B1, B2, B3)                    \
   do {                                           \
      B1 ^= B3;                                   \
      B3 = ~B3;                                   \
      B2 ^= B3;                                   \
      B3 ^= B0;                                   \
      auto B4 = B1;                               \
      B1 &= B3;                                   \
      B1 ^= B2;                                   \
      B4 ^= B3;                                   \
      B0 ^= B4;                                   \
      B2 &= B4;                                   \
      B2 ^= B0;                                   \
      B0 &= B1;                                   \
      B3 ^= B0;                                   \
      B4 |= B1;                                   \
      B4 ^= B0;                                   \
      B0 |= B3;                                   \
      B0 ^= B2;                                   \
      B2 &= B3;                                   \
      B0 = ~B0;                                   \
      B4 ^= B2;                                   \
      B2 = B0;                                    \
      B0 = B1;                                    \
      B1 = B4;                                    \
   } while(0)

#define SBoxE6(B0, B1, B2, B3)                    \
   do {                                           \
      B0 ^= B1;                                   \
      B1 ^= B3;                                   \
      B3 = ~B3;                                   \
      auto B4 = B1;                               \
      B1 &= B0;                                   \
      B2 ^= B3;                                   \
      B1 ^= B2;                                   \
      B2 |= B4;                                   \
      B4 ^= B3;                                   \
      B3 &= B1;                                   \
      B3 ^= B0;                                   \
      B4 ^= B1;                                   \
      B4 ^= B2;                                   \
      B2 ^= B0;                                   \
      B0 &= B3;                                   \
      B2 = ~B2;                                   \
      B0 ^= B4;                                   \
      B4 |= B3;                                   \
      B4 ^= B2;                                   \
      B2 = B0;                                    \
      B0 = B1;                                    \
      B1 = B3;                                    \
      B3 = B4;                                    \
   } while(0)

#define SBoxE7(B0, B1, B2, B3)                    \
   do {                                           \
      B2 = ~B2;                                   \
      auto B4 = B3;                               \
      B3 &= B0;                                   \
      B0 ^= B4;                                   \
      B3 ^= B2;                                   \
      B2 |= B4;                                   \
      B1 ^= B3;                                   \
      B2 ^= B0;                                   \
      B0 |= B1;                                   \
      B2 ^= B1;                                   \
      B4 ^= B0;                                   \
      B0 |= B3;                                   \
      B0 ^= B2;                                   \
      B4 ^= B3;                                   \
      B4 ^= B0;                                   \
      B3 = ~B3;                                   \
      B2 &= B4;                                   \
      B3 ^= B2;                                   \
      B2 = B4;                                    \
   } while(0)

#define SBoxE8(B0, B1, B2, B3)                    \
   do {                                           \
      auto B4 = B1;                               \
      B1 |= B2;                                   \
      B1 ^= B3;                                   \
      B4 ^= B2;                                   \
      B2 ^= B1;                                   \
      B3 |= B4;                                   \
      B3 &= B0;                                   \
      B4 ^= B2;                                   \
      B3 ^= B1;                                   \
      B1 |= B4;                                   \
      B1 ^= B0;                                   \
      B0 |= B4;                                   \
      B0 ^= B2;                                   \
      B1 ^= B4;                                   \
      B2 ^= B1;                                   \
      B1 &= B0;                                   \
      B1 ^= B4;                                   \
      B2 = ~B2;                                   \
      B2 |= B0;                                   \
      B4 ^= B2;                                   \
      B2 = B1;                                    \
      B1 = B3;                                    \
      B3 = B0;                                    \
      B0 = B4;                                    \
   } while(0)

#define SBoxD1(B0, B1, B2, B3)                    \
   do {                                           \
      B2 = ~B2;                                   \
      auto B4 = B1;                               \
      B1 |= B0;                                   \
      B4 = ~B4;                                   \
      B1 ^= B2;                                   \
      B2 |= B4;                                   \
      B1 ^= B3;                                   \
      B0 ^= B4;                                   \
      B2 ^= B0;                                   \
      B0 &= B3;                                   \
      B4 ^= B0;                                   \
      B0 |= B1;                                   \
      B0 ^= B2;                                   \
      B3 ^= B4;                                   \
      B2 ^= B1;                                   \
      B3 ^= B0;                                   \
      B3 ^= B1;                                   \
      B2 &= B3;                                   \
      B4 ^= B2;                                   \
      B2 = B1;                                    \
      B1 = B4;                                    \
      } while(0)

#define SBoxD2(B0, B1, B2, B3)                    \
   do {                                           \
      auto B4 = B1;                               \
      B1 ^= B3;                                   \
      B3 &= B1;                                   \
      B4 ^= B2;                                   \
      B3 ^= B0;                                   \
      B0 |= B1;                                   \
      B2 ^= B3;                                   \
      B0 ^= B4;                                   \
      B0 |= B2;                                   \
      B1 ^= B3;                                   \
      B0 ^= B1;                                   \
      B1 |= B3;                                   \
      B1 ^= B0;                                   \
      B4 = ~B4;                                   \
      B4 ^= B1;                                   \
      B1 |= B0;                                   \
      B1 ^= B0;                                   \
      B1 |= B4;                                   \
      B3 ^= B1;                                   \
      B1 = B0;                                    \
      B0 = B4;                                    \
      B4 = B2;                                    \
      B2 = B3;                                    \
      B3 = B4;                                    \
      } while(0)

#define SBoxD3(B0, B1, B2, B3)                    \
   do {                                           \
      B2 ^= B3;                                   \
      B3 ^= B0;                                   \
      auto B4 = B3;                               \
      B3 &= B2;                                   \
      B3 ^= B1;                                   \
      B1 |= B2;                                   \
      B1 ^= B4;                                   \
      B4 &= B3;                                   \
      B2 ^= B3;                                   \
      B4 &= B0;                                   \
      B4 ^= B2;                                   \
      B2 &= B1;                                   \
      B2 |= B0;                                   \
      B3 = ~B3;                                   \
      B2 ^= B3;                                   \
      B0 ^= B3;                                   \
      B0 &= B1;                                   \
      B3 ^= B4;                                   \
      B3 ^= B0;                                   \
      B0 = B1;                                    \
      B1 = B4;                                    \
      } while(0)

#define SBoxD4(B0, B1, B2, B3)                    \
   do {                                           \
      auto B4 = B2;                               \
      B2 ^= B1;                                   \
      B0 ^= B2;                                   \
      B4 &= B2;                                   \
      B4 ^= B0;                                   \
      B0 &= B1;                                   \
      B1 ^= B3;                                   \
      B3 |= B4;                                   \
      B2 ^= B3;                                   \
      B0 ^= B3;                                   \
      B1 ^= B4;                                   \
      B3 &= B2;                                   \
      B3 ^= B1;                                   \
      B1 ^= B0;                                   \
      B1 |= B2;                                   \
      B0 ^= B3;                                   \
      B1 ^= B4;                                   \
      B0 ^= B1;                                   \
      B4 = B0;                                    \
      B0 = B2;                                    \
      B2 = B3;                                    \
      B3 = B4;                                    \
      } while(0)

#define SBoxD5(B0, B1, B2, B3)                    \
   do {                                           \
      auto B4 = B2;                               \
      B2 &= B3;                                   \
      B2 ^= B1;                                   \
      B1 |= B3;                                   \
      B1 &= B0;                                   \
      B4 ^= B2;                                   \
      B4 ^= B1;                                   \
      B1 &= B2;                                   \
      B0 = ~B0;                                   \
      B3 ^= B4;                                   \
      B1 ^= B3;                                   \
      B3 &= B0;                                   \
      B3 ^= B2;                                   \
      B0 ^= B1;                                   \
      B2 &= B0;                                   \
      B3 ^= B0;                                   \
      B2 ^= B4;                                   \
      B2 |= B3;                                   \
      B3 ^= B0;                                   \
      B2 ^= B1;                                   \
      B1 = B3;                                    \
      B3 = B4;                                    \
      } while(0)

#define SBoxD6(B0, B1, B2, B3)                    \
   do {                                           \
      B1 = ~B1;                                   \
      auto B4 = B3;                               \
      B2 ^= B1;                                   \
      B3 |= B0;                                   \
      B3 ^= B2;                                   \
      B2 |= B1;                                   \
      B2 &= B0;                                   \
      B4 ^= B3;                                   \
      B2 ^= B4;                                   \
      B4 |= B0;                                   \
      B4 ^= B1;                                   \
      B1 &= B2;                                   \
      B1 ^= B3;                                   \
      B4 ^= B2;                                   \
      B3 &= B4;                                   \
      B4 ^= B1;                                   \
      B3 ^= B4;                                   \
      B4 = ~B4;                                   \
      B3 ^= B0;                                   \
      B0 = B1;                                    \
      B1 = B4;                                    \
      B4 = B3;                                    \
      B3 = B2;                                    \
      B2 = B4;                                    \
      } while(0)

#define SBoxD7(B0, B1, B2, B3)                    \
   do {                                           \
      B0 ^= B2;                                   \
      auto B4 = B2;                               \
      B2 &= B0;                                   \
      B4 ^= B3;                                   \
      B2 = ~B2;                                   \
      B3 ^= B1;                                   \
      B2 ^= B3;                                   \
      B4 |= B0;                                   \
      B0 ^= B2;                                   \
      B3 ^= B4;                                   \
      B4 ^= B1;                                   \
      B1 &= B3;                                   \
      B1 ^= B0;                                   \
      B0 ^= B3;                                   \
      B0 |= B2;                                   \
      B3 ^= B1;                                   \
      B4 ^= B0;                                   \
      B0 = B1;                                    \
      B1 = B2;                                    \
      B2 = B4;                                    \
      } while(0)

#define SBoxD8(B0, B1, B2, B3)                    \
   do {                                           \
      auto B4 = B2;                               \
      B2 ^= B0;                                   \
      B0 &= B3;                                   \
      B4 |= B3;                                   \
      B2 = ~B2;                                   \
      B3 ^= B1;                                   \
      B1 |= B0;                                   \
      B0 ^= B2;                                   \
      B2 &= B4;                                   \
      B3 &= B4;                                   \
      B1 ^= B2;                                   \
      B2 ^= B0;                                   \
      B0 |= B2;                                   \
      B4 ^= B1;                                   \
      B0 ^= B3;                                   \
      B3 ^= B4;                                   \
      B4 |= B0;                                   \
      B3 ^= B2;                                   \
      B4 ^= B2;                                   \
      B2 = B1;                                    \
      B1 = B0;                                    \
      B0 = B3;                                    \
      B3 = B4;                                    \
      } while(0)

#if defined(BOTAN_TARGET_SUPPORTS_SSE2)
  #include <emmintrin.h>
  #define BOTAN_SIMD_USE_SSE2

#elif defined(BOTAN_TARGET_SUPPORTS_ALTIVEC)
  #include <altivec.h>
  #undef vector
  #undef bool
  #define BOTAN_SIMD_USE_ALTIVEC

#elif defined(BOTAN_TARGET_SUPPORTS_NEON)
  #include <arm_neon.h>
  #define BOTAN_SIMD_USE_NEON

#else
  #error "No SIMD instruction set enabled"
#endif

#if defined(BOTAN_SIMD_USE_SSE2)
  #define BOTAN_SIMD_ISA "sse2"
  #define BOTAN_VPERM_ISA "ssse3"
  #define BOTAN_CLMUL_ISA "pclmul"
#elif defined(BOTAN_SIMD_USE_NEON)
  #if defined(BOTAN_TARGET_ARCH_IS_ARM64)
    #define BOTAN_SIMD_ISA "+simd"
    #define BOTAN_CLMUL_ISA "+crypto"
  #else
    #define BOTAN_SIMD_ISA "fpu=neon"
  #endif
  #define BOTAN_VPERM_ISA BOTAN_SIMD_ISA
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
  #define BOTAN_SIMD_ISA "altivec"
  #define BOTAN_VPERM_ISA "altivec"
#endif

namespace Botan {

#if defined(BOTAN_SIMD_USE_SSE2)
   typedef __m128i native_simd_type;
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
   typedef __vector unsigned int native_simd_type;
#elif defined(BOTAN_SIMD_USE_NEON)
   typedef uint32x4_t native_simd_type;
#endif

/**
* 4x32 bit SIMD register
*
* This class is not a general purpose SIMD type, and only offers
* instructions needed for evaluation of specific crypto primitives.
* For example it does not currently have equality operators of any
* kind.
*
* Implemented for SSE2, VMX (Altivec), and NEON.
*/
class SIMD_4x32 final
   {
   public:

      SIMD_4x32& operator=(const SIMD_4x32& other) = default;
      SIMD_4x32(const SIMD_4x32& other) = default;

      SIMD_4x32& operator=(SIMD_4x32&& other) = default;
      SIMD_4x32(SIMD_4x32&& other) = default;

      /**
      * Zero initialize SIMD register with 4 32-bit elements
      */
      SIMD_4x32() // zero initialized
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_setzero_si128();
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         m_simd = vec_splat_u32(0);
#elif defined(BOTAN_SIMD_USE_NEON)
         m_simd = vdupq_n_u32(0);
#endif
         }

      /**
      * Load SIMD register with 4 32-bit elements
      */
      explicit SIMD_4x32(const uint32_t B[4])
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_loadu_si128(reinterpret_cast<const __m128i*>(B));
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         __vector unsigned int val = { B[0], B[1], B[2], B[3]};
         m_simd = val;
#elif defined(BOTAN_SIMD_USE_NEON)
         m_simd = vld1q_u32(B);
#endif
         }

      /**
      * Load SIMD register with 4 32-bit elements
      */
      SIMD_4x32(uint32_t B0, uint32_t B1, uint32_t B2, uint32_t B3)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_set_epi32(B3, B2, B1, B0);
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         __vector unsigned int val = {B0, B1, B2, B3};
         m_simd = val;
#elif defined(BOTAN_SIMD_USE_NEON)
         // Better way to do this?
         const uint32_t B[4] = { B0, B1, B2, B3 };
         m_simd = vld1q_u32(B);
#endif
         }

      /**
      * Load SIMD register with one 32-bit element repeated
      */
      static SIMD_4x32 splat(uint32_t B)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_set1_epi32(B));
#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vdupq_n_u32(B));
#else
         return SIMD_4x32(B, B, B, B);
#endif
         }

      /**
      * Load SIMD register with one 8-bit element repeated
      */
      static SIMD_4x32 splat_u8(uint8_t B)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_set1_epi8(B));
#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vreinterpretq_u32_u8(vdupq_n_u8(B)));
#else
         const uint32_t B4 = make_uint32(B, B, B, B);
         return SIMD_4x32(B4, B4, B4, B4);
#endif
         }

      /**
      * Load a SIMD register with little-endian convention
      */
      static SIMD_4x32 load_le(const void* in)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(in)));
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         uint32_t R[4];
         Botan::load_le(R, static_cast<const uint8_t*>(in), 4);
         return SIMD_4x32(R);
#elif defined(BOTAN_SIMD_USE_NEON)
         SIMD_4x32 l(vld1q_u32(static_cast<const uint32_t*>(in)));
         return CPUID::is_big_endian() ? l.bswap() : l;
#endif
         }

      /**
      * Load a SIMD register with big-endian convention
      */
      static SIMD_4x32 load_be(const void* in)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         return load_le(in).bswap();

#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         uint32_t R[4];
         Botan::load_be(R, static_cast<const uint8_t*>(in), 4);
         return SIMD_4x32(R);

#elif defined(BOTAN_SIMD_USE_NEON)
         SIMD_4x32 l(vld1q_u32(static_cast<const uint32_t*>(in)));
         return CPUID::is_little_endian() ? l.bswap() : l;
#endif
         }

      void store_le(uint32_t out[4]) const
         {
         this->store_le(reinterpret_cast<uint8_t*>(out));
         }

      void store_le(uint64_t out[2]) const
         {
         this->store_le(reinterpret_cast<uint8_t*>(out));
         }

      /**
      * Load a SIMD register with little-endian convention
      */
      void store_le(uint8_t out[]) const
         {
#if defined(BOTAN_SIMD_USE_SSE2)

         _mm_storeu_si128(reinterpret_cast<__m128i*>(out), raw());

#elif defined(BOTAN_SIMD_USE_ALTIVEC)

         union {
            __vector unsigned int V;
            uint32_t R[4];
            } vec;
         vec.V = raw();
         Botan::store_le(out, vec.R[0], vec.R[1], vec.R[2], vec.R[3]);

#elif defined(BOTAN_SIMD_USE_NEON)
         if(CPUID::is_little_endian())
            {
            vst1q_u8(out, vreinterpretq_u8_u32(m_simd));
            }
         else
            {
            vst1q_u8(out, vreinterpretq_u8_u32(bswap().m_simd));
            }
#endif
         }

      /**
      * Load a SIMD register with big-endian convention
      */
      void store_be(uint8_t out[]) const
         {
#if defined(BOTAN_SIMD_USE_SSE2)

         bswap().store_le(out);

#elif defined(BOTAN_SIMD_USE_ALTIVEC)

         union {
            __vector unsigned int V;
            uint32_t R[4];
            } vec;
         vec.V = m_simd;
         Botan::store_be(out, vec.R[0], vec.R[1], vec.R[2], vec.R[3]);

#elif defined(BOTAN_SIMD_USE_NEON)
         if(CPUID::is_little_endian())
            {
            vst1q_u8(out, vreinterpretq_u8_u32(bswap().m_simd));
            }
         else
            {
            vst1q_u8(out, vreinterpretq_u8_u32(m_simd));
            }
#endif
         }

      /*
      * This is used for SHA-2/SHACAL2
      * Return rotr(ROT1) ^ rotr(ROT2) ^ rotr(ROT3)
      */
      template<size_t ROT1, size_t ROT2, size_t ROT3>
      SIMD_4x32 rho() const
         {
         const SIMD_4x32 rot1 = this->rotr<ROT1>();
         const SIMD_4x32 rot2 = this->rotr<ROT2>();
         const SIMD_4x32 rot3 = this->rotr<ROT3>();
         return (rot1 ^ rot2 ^ rot3);
         }

      /**
      * Left rotation by a compile time constant
      */
      template<size_t ROT>
      SIMD_4x32 rotl() const
         {
         static_assert(ROT > 0 && ROT < 32, "Invalid rotation constant");

#if defined(BOTAN_SIMD_USE_SSE2)

         return SIMD_4x32(_mm_or_si128(_mm_slli_epi32(m_simd, static_cast<int>(ROT)),
                                       _mm_srli_epi32(m_simd, static_cast<int>(32-ROT))));

#elif defined(BOTAN_SIMD_USE_ALTIVEC)

         const unsigned int r = static_cast<unsigned int>(ROT);
         __vector unsigned int rot = {r, r, r, r};
         return SIMD_4x32(vec_rl(m_simd, rot));

#elif defined(BOTAN_SIMD_USE_NEON)

#if defined(BOTAN_TARGET_ARCH_IS_ARM64)

         BOTAN_IF_CONSTEXPR(ROT == 8)
            {
            const uint8_t maskb[16] = { 3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14 };
            const uint8x16_t mask = vld1q_u8(maskb);
            return SIMD_4x32(vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(m_simd), mask)));
            }
         else BOTAN_IF_CONSTEXPR(ROT == 16)
            {
            return SIMD_4x32(vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(m_simd))));
            }
#endif
         return SIMD_4x32(vorrq_u32(vshlq_n_u32(m_simd, static_cast<int>(ROT)),
                                    vshrq_n_u32(m_simd, static_cast<int>(32-ROT))));
#endif
         }

      /**
      * Right rotation by a compile time constant
      */
      template<size_t ROT>
      SIMD_4x32 rotr() const
         {
         return this->rotl<32-ROT>();
         }

      /**
      * Add elements of a SIMD vector
      */
      SIMD_4x32 operator+(const SIMD_4x32& other) const
         {
         SIMD_4x32 retval(*this);
         retval += other;
         return retval;
         }

      /**
      * Subtract elements of a SIMD vector
      */
      SIMD_4x32 operator-(const SIMD_4x32& other) const
         {
         SIMD_4x32 retval(*this);
         retval -= other;
         return retval;
         }

      /**
      * XOR elements of a SIMD vector
      */
      SIMD_4x32 operator^(const SIMD_4x32& other) const
         {
         SIMD_4x32 retval(*this);
         retval ^= other;
         return retval;
         }

      /**
      * Binary OR elements of a SIMD vector
      */
      SIMD_4x32 operator|(const SIMD_4x32& other) const
         {
         SIMD_4x32 retval(*this);
         retval |= other;
         return retval;
         }

      /**
      * Binary AND elements of a SIMD vector
      */
      SIMD_4x32 operator&(const SIMD_4x32& other) const
         {
         SIMD_4x32 retval(*this);
         retval &= other;
         return retval;
         }

      void operator+=(const SIMD_4x32& other)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_add_epi32(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         m_simd = vec_add(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_NEON)
         m_simd = vaddq_u32(m_simd, other.m_simd);
#endif
         }

      void operator-=(const SIMD_4x32& other)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_sub_epi32(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         m_simd = vec_sub(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_NEON)
         m_simd = vsubq_u32(m_simd, other.m_simd);
#endif
         }

      void operator^=(const SIMD_4x32& other)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_xor_si128(m_simd, other.m_simd);

#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         m_simd = vec_xor(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_NEON)
         m_simd = veorq_u32(m_simd, other.m_simd);
#endif
         }

      void operator|=(const SIMD_4x32& other)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_or_si128(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         m_simd = vec_or(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_NEON)
         m_simd = vorrq_u32(m_simd, other.m_simd);
#endif
         }

      void operator&=(const SIMD_4x32& other)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         m_simd = _mm_and_si128(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         m_simd = vec_and(m_simd, other.m_simd);
#elif defined(BOTAN_SIMD_USE_NEON)
         m_simd = vandq_u32(m_simd, other.m_simd);
#endif
         }


      template<int SHIFT> SIMD_4x32 shl() const
         {
         static_assert(SHIFT > 0 && SHIFT <= 31, "Invalid shift count");

#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_slli_epi32(m_simd, SHIFT));

#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         const unsigned int s = static_cast<unsigned int>(SHIFT);
         const __vector unsigned int shifts = {s, s, s, s};
         return SIMD_4x32(vec_sl(m_simd, shifts));
#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vshlq_n_u32(m_simd, SHIFT));
#endif
         }

      template<int SHIFT> SIMD_4x32 shr() const
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_srli_epi32(m_simd, SHIFT));

#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         const unsigned int s = static_cast<unsigned int>(SHIFT);
         const __vector unsigned int shifts = {s, s, s, s};
         return SIMD_4x32(vec_sr(m_simd, shifts));
#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vshrq_n_u32(m_simd, SHIFT));
#endif
         }

      SIMD_4x32 operator~() const
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_xor_si128(m_simd, _mm_set1_epi32(0xFFFFFFFF)));
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         return SIMD_4x32(vec_nor(m_simd, m_simd));
#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vmvnq_u32(m_simd));
#endif
         }

      // (~reg) & other
      SIMD_4x32 andc(const SIMD_4x32& other) const
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_andnot_si128(m_simd, other.m_simd));
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         /*
         AltiVec does arg1 & ~arg2 rather than SSE's ~arg1 & arg2
         so swap the arguments
         */
         return SIMD_4x32(vec_andc(other.m_simd, m_simd));
#elif defined(BOTAN_SIMD_USE_NEON)
         // NEON is also a & ~b
         return SIMD_4x32(vbicq_u32(other.m_simd, m_simd));
#endif
         }

      /**
      * Return copy *this with each word byte swapped
      */
      SIMD_4x32 bswap() const
         {
#if defined(BOTAN_SIMD_USE_SSE2)

         __m128i T = m_simd;
         T = _mm_shufflehi_epi16(T, _MM_SHUFFLE(2, 3, 0, 1));
         T = _mm_shufflelo_epi16(T, _MM_SHUFFLE(2, 3, 0, 1));
         return SIMD_4x32(_mm_or_si128(_mm_srli_epi16(T, 8), _mm_slli_epi16(T, 8)));

#elif defined(BOTAN_SIMD_USE_ALTIVEC)

         union {
            __vector unsigned int V;
            uint32_t R[4];
            } vec;

         vec.V = m_simd;
         bswap_4(vec.R);
         return SIMD_4x32(vec.R[0], vec.R[1], vec.R[2], vec.R[3]);

#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(m_simd))));
#endif
         }

      template<size_t I>
      SIMD_4x32 shift_elems_left() const
         {
         static_assert(I <= 3, "Invalid shift count");

#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_slli_si128(raw(), 4*I));
#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vextq_u32(vdupq_n_u32(0), raw(), 4-I));
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         const __vector unsigned int zero = vec_splat_u32(0);

         const __vector unsigned char shuf[3] = {
            { 16, 17, 18, 19, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
            { 16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7 },
            { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 0, 1, 2, 3 },
         };

         return SIMD_4x32(vec_perm(raw(), zero, shuf[I-1]));
#endif
         }

      template<size_t I>
      SIMD_4x32 shift_elems_right() const
         {
         static_assert(I <= 3, "Invalid shift count");

#if defined(BOTAN_SIMD_USE_SSE2)
         return SIMD_4x32(_mm_srli_si128(raw(), 4*I));
#elif defined(BOTAN_SIMD_USE_NEON)
         return SIMD_4x32(vextq_u32(raw(), vdupq_n_u32(0), I));
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         const __vector unsigned int zero = vec_splat_u32(0);

         const __vector unsigned char shuf[3] = {
            { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 },
            { 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 },
            { 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 },
         };

         return SIMD_4x32(vec_perm(raw(), zero, shuf[I-1]));
#endif
         }

      /**
      * 4x4 Transposition on SIMD registers
      */
      static void transpose(SIMD_4x32& B0, SIMD_4x32& B1,
                            SIMD_4x32& B2, SIMD_4x32& B3)
         {
#if defined(BOTAN_SIMD_USE_SSE2)
         const __m128i T0 = _mm_unpacklo_epi32(B0.m_simd, B1.m_simd);
         const __m128i T1 = _mm_unpacklo_epi32(B2.m_simd, B3.m_simd);
         const __m128i T2 = _mm_unpackhi_epi32(B0.m_simd, B1.m_simd);
         const __m128i T3 = _mm_unpackhi_epi32(B2.m_simd, B3.m_simd);

         B0.m_simd = _mm_unpacklo_epi64(T0, T1);
         B1.m_simd = _mm_unpackhi_epi64(T0, T1);
         B2.m_simd = _mm_unpacklo_epi64(T2, T3);
         B3.m_simd = _mm_unpackhi_epi64(T2, T3);
#elif defined(BOTAN_SIMD_USE_ALTIVEC)
         const __vector unsigned int T0 = vec_mergeh(B0.m_simd, B2.m_simd);
         const __vector unsigned int T1 = vec_mergeh(B1.m_simd, B3.m_simd);
         const __vector unsigned int T2 = vec_mergel(B0.m_simd, B2.m_simd);
         const __vector unsigned int T3 = vec_mergel(B1.m_simd, B3.m_simd);

         B0.m_simd = vec_mergeh(T0, T1);
         B1.m_simd = vec_mergel(T0, T1);
         B2.m_simd = vec_mergeh(T2, T3);
         B3.m_simd = vec_mergel(T2, T3);

#elif defined(BOTAN_SIMD_USE_NEON) && defined(BOTAN_TARGET_ARCH_IS_ARM32)
         const uint32x4x2_t T0 = vzipq_u32(B0.m_simd, B2.m_simd);
         const uint32x4x2_t T1 = vzipq_u32(B1.m_simd, B3.m_simd);
         const uint32x4x2_t O0 = vzipq_u32(T0.val[0], T1.val[0]);
         const uint32x4x2_t O1 = vzipq_u32(T0.val[1], T1.val[1]);

         B0.m_simd = O0.val[0];
         B1.m_simd = O0.val[1];
         B2.m_simd = O1.val[0];
         B3.m_simd = O1.val[1];

#elif defined(BOTAN_SIMD_USE_NEON) && defined(BOTAN_TARGET_ARCH_IS_ARM64)
         const uint32x4_t T0 = vzip1q_u32(B0.m_simd, B2.m_simd);
         const uint32x4_t T2 = vzip2q_u32(B0.m_simd, B2.m_simd);
         const uint32x4_t T1 = vzip1q_u32(B1.m_simd, B3.m_simd);
         const uint32x4_t T3 = vzip2q_u32(B1.m_simd, B3.m_simd);

         B0.m_simd = vzip1q_u32(T0, T1);
         B1.m_simd = vzip2q_u32(T0, T1);
         B2.m_simd = vzip1q_u32(T2, T3);
         B3.m_simd = vzip2q_u32(T2, T3);
#endif
         }

      native_simd_type raw() const BOTAN_FUNC_ISA(BOTAN_SIMD_ISA) { return m_simd; }

      explicit SIMD_4x32(native_simd_type x) : m_simd(x) {}
   private:
      native_simd_type m_simd;
   };

}

namespace Botan {

inline std::vector<uint8_t> to_byte_vector(const std::string& s)
   {
   return std::vector<uint8_t>(s.cbegin(), s.cend());
   }

inline std::string to_string(const secure_vector<uint8_t> &bytes)
   {
   return std::string(bytes.cbegin(), bytes.cend());
   }

/**
* Return the keys of a map as a std::set
*/
template<typename K, typename V>
std::set<K> map_keys_as_set(const std::map<K, V>& kv)
   {
   std::set<K> s;
   for(auto&& i : kv)
      {
      s.insert(i.first);
      }
   return s;
   }

/*
* Searching through a std::map
* @param mapping the map to search
* @param key is what to look for
* @param null_result is the value to return if key is not in mapping
* @return mapping[key] or null_result
*/
template<typename K, typename V>
inline V search_map(const std::map<K, V>& mapping,
                    const K& key,
                    const V& null_result = V())
   {
   auto i = mapping.find(key);
   if(i == mapping.end())
      return null_result;
   return i->second;
   }

template<typename K, typename V, typename R>
inline R search_map(const std::map<K, V>& mapping, const K& key,
                    const R& null_result, const R& found_result)
   {
   auto i = mapping.find(key);
   if(i == mapping.end())
      return null_result;
   return found_result;
   }

/*
* Insert a key/value pair into a multimap
*/
template<typename K, typename V>
void multimap_insert(std::multimap<K, V>& multimap,
                     const K& key, const V& value)
   {
   multimap.insert(std::make_pair(key, value));
   }

/**
* Existence check for values
*/
template<typename T>
bool value_exists(const std::vector<T>& vec,
                  const T& val)
   {
   for(size_t i = 0; i != vec.size(); ++i)
      if(vec[i] == val)
         return true;
   return false;
   }

template<typename T, typename Pred>
void map_remove_if(Pred pred, T& assoc)
   {
   auto i = assoc.begin();
   while(i != assoc.end())
      {
      if(pred(i->first))
         assoc.erase(i++);
      else
         i++;
      }
   }

}

namespace Botan {

class BOTAN_TEST_API Timer final
   {
   public:
      Timer(const std::string& name,
            const std::string& provider,
            const std::string& doing,
            uint64_t event_mult,
            size_t buf_size,
            double clock_cycle_ratio,
            uint64_t clock_speed)
         : m_name(name + ((provider.empty() || provider == "base") ? "" : " [" + provider + "]"))
         , m_doing(doing)
         , m_buf_size(buf_size)
         , m_event_mult(event_mult)
         , m_clock_cycle_ratio(clock_cycle_ratio)
         , m_clock_speed(clock_speed)
         {}

      Timer(const std::string& name) :
         Timer(name, "", "", 1, 0, 0.0, 0)
         {}

      Timer(const std::string& name, size_t buf_size) :
         Timer(name, "", "", buf_size, buf_size, 0.0, 0)
         {}

      Timer(const Timer& other) = default;
      Timer& operator=(const Timer& other) = default;

      void start();

      void stop();

      bool under(std::chrono::milliseconds msec)
         {
         return (milliseconds() < msec.count());
         }

      class Timer_Scope final
         {
         public:
            explicit Timer_Scope(Timer& timer)
               : m_timer(timer)
               {
               m_timer.start();
               }
            ~Timer_Scope()
               {
               try
                  {
                  m_timer.stop();
                  }
               catch(...) {}
               }
         private:
            Timer& m_timer;
         };

      template<typename F>
      auto run(F f) -> decltype(f())
         {
         Timer_Scope timer(*this);
         return f();
         }

      template<typename F>
      void run_until_elapsed(std::chrono::milliseconds msec, F f)
         {
         while(this->under(msec))
            {
            run(f);
            }
         }

      uint64_t value() const
         {
         return m_time_used;
         }

      double seconds() const
         {
         return milliseconds() / 1000.0;
         }

      double milliseconds() const
         {
         return value() / 1000000.0;
         }

      double ms_per_event() const
         {
         return milliseconds() / events();
         }

      uint64_t cycles_consumed() const
         {
         if(m_clock_speed != 0)
            {
            return static_cast<uint64_t>((m_clock_speed * value()) / 1000.0);
            }
         return m_cpu_cycles_used;
         }

      uint64_t events() const
         {
         return m_event_count * m_event_mult;
         }

      const std::string& get_name() const
         {
         return m_name;
         }

      const std::string& doing() const
         {
         return m_doing;
         }

      size_t buf_size() const
         {
         return m_buf_size;
         }

      double bytes_per_second() const
         {
         return seconds() > 0.0 ? events() / seconds() : 0.0;
         }

      double events_per_second() const
         {
         return seconds() > 0.0 ? events() / seconds() : 0.0;
         }

      double seconds_per_event() const
         {
         return events() > 0 ? seconds() / events() : 0.0;
         }

      void set_custom_msg(const std::string& s)
         {
         m_custom_msg = s;
         }

      bool operator<(const Timer& other) const;

      std::string to_string() const;

   private:
      std::string result_string_bps() const;
      std::string result_string_ops() const;

      // const data
      std::string m_name, m_doing;
      size_t m_buf_size;
      uint64_t m_event_mult;
      double m_clock_cycle_ratio;
      uint64_t m_clock_speed;

      // set at runtime
      std::string m_custom_msg;
      uint64_t m_time_used = 0, m_timer_start = 0;
      uint64_t m_event_count = 0;

      uint64_t m_max_time = 0, m_min_time = 0;
      uint64_t m_cpu_cycles_start = 0, m_cpu_cycles_used = 0;
   };

}

#endif // BOTAN_AMALGAMATION_INTERNAL_H_
