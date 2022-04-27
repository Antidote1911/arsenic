/*
* Botan 2.19.1 Amalgamation
* (C) 1999-2020 The Botan Authors
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include "botan_all.h"

#include <chrono>
#include <functional>
#include <map>
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

template<typename T>
inline T bit_permute_step(T x, T mask, size_t shift)
   {
   /*
   See https://reflectionsonsecurity.wordpress.com/2014/05/11/efficient-bit-permutation-using-delta-swaps/
   and http://programming.sirrida.de/bit_perm.html
   */
   const T swap = ((x >> shift) ^ x) & mask;
   return (x ^ swap) ^ (swap << shift);
   }

template<typename T>
inline void swap_bits(T& x, T& y, T mask, size_t shift)
   {
   const T swap = ((x >> shift) ^ y) & mask;
   x ^= swap << shift;
   y ^= swap;
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

      static Mask<T> is_within_range(T v, T l, T u)
         {
         //return Mask<T>::is_gte(v, l) & Mask<T>::is_lte(v, u);

         const T v_lt_l = v^((v^l) | ((v-l)^v));
         const T v_gt_u = u^((u^v) | ((u-v)^u));
         const T either = v_lt_l | v_gt_u;
         return ~Mask<T>(expand_top_bit(either));
         }

      static Mask<T> is_any_of(T v, std::initializer_list<T> accepted)
         {
         T accept = 0;

         for(auto a: accepted)
            {
            const T diff = a ^ v;
            const T eq_zero = ~diff & (diff - 1);
            accept |= eq_zero;
            }

         return Mask<T>(expand_top_bit(accept));
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
* Polynomial doubling in GF(2^n)
*/
void BOTAN_TEST_API poly_double_n(uint8_t out[], const uint8_t in[], size_t n);

/**
* Returns true iff poly_double_n is implemented for this size.
*/
inline bool poly_double_supported_size(size_t n)
   {
   return (n == 8 || n == 16 || n == 24 || n == 32 || n == 64 || n == 128);
   }

inline void poly_double_n(uint8_t buf[], size_t n)
   {
   return poly_double_n(buf, buf, n);
   }

/*
* Little endian convention - used for XTS
*/
void BOTAN_TEST_API poly_double_n_le(uint8_t out[], const uint8_t in[], size_t n);

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

template<typename T>
BOTAN_FORCE_INLINE void SBoxE0(T& a, T& b, T& c, T& d)
   {
   d ^= a;
   T t0 = b;
   b &= d;
   t0 ^= c;
   b ^= a;
   a |= d;
   a ^= t0;
   t0 ^= d;
   d ^= c;
   c |= b;
   c ^= t0;
   t0 = ~t0;
   t0 |= b;
   b ^= d;
   b ^= t0;
   d |= a;
   b ^= d;
   t0 ^= d;
   d = a;
   a = b;
   b = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxE1(T& a, T& b, T& c, T& d)
   {
   a = ~a;
   c = ~c;
   T t0 = a;
   a &= b;
   c ^= a;
   a |= d;
   d ^= c;
   b ^= a;
   a ^= t0;
   t0 |= b;
   b ^= d;
   c |= a;
   c &= t0;
   a ^= b;
   b &= c;
   b ^= a;
   a &= c;
   t0 ^= a;
   a = c;
   c = d;
   d = b;
   b = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxE2(T& a, T& b, T& c, T& d)
   {
   T t0 = a;
   a &= c;
   a ^= d;
   c ^= b;
   c ^= a;
   d |= t0;
   d ^= b;
   t0 ^= c;
   b = d;
   d |= t0;
   d ^= a;
   a &= b;
   t0 ^= a;
   b ^= d;
   b ^= t0;
   a = c;
   c = b;
   b = d;
   d = ~t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxE3(T& a, T& b, T& c, T& d)
   {
   T t0 = a;
   a |= d;
   d ^= b;
   b &= t0;
   t0 ^= c;
   c ^= d;
   d &= a;
   t0 |= b;
   d ^= t0;
   a ^= b;
   t0 &= a;
   b ^= d;
   t0 ^= c;
   b |= a;
   b ^= c;
   a ^= d;
   c = b;
   b |= d;
   a ^= b;
   b = c;
   c = d;
   d = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxE4(T& a, T& b, T& c, T& d)
   {
   b ^= d;
   d = ~d;
   c ^= d;
   d ^= a;
   T t0 = b;
   b &= d;
   b ^= c;
   t0 ^= d;
   a ^= t0;
   c &= t0;
   c ^= a;
   a &= b;
   d ^= a;
   t0 |= b;
   t0 ^= a;
   a |= d;
   a ^= c;
   c &= d;
   a = ~a;
   t0 ^= c;
   c = a;
   a = b;
   b = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxE5(T& a, T& b, T& c, T& d)
   {
   a ^= b;
   b ^= d;
   d = ~d;
   T t0 = b;
   b &= a;
   c ^= d;
   b ^= c;
   c |= t0;
   t0 ^= d;
   d &= b;
   d ^= a;
   t0 ^= b;
   t0 ^= c;
   c ^= a;
   a &= d;
   c = ~c;
   a ^= t0;
   t0 |= d;
   t0 ^= c;
   c = a;
   a = b;
   b = d;
   d = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxE6(T& a, T& b, T& c, T& d)
   {
   c = ~c;
   T t0 = d;
   d &= a;
   a ^= t0;
   d ^= c;
   c |= t0;
   b ^= d;
   c ^= a;
   a |= b;
   c ^= b;
   t0 ^= a;
   a |= d;
   a ^= c;
   t0 ^= d;
   t0 ^= a;
   d = ~d;
   c &= t0;
   d ^= c;
   c = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxE7(T& a, T& b, T& c, T& d)
   {
   T t0 = b;
   b |= c;
   b ^= d;
   t0 ^= c;
   c ^= b;
   d |= t0;
   d &= a;
   t0 ^= c;
   d ^= b;
   b |= t0;
   b ^= a;
   a |= t0;
   a ^= c;
   b ^= t0;
   c ^= b;
   b &= a;
   b ^= t0;
   c = ~c;
   c |= a;
   t0 ^= c;
   c = b;
   b = d;
   d = a;
   a = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD0(T& a, T& b, T& c, T& d)
   {
   c = ~c;
   T t0 = b;
   b |= a;
   t0 = ~t0;
   b ^= c;
   c |= t0;
   b ^= d;
   a ^= t0;
   c ^= a;
   a &= d;
   t0 ^= a;
   a |= b;
   a ^= c;
   d ^= t0;
   c ^= b;
   d ^= a;
   d ^= b;
   c &= d;
   t0 ^= c;
   c = b;
   b = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD1(T& a, T& b, T& c, T& d)
   {
   T t0 = b;
   b ^= d;
   d &= b;
   t0 ^= c;
   d ^= a;
   a |= b;
   c ^= d;
   a ^= t0;
   a |= c;
   b ^= d;
   a ^= b;
   b |= d;
   b ^= a;
   t0 = ~t0;
   t0 ^= b;
   b |= a;
   b ^= a;
   b |= t0;
   d ^= b;
   b = a;
   a = t0;
   t0 = c;
   c = d;
   d = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD2(T& a, T& b, T& c, T& d)
   {
   c ^= d;
   d ^= a;
   T t0 = d;
   d &= c;
   d ^= b;
   b |= c;
   b ^= t0;
   t0 &= d;
   c ^= d;
   t0 &= a;
   t0 ^= c;
   c &= b;
   c |= a;
   d = ~d;
   c ^= d;
   a ^= d;
   a &= b;
   d ^= t0;
   d ^= a;
   a = b;
   b = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD3(T& a, T& b, T& c, T& d)
   {
   T t0 = c;
   c ^= b;
   a ^= c;
   t0 &= c;
   t0 ^= a;
   a &= b;
   b ^= d;
   d |= t0;
   c ^= d;
   a ^= d;
   b ^= t0;
   d &= c;
   d ^= b;
   b ^= a;
   b |= c;
   a ^= d;
   b ^= t0;
   a ^= b;
   t0 = a;
   a = c;
   c = d;
   d = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD4(T& a, T& b, T& c, T& d)
   {
   T t0 = c;
   c &= d;
   c ^= b;
   b |= d;
   b &= a;
   t0 ^= c;
   t0 ^= b;
   b &= c;
   a = ~a;
   d ^= t0;
   b ^= d;
   d &= a;
   d ^= c;
   a ^= b;
   c &= a;
   d ^= a;
   c ^= t0;
   c |= d;
   d ^= a;
   c ^= b;
   b = d;
   d = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD5(T& a, T& b, T& c, T& d)
   {
   b = ~b;
   T t0 = d;
   c ^= b;
   d |= a;
   d ^= c;
   c |= b;
   c &= a;
   t0 ^= d;
   c ^= t0;
   t0 |= a;
   t0 ^= b;
   b &= c;
   b ^= d;
   t0 ^= c;
   d &= t0;
   t0 ^= b;
   d ^= t0;
   t0 = ~t0;
   d ^= a;
   a = b;
   b = t0;
   t0 = d;
   d = c;
   c = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD6(T& a, T& b, T& c, T& d)
   {
   a ^= c;
   T t0 = c;
   c &= a;
   t0 ^= d;
   c = ~c;
   d ^= b;
   c ^= d;
   t0 |= a;
   a ^= c;
   d ^= t0;
   t0 ^= b;
   b &= d;
   b ^= a;
   a ^= d;
   a |= c;
   d ^= b;
   t0 ^= a;
   a = b;
   b = c;
   c = t0;
   }

template<typename T>
BOTAN_FORCE_INLINE void SBoxD7(T& a, T& b, T& c, T& d)
   {
   T t0 = c;
   c ^= a;
   a &= d;
   t0 |= d;
   c = ~c;
   d ^= b;
   b |= a;
   a ^= c;
   c &= t0;
   d &= t0;
   b ^= c;
   c ^= a;
   a |= c;
   t0 ^= b;
   a ^= d;
   d ^= t0;
   t0 |= a;
   d ^= c;
   t0 ^= c;
   c = b;
   b = a;
   a = d;
   d = t0;
   }

/*
A bug in Clang 12 and XCode 13 cause a miscompilation of SHA3_round
with optimization levels -O2 and higher.

For details
 https://github.com/randombit/botan/issues/2802 and
 https://bugs.llvm.org/show_bug.cgi?id=51957
*/

#if    defined(__clang__) && \
    (( defined(__apple_build_version__) && __clang_major__ == 13) || \
     (!defined(__apple_build_version__) && __clang_major__ == 12))
  #define BOTAN_HAS_BROKEN_CLANG_SHA3
#endif

#if defined(BOTAN_HAS_BROKEN_CLANG_SHA3)
  #include <tuple>
#endif

namespace Botan {

#if defined(BOTAN_HAS_BROKEN_CLANG_SHA3)

namespace {

__attribute__((noinline)) std::tuple<uint64_t, uint64_t, uint64_t, uint64_t, uint64_t>
   xor_CNs(const uint64_t A[25])
   {
   return std::tuple<uint64_t, uint64_t, uint64_t, uint64_t, uint64_t>(
      A[0] ^ A[5] ^ A[10] ^ A[15] ^ A[20],
      A[1] ^ A[6] ^ A[11] ^ A[16] ^ A[21],
      A[2] ^ A[7] ^ A[12] ^ A[17] ^ A[22],
      A[3] ^ A[8] ^ A[13] ^ A[18] ^ A[23],
      A[4] ^ A[9] ^ A[14] ^ A[19] ^ A[24]);
   }

}

#endif

inline void SHA3_round(uint64_t T[25], const uint64_t A[25], uint64_t RC)
   {
#if defined(BOTAN_HAS_BROKEN_CLANG_SHA3)
   const auto [C0,C1,C2,C3,C4] = xor_CNs(A);
#else
   const uint64_t C0 = A[0] ^ A[5] ^ A[10] ^ A[15] ^ A[20];
   const uint64_t C1 = A[1] ^ A[6] ^ A[11] ^ A[16] ^ A[21];
   const uint64_t C2 = A[2] ^ A[7] ^ A[12] ^ A[17] ^ A[22];
   const uint64_t C3 = A[3] ^ A[8] ^ A[13] ^ A[18] ^ A[23];
   const uint64_t C4 = A[4] ^ A[9] ^ A[14] ^ A[19] ^ A[24];
#endif

   const uint64_t D0 = rotl<1>(C0) ^ C3;
   const uint64_t D1 = rotl<1>(C1) ^ C4;
   const uint64_t D2 = rotl<1>(C2) ^ C0;
   const uint64_t D3 = rotl<1>(C3) ^ C1;
   const uint64_t D4 = rotl<1>(C4) ^ C2;

   const uint64_t B00 =          A[ 0] ^ D1;
   const uint64_t B01 = rotl<44>(A[ 6] ^ D2);
   const uint64_t B02 = rotl<43>(A[12] ^ D3);
   const uint64_t B03 = rotl<21>(A[18] ^ D4);
   const uint64_t B04 = rotl<14>(A[24] ^ D0);
   T[ 0] = B00 ^ (~B01 & B02) ^ RC;
   T[ 1] = B01 ^ (~B02 & B03);
   T[ 2] = B02 ^ (~B03 & B04);
   T[ 3] = B03 ^ (~B04 & B00);
   T[ 4] = B04 ^ (~B00 & B01);

   const uint64_t B05 = rotl<28>(A[ 3] ^ D4);
   const uint64_t B06 = rotl<20>(A[ 9] ^ D0);
   const uint64_t B07 = rotl< 3>(A[10] ^ D1);
   const uint64_t B08 = rotl<45>(A[16] ^ D2);
   const uint64_t B09 = rotl<61>(A[22] ^ D3);
   T[ 5] = B05 ^ (~B06 & B07);
   T[ 6] = B06 ^ (~B07 & B08);
   T[ 7] = B07 ^ (~B08 & B09);
   T[ 8] = B08 ^ (~B09 & B05);
   T[ 9] = B09 ^ (~B05 & B06);

   const uint64_t B10 = rotl< 1>(A[ 1] ^ D2);
   const uint64_t B11 = rotl< 6>(A[ 7] ^ D3);
   const uint64_t B12 = rotl<25>(A[13] ^ D4);
   const uint64_t B13 = rotl< 8>(A[19] ^ D0);
   const uint64_t B14 = rotl<18>(A[20] ^ D1);
   T[10] = B10 ^ (~B11 & B12);
   T[11] = B11 ^ (~B12 & B13);
   T[12] = B12 ^ (~B13 & B14);
   T[13] = B13 ^ (~B14 & B10);
   T[14] = B14 ^ (~B10 & B11);

   const uint64_t B15 = rotl<27>(A[ 4] ^ D0);
   const uint64_t B16 = rotl<36>(A[ 5] ^ D1);
   const uint64_t B17 = rotl<10>(A[11] ^ D2);
   const uint64_t B18 = rotl<15>(A[17] ^ D3);
   const uint64_t B19 = rotl<56>(A[23] ^ D4);
   T[15] = B15 ^ (~B16 & B17);
   T[16] = B16 ^ (~B17 & B18);
   T[17] = B17 ^ (~B18 & B19);
   T[18] = B18 ^ (~B19 & B15);
   T[19] = B19 ^ (~B15 & B16);

   const uint64_t B20 = rotl<62>(A[ 2] ^ D3);
   const uint64_t B21 = rotl<55>(A[ 8] ^ D4);
   const uint64_t B22 = rotl<39>(A[14] ^ D0);
   const uint64_t B23 = rotl<41>(A[15] ^ D1);
   const uint64_t B24 = rotl< 2>(A[21] ^ D2);
   T[20] = B20 ^ (~B21 & B22);
   T[21] = B21 ^ (~B22 & B23);
   T[22] = B22 ^ (~B23 & B24);
   T[23] = B23 ^ (~B24 & B20);
   T[24] = B24 ^ (~B20 & B21);
   }

}

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
  #define BOTAN_CLMUL_ISA "crypto"
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
      BOTAN_FORCE_INLINE SIMD_8x32()
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

#if defined(__AVX512VL__)
         return SIMD_8x32(_mm256_rol_epi32(m_avx2, ROT));
#else
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
#endif
         }

      template<size_t ROT>
      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 rotr() const
         {
         return this->rotl<32-ROT>();
         }

      template<size_t ROT1, size_t ROT2, size_t ROT3>
      SIMD_8x32 BOTAN_FUNC_ISA("avx2") rho() const
         {
         SIMD_8x32 res;

         const SIMD_8x32 rot1 = this->rotr<ROT1>();
         const SIMD_8x32 rot2 = this->rotr<ROT2>();
         const SIMD_8x32 rot3 = this->rotr<ROT3>();

         return rot1 ^ rot2 ^ rot3;
         }

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 operator+(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval += other;
         return retval;
         }

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 operator-(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval -= other;
         return retval;
         }

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 operator^(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval ^= other;
         return retval;
         }

      BOTAN_FUNC_ISA("avx2")
      SIMD_8x32 operator|(const SIMD_8x32& other) const
         {
         SIMD_8x32 retval(*this);
         retval |= other;
         return retval;
         }

      BOTAN_FUNC_ISA("avx2")
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

      template<int SHIFT> BOTAN_FUNC_ISA("avx2") SIMD_8x32 shr() const
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
/*
* Adler32
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

void adler32_update(const uint8_t input[], size_t length,
                    uint16_t& S1, uint16_t& S2)
   {
   uint32_t S1x = S1;
   uint32_t S2x = S2;

   while(length >= 16)
      {
      S1x += input[ 0]; S2x += S1x;
      S1x += input[ 1]; S2x += S1x;
      S1x += input[ 2]; S2x += S1x;
      S1x += input[ 3]; S2x += S1x;
      S1x += input[ 4]; S2x += S1x;
      S1x += input[ 5]; S2x += S1x;
      S1x += input[ 6]; S2x += S1x;
      S1x += input[ 7]; S2x += S1x;
      S1x += input[ 8]; S2x += S1x;
      S1x += input[ 9]; S2x += S1x;
      S1x += input[10]; S2x += S1x;
      S1x += input[11]; S2x += S1x;
      S1x += input[12]; S2x += S1x;
      S1x += input[13]; S2x += S1x;
      S1x += input[14]; S2x += S1x;
      S1x += input[15]; S2x += S1x;
      input += 16;
      length -= 16;
      }

   for(size_t j = 0; j != length; ++j)
      {
      S1x += input[j];
      S2x += S1x;
      }

   S1 = S1x % 65521;
   S2 = S2x % 65521;
   }

}

/*
* Update an Adler32 Checksum
*/
void Adler32::add_data(const uint8_t input[], size_t length)
   {
   const size_t PROCESS_AMOUNT = 5552;

   while(length >= PROCESS_AMOUNT)
      {
      adler32_update(input, PROCESS_AMOUNT, m_S1, m_S2);
      input += PROCESS_AMOUNT;
      length -= PROCESS_AMOUNT;
      }

   adler32_update(input, length, m_S1, m_S2);
   }

/*
* Finalize an Adler32 Checksum
*/
void Adler32::final_result(uint8_t output[])
   {
   store_be(output, m_S2, m_S1);
   clear();
   }

std::unique_ptr<HashFunction> Adler32::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new Adler32(*this));
   }

}
/*
* (C) 2013,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <sstream>

#if defined(BOTAN_HAS_BLOCK_CIPHER)
#endif

#if defined(BOTAN_HAS_AEAD_CCM)
#endif

#if defined(BOTAN_HAS_AEAD_CHACHA20_POLY1305)
#endif

#if defined(BOTAN_HAS_AEAD_EAX)
#endif

#if defined(BOTAN_HAS_AEAD_GCM)
#endif

#if defined(BOTAN_HAS_AEAD_OCB)
#endif

#if defined(BOTAN_HAS_AEAD_SIV)
#endif

namespace Botan {

void AEAD_Mode::set_associated_data_n(size_t i, const uint8_t ad[], size_t ad_len)
   {
   if(i == 0)
      this->set_associated_data(ad, ad_len);
   else
      throw Invalid_Argument("AEAD '" + name() + "' does not support multiple associated data");
   }

std::unique_ptr<AEAD_Mode> AEAD_Mode::create_or_throw(const std::string& algo,
                                                      Cipher_Dir dir,
                                                      const std::string& provider)
   {
   if(auto aead = AEAD_Mode::create(algo, dir, provider))
      return aead;

   throw Lookup_Error("AEAD", algo, provider);
   }

std::unique_ptr<AEAD_Mode> AEAD_Mode::create(const std::string& algo,
                                             Cipher_Dir dir,
                                             const std::string& provider)
   {
   BOTAN_UNUSED(provider);
#if defined(BOTAN_HAS_AEAD_CHACHA20_POLY1305)
   if(algo == "ChaCha20Poly1305")
      {
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new ChaCha20Poly1305_Encryption);
      else
         return std::unique_ptr<AEAD_Mode>(new ChaCha20Poly1305_Decryption);

      }
#endif

   if(algo.find('/') != std::string::npos)
      {
      const std::vector<std::string> algo_parts = split_on(algo, '/');
      const std::string cipher_name = algo_parts[0];
      const std::vector<std::string> mode_info = parse_algorithm_name(algo_parts[1]);

      if(mode_info.empty())
         return std::unique_ptr<AEAD_Mode>();

      std::ostringstream alg_args;

      alg_args << '(' << cipher_name;
      for(size_t i = 1; i < mode_info.size(); ++i)
         alg_args << ',' << mode_info[i];
      for(size_t i = 2; i < algo_parts.size(); ++i)
         alg_args << ',' << algo_parts[i];
      alg_args << ')';

      const std::string mode_name = mode_info[0] + alg_args.str();
      return AEAD_Mode::create(mode_name, dir);
      }

#if defined(BOTAN_HAS_BLOCK_CIPHER)

   SCAN_Name req(algo);

   if(req.arg_count() == 0)
      {
      return std::unique_ptr<AEAD_Mode>();
      }

   std::unique_ptr<BlockCipher> bc(BlockCipher::create(req.arg(0), provider));

   if(!bc)
      {
      return std::unique_ptr<AEAD_Mode>();
      }

#if defined(BOTAN_HAS_AEAD_CCM)
   if(req.algo_name() == "CCM")
      {
      size_t tag_len = req.arg_as_integer(1, 16);
      size_t L_len = req.arg_as_integer(2, 3);
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new CCM_Encryption(bc.release(), tag_len, L_len));
      else
         return std::unique_ptr<AEAD_Mode>(new CCM_Decryption(bc.release(), tag_len, L_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_GCM)
   if(req.algo_name() == "GCM")
      {
      size_t tag_len = req.arg_as_integer(1, 16);
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new GCM_Encryption(bc.release(), tag_len));
      else
         return std::unique_ptr<AEAD_Mode>(new GCM_Decryption(bc.release(), tag_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_OCB)
   if(req.algo_name() == "OCB")
      {
      size_t tag_len = req.arg_as_integer(1, 16);
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new OCB_Encryption(bc.release(), tag_len));
      else
         return std::unique_ptr<AEAD_Mode>(new OCB_Decryption(bc.release(), tag_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_EAX)
   if(req.algo_name() == "EAX")
      {
      size_t tag_len = req.arg_as_integer(1, bc->block_size());
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new EAX_Encryption(bc.release(), tag_len));
      else
         return std::unique_ptr<AEAD_Mode>(new EAX_Decryption(bc.release(), tag_len));
      }
#endif

#if defined(BOTAN_HAS_AEAD_SIV)
   if(req.algo_name() == "SIV")
      {
      if(dir == ENCRYPTION)
         return std::unique_ptr<AEAD_Mode>(new SIV_Encryption(bc.release()));
      else
         return std::unique_ptr<AEAD_Mode>(new SIV_Decryption(bc.release()));
      }
#endif

#endif

   return std::unique_ptr<AEAD_Mode>();
   }



}
/*
* (C) 1999-2010,2015,2017,2018,2020 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

#if defined(BOTAN_HAS_AES_POWER8) || defined(BOTAN_HAS_AES_ARMV8) || defined(BOTAN_HAS_AES_NI)
   #define BOTAN_HAS_HW_AES_SUPPORT
#endif

/*
* One of three AES implementation strategies are used to get a constant time
* implementation which is immune to common cache/timing based side channels:
*
* - If AES hardware support is available (AES-NI, POWER8, Aarch64) use that
*
* - If 128-bit SIMD with byte shuffles are available (SSSE3, NEON, or Altivec),
*   use the vperm technique published by Mike Hamburg at CHES 2009.
*
* - If no hardware or SIMD support, fall back to a constant time bitsliced
*   implementation. This uses 32-bit words resulting in 2 blocks being processed
*   in parallel. Moving to 4 blocks (with 64-bit words) would approximately
*   double performance on 64-bit CPUs. Likewise moving to 128 bit SIMD would
*   again approximately double performance vs 64-bit. However the assumption is
*   that most 64-bit CPUs either have hardware AES or SIMD shuffle support and
*   that the majority of users falling back to this code will be 32-bit cores.
*   If this assumption proves to be unsound, the bitsliced code can easily be
*   extended to operate on either 32 or 64 bit words depending on the native
*   wordsize of the target processor.
*
* Useful references
*
* - "Accelerating AES with Vector Permute Instructions" Mike Hamburg
*   https://www.shiftleft.org/papers/vector_aes/vector_aes.pdf
*
* - "Faster and Timing-Attack Resistant AES-GCM" Käsper and Schwabe
*   https://eprint.iacr.org/2009/129.pdf
*
* - "A new combinational logic minimization technique with applications to cryptology."
*   Boyar and Peralta https://eprint.iacr.org/2009/191.pdf
*
* - "A depth-16 circuit for the AES S-box" Boyar and Peralta
*    https://eprint.iacr.org/2011/332.pdf
*
* - "A Very Compact S-box for AES" Canright
*   https://www.iacr.org/archive/ches2005/032.pdf
*   https://core.ac.uk/download/pdf/36694529.pdf (extended)
*/

namespace {

/*
This is an AES sbox circuit which can execute in bitsliced mode up to 32x in
parallel.

The circuit is from the "Circuit Minimization Team" group
http://www.cs.yale.edu/homes/peralta/CircuitStuff/CMT.html
http://www.cs.yale.edu/homes/peralta/CircuitStuff/SLP_AES_113.txt

This circuit has size 113 and depth 27. In software it is much faster than
circuits which are considered faster for hardware purposes (where circuit depth
is the critical constraint), because unlike in hardware, on common CPUs we can
only execute - at best - 3 or 4 logic operations per cycle. So a smaller circuit
is superior. On an x86-64 machine this circuit is about 15% faster than the
circuit of size 128 and depth 16 given in "A depth-16 circuit for the AES S-box".

Another circuit for AES Sbox of size 102 and depth 24 is describted in "New
Circuit Minimization Techniques for Smaller and Faster AES SBoxes"
[https://eprint.iacr.org/2019/802] however it relies on "non-standard" gates
like MUX, NOR, NAND, etc and so in practice in bitsliced software, its size is
actually a bit larger than this circuit, as few CPUs have such instructions and
otherwise they must be emulated using a sequence of available bit operations.
*/
void AES_SBOX(uint32_t V[8])
   {
   const uint32_t U0 = V[0];
   const uint32_t U1 = V[1];
   const uint32_t U2 = V[2];
   const uint32_t U3 = V[3];
   const uint32_t U4 = V[4];
   const uint32_t U5 = V[5];
   const uint32_t U6 = V[6];
   const uint32_t U7 = V[7];

   const uint32_t y14 = U3 ^ U5;
   const uint32_t y13 = U0 ^ U6;
   const uint32_t y9 = U0 ^ U3;
   const uint32_t y8 = U0 ^ U5;
   const uint32_t t0 = U1 ^ U2;
   const uint32_t y1 = t0 ^ U7;
   const uint32_t y4 = y1 ^ U3;
   const uint32_t y12 = y13 ^ y14;
   const uint32_t y2 = y1 ^ U0;
   const uint32_t y5 = y1 ^ U6;
   const uint32_t y3 = y5 ^ y8;
   const uint32_t t1 = U4 ^ y12;
   const uint32_t y15 = t1 ^ U5;
   const uint32_t y20 = t1 ^ U1;
   const uint32_t y6 = y15 ^ U7;
   const uint32_t y10 = y15 ^ t0;
   const uint32_t y11 = y20 ^ y9;
   const uint32_t y7 = U7 ^ y11;
   const uint32_t y17 = y10 ^ y11;
   const uint32_t y19 = y10 ^ y8;
   const uint32_t y16 = t0 ^ y11;
   const uint32_t y21 = y13 ^ y16;
   const uint32_t y18 = U0 ^ y16;
   const uint32_t t2 = y12 & y15;
   const uint32_t t3 = y3 & y6;
   const uint32_t t4 = t3 ^ t2;
   const uint32_t t5 = y4 & U7;
   const uint32_t t6 = t5 ^ t2;
   const uint32_t t7 = y13 & y16;
   const uint32_t t8 = y5 & y1;
   const uint32_t t9 = t8 ^ t7;
   const uint32_t t10 = y2 & y7;
   const uint32_t t11 = t10 ^ t7;
   const uint32_t t12 = y9 & y11;
   const uint32_t t13 = y14 & y17;
   const uint32_t t14 = t13 ^ t12;
   const uint32_t t15 = y8 & y10;
   const uint32_t t16 = t15 ^ t12;
   const uint32_t t17 = t4 ^ y20;
   const uint32_t t18 = t6 ^ t16;
   const uint32_t t19 = t9 ^ t14;
   const uint32_t t20 = t11 ^ t16;
   const uint32_t t21 = t17 ^ t14;
   const uint32_t t22 = t18 ^ y19;
   const uint32_t t23 = t19 ^ y21;
   const uint32_t t24 = t20 ^ y18;
   const uint32_t t25 = t21 ^ t22;
   const uint32_t t26 = t21 & t23;
   const uint32_t t27 = t24 ^ t26;
   const uint32_t t28 = t25 & t27;
   const uint32_t t29 = t28 ^ t22;
   const uint32_t t30 = t23 ^ t24;
   const uint32_t t31 = t22 ^ t26;
   const uint32_t t32 = t31 & t30;
   const uint32_t t33 = t32 ^ t24;
   const uint32_t t34 = t23 ^ t33;
   const uint32_t t35 = t27 ^ t33;
   const uint32_t t36 = t24 & t35;
   const uint32_t t37 = t36 ^ t34;
   const uint32_t t38 = t27 ^ t36;
   const uint32_t t39 = t29 & t38;
   const uint32_t t40 = t25 ^ t39;
   const uint32_t t41 = t40 ^ t37;
   const uint32_t t42 = t29 ^ t33;
   const uint32_t t43 = t29 ^ t40;
   const uint32_t t44 = t33 ^ t37;
   const uint32_t t45 = t42 ^ t41;
   const uint32_t z0 = t44 & y15;
   const uint32_t z1 = t37 & y6;
   const uint32_t z2 = t33 & U7;
   const uint32_t z3 = t43 & y16;
   const uint32_t z4 = t40 & y1;
   const uint32_t z5 = t29 & y7;
   const uint32_t z6 = t42 & y11;
   const uint32_t z7 = t45 & y17;
   const uint32_t z8 = t41 & y10;
   const uint32_t z9 = t44 & y12;
   const uint32_t z10 = t37 & y3;
   const uint32_t z11 = t33 & y4;
   const uint32_t z12 = t43 & y13;
   const uint32_t z13 = t40 & y5;
   const uint32_t z14 = t29 & y2;
   const uint32_t z15 = t42 & y9;
   const uint32_t z16 = t45 & y14;
   const uint32_t z17 = t41 & y8;
   const uint32_t tc1 = z15 ^ z16;
   const uint32_t tc2 = z10 ^ tc1;
   const uint32_t tc3 = z9 ^ tc2;
   const uint32_t tc4 = z0 ^ z2;
   const uint32_t tc5 = z1 ^ z0;
   const uint32_t tc6 = z3 ^ z4;
   const uint32_t tc7 = z12 ^ tc4;
   const uint32_t tc8 = z7 ^ tc6;
   const uint32_t tc9 = z8 ^ tc7;
   const uint32_t tc10 = tc8 ^ tc9;
   const uint32_t tc11 = tc6 ^ tc5;
   const uint32_t tc12 = z3 ^ z5;
   const uint32_t tc13 = z13 ^ tc1;
   const uint32_t tc14 = tc4 ^ tc12;
   const uint32_t S3 = tc3 ^ tc11;
   const uint32_t tc16 = z6 ^ tc8;
   const uint32_t tc17 = z14 ^ tc10;
   const uint32_t tc18 = ~tc13 ^ tc14;
   const uint32_t S7 = z12 ^ tc18;
   const uint32_t tc20 = z15 ^ tc16;
   const uint32_t tc21 = tc2 ^ z11;
   const uint32_t S0 = tc3 ^ tc16;
   const uint32_t S6 = tc10 ^ tc18;
   const uint32_t S4 = tc14 ^ S3;
   const uint32_t S1 = ~(S3 ^ tc16);
   const uint32_t tc26 = tc17 ^ tc20;
   const uint32_t S2 = ~(tc26 ^ z17);
   const uint32_t S5 = tc21 ^ tc17;

   V[0] = S0;
   V[1] = S1;
   V[2] = S2;
   V[3] = S3;
   V[4] = S4;
   V[5] = S5;
   V[6] = S6;
   V[7] = S7;
   }

/*
A circuit for inverse AES Sbox of size 121 and depth 21 from
http://www.cs.yale.edu/homes/peralta/CircuitStuff/CMT.html
http://www.cs.yale.edu/homes/peralta/CircuitStuff/Sinv.txt
*/
void AES_INV_SBOX(uint32_t V[8])
   {
   const uint32_t U0 = V[0];
   const uint32_t U1 = V[1];
   const uint32_t U2 = V[2];
   const uint32_t U3 = V[3];
   const uint32_t U4 = V[4];
   const uint32_t U5 = V[5];
   const uint32_t U6 = V[6];
   const uint32_t U7 = V[7];

   const uint32_t Y0 = U0 ^ U3;
   const uint32_t Y2 = ~(U1 ^ U3);
   const uint32_t Y4 = U0 ^ Y2;
   const uint32_t RTL0 = U6 ^ U7;
   const uint32_t Y1 = Y2 ^ RTL0;
   const uint32_t Y7 = ~(U2 ^ Y1);
   const uint32_t RTL1 = U3 ^ U4;
   const uint32_t Y6 = ~(U7 ^ RTL1);
   const uint32_t Y3 = Y1 ^ RTL1;
   const uint32_t RTL2 = ~(U0 ^ U2);
   const uint32_t Y5 = U5 ^ RTL2;
   const uint32_t sa1 = Y0 ^ Y2;
   const uint32_t sa0 = Y1 ^ Y3;
   const uint32_t sb1 = Y4 ^ Y6;
   const uint32_t sb0 = Y5 ^ Y7;
   const uint32_t ah = Y0 ^ Y1;
   const uint32_t al = Y2 ^ Y3;
   const uint32_t aa = sa0 ^ sa1;
   const uint32_t bh = Y4 ^ Y5;
   const uint32_t bl = Y6 ^ Y7;
   const uint32_t bb = sb0 ^ sb1;
   const uint32_t ab20 = sa0 ^ sb0;
   const uint32_t ab22 = al ^ bl;
   const uint32_t ab23 = Y3 ^ Y7;
   const uint32_t ab21 = sa1 ^ sb1;
   const uint32_t abcd1 = ah & bh;
   const uint32_t rr1 = Y0 & Y4;
   const uint32_t ph11 = ab20 ^ abcd1;
   const uint32_t t01 = Y1 & Y5;
   const uint32_t ph01 = t01 ^ abcd1;
   const uint32_t abcd2 = al & bl;
   const uint32_t r1 = Y2 & Y6;
   const uint32_t pl11 = ab22 ^ abcd2;
   const uint32_t r2 = Y3 & Y7;
   const uint32_t pl01 = r2 ^ abcd2;
   const uint32_t r3 = sa0 & sb0;
   const uint32_t vr1 = aa & bb;
   const uint32_t pr1 = vr1 ^ r3;
   const uint32_t wr1 = sa1 & sb1;
   const uint32_t qr1 = wr1 ^ r3;
   const uint32_t ab0 = ph11 ^ rr1;
   const uint32_t ab1 = ph01 ^ ab21;
   const uint32_t ab2 = pl11 ^ r1;
   const uint32_t ab3 = pl01 ^ qr1;
   const uint32_t cp1 = ab0 ^ pr1;
   const uint32_t cp2 = ab1 ^ qr1;
   const uint32_t cp3 = ab2 ^ pr1;
   const uint32_t cp4 = ab3 ^ ab23;
   const uint32_t tinv1 = cp3 ^ cp4;
   const uint32_t tinv2 = cp3 & cp1;
   const uint32_t tinv3 = cp2 ^ tinv2;
   const uint32_t tinv4 = cp1 ^ cp2;
   const uint32_t tinv5 = cp4 ^ tinv2;
   const uint32_t tinv6 = tinv5 & tinv4;
   const uint32_t tinv7 = tinv3 & tinv1;
   const uint32_t d2 = cp4 ^ tinv7;
   const uint32_t d0 = cp2 ^ tinv6;
   const uint32_t tinv8 = cp1 & cp4;
   const uint32_t tinv9 = tinv4 & tinv8;
   const uint32_t tinv10 = tinv4 ^ tinv2;
   const uint32_t d1 = tinv9 ^ tinv10;
   const uint32_t tinv11 = cp2 & cp3;
   const uint32_t tinv12 = tinv1 & tinv11;
   const uint32_t tinv13 = tinv1 ^ tinv2;
   const uint32_t d3 = tinv12 ^ tinv13;
   const uint32_t sd1 = d1 ^ d3;
   const uint32_t sd0 = d0 ^ d2;
   const uint32_t dl = d0 ^ d1;
   const uint32_t dh = d2 ^ d3;
   const uint32_t dd = sd0 ^ sd1;
   const uint32_t abcd3 = dh & bh;
   const uint32_t rr2 = d3 & Y4;
   const uint32_t t02 = d2 & Y5;
   const uint32_t abcd4 = dl & bl;
   const uint32_t r4 = d1 & Y6;
   const uint32_t r5 = d0 & Y7;
   const uint32_t r6 = sd0 & sb0;
   const uint32_t vr2 = dd & bb;
   const uint32_t wr2 = sd1 & sb1;
   const uint32_t abcd5 = dh & ah;
   const uint32_t r7 = d3 & Y0;
   const uint32_t r8 = d2 & Y1;
   const uint32_t abcd6 = dl & al;
   const uint32_t r9 = d1 & Y2;
   const uint32_t r10 = d0 & Y3;
   const uint32_t r11 = sd0 & sa0;
   const uint32_t vr3 = dd & aa;
   const uint32_t wr3 = sd1 & sa1;
   const uint32_t ph12 = rr2 ^ abcd3;
   const uint32_t ph02 = t02 ^ abcd3;
   const uint32_t pl12 = r4 ^ abcd4;
   const uint32_t pl02 = r5 ^ abcd4;
   const uint32_t pr2 = vr2 ^ r6;
   const uint32_t qr2 = wr2 ^ r6;
   const uint32_t p0 = ph12 ^ pr2;
   const uint32_t p1 = ph02 ^ qr2;
   const uint32_t p2 = pl12 ^ pr2;
   const uint32_t p3 = pl02 ^ qr2;
   const uint32_t ph13 = r7 ^ abcd5;
   const uint32_t ph03 = r8 ^ abcd5;
   const uint32_t pl13 = r9 ^ abcd6;
   const uint32_t pl03 = r10 ^ abcd6;
   const uint32_t pr3 = vr3 ^ r11;
   const uint32_t qr3 = wr3 ^ r11;
   const uint32_t p4 = ph13 ^ pr3;
   const uint32_t S7 = ph03 ^ qr3;
   const uint32_t p6 = pl13 ^ pr3;
   const uint32_t p7 = pl03 ^ qr3;
   const uint32_t S3 = p1 ^ p6;
   const uint32_t S6 = p2 ^ p6;
   const uint32_t S0 = p3 ^ p6;
   const uint32_t X11 = p0 ^ p2;
   const uint32_t S5 = S0 ^ X11;
   const uint32_t X13 = p4 ^ p7;
   const uint32_t X14 = X11 ^ X13;
   const uint32_t S1 = S3 ^ X14;
   const uint32_t X16 = p1 ^ S7;
   const uint32_t S2 = X14 ^ X16;
   const uint32_t X18 = p0 ^ p4;
   const uint32_t X19 = S5 ^ X16;
   const uint32_t S4 = X18 ^ X19;

   V[0] = S0;
   V[1] = S1;
   V[2] = S2;
   V[3] = S3;
   V[4] = S4;
   V[5] = S5;
   V[6] = S6;
   V[7] = S7;
   }

inline void bit_transpose(uint32_t B[8])
   {
   swap_bits<uint32_t>(B[1], B[0], 0x55555555, 1);
   swap_bits<uint32_t>(B[3], B[2], 0x55555555, 1);
   swap_bits<uint32_t>(B[5], B[4], 0x55555555, 1);
   swap_bits<uint32_t>(B[7], B[6], 0x55555555, 1);

   swap_bits<uint32_t>(B[2], B[0], 0x33333333, 2);
   swap_bits<uint32_t>(B[3], B[1], 0x33333333, 2);
   swap_bits<uint32_t>(B[6], B[4], 0x33333333, 2);
   swap_bits<uint32_t>(B[7], B[5], 0x33333333, 2);

   swap_bits<uint32_t>(B[4], B[0], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[5], B[1], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[6], B[2], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[7], B[3], 0x0F0F0F0F, 4);
   }

inline void ks_expand(uint32_t B[8], const uint32_t K[], size_t r)
   {
   /*
   This is bit_transpose of K[r..r+4] || K[r..r+4], we can save some computation
   due to knowing the first and second halves are the same data.
   */
   for(size_t i = 0; i != 4; ++i)
      B[i] = K[r + i];

   swap_bits<uint32_t>(B[1], B[0], 0x55555555, 1);
   swap_bits<uint32_t>(B[3], B[2], 0x55555555, 1);

   swap_bits<uint32_t>(B[2], B[0], 0x33333333, 2);
   swap_bits<uint32_t>(B[3], B[1], 0x33333333, 2);

   B[4] = B[0];
   B[5] = B[1];
   B[6] = B[2];
   B[7] = B[3];

   swap_bits<uint32_t>(B[4], B[0], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[5], B[1], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[6], B[2], 0x0F0F0F0F, 4);
   swap_bits<uint32_t>(B[7], B[3], 0x0F0F0F0F, 4);
   }

inline void shift_rows(uint32_t B[8])
   {
   // 3 0 1 2 7 4 5 6 10 11 8 9 14 15 12 13 17 18 19 16 21 22 23 20 24 25 26 27 28 29 30 31
#if defined(BOTAN_TARGET_CPU_HAS_NATIVE_64BIT)
   for(size_t i = 0; i != 8; i += 2)
      {
      uint64_t x = (static_cast<uint64_t>(B[i]) << 32) | B[i+1];
      x = bit_permute_step<uint64_t>(x, 0x0022331100223311, 2);
      x = bit_permute_step<uint64_t>(x, 0x0055005500550055, 1);
      B[i] = static_cast<uint32_t>(x >> 32);
      B[i+1] = static_cast<uint32_t>(x);
      }
#else
   for(size_t i = 0; i != 8; ++i)
      {
      uint32_t x = B[i];
      x = bit_permute_step<uint32_t>(x, 0x00223311, 2);
      x = bit_permute_step<uint32_t>(x, 0x00550055, 1);
      B[i] = x;
      }
#endif
   }

inline void inv_shift_rows(uint32_t B[8])
   {
   // Inverse of shift_rows, just inverting the steps

#if defined(BOTAN_TARGET_CPU_HAS_NATIVE_64BIT)
   for(size_t i = 0; i != 8; i += 2)
      {
      uint64_t x = (static_cast<uint64_t>(B[i]) << 32) | B[i+1];
      x = bit_permute_step<uint64_t>(x, 0x0055005500550055, 1);
      x = bit_permute_step<uint64_t>(x, 0x0022331100223311, 2);
      B[i] = static_cast<uint32_t>(x >> 32);
      B[i+1] = static_cast<uint32_t>(x);
      }
#else
   for(size_t i = 0; i != 8; ++i)
      {
      uint32_t x = B[i];
      x = bit_permute_step<uint32_t>(x, 0x00550055, 1);
      x = bit_permute_step<uint32_t>(x, 0x00223311, 2);
      B[i] = x;
      }
#endif
   }

inline void mix_columns(uint32_t B[8])
   {
   // carry high bits in B[0] to positions in 0x1b == 0b11011
   const uint32_t X2[8] = {
      B[1],
      B[2],
      B[3],
      B[4] ^ B[0],
      B[5] ^ B[0],
      B[6],
      B[7] ^ B[0],
      B[0],
   };

   for(size_t i = 0; i != 8; i++)
      {
      const uint32_t X3 = B[i] ^ X2[i];
      B[i] = X2[i] ^ rotr<8>(B[i]) ^ rotr<16>(B[i]) ^ rotr<24>(X3);
      }
   }

void inv_mix_columns(uint32_t B[8])
   {
   /*
   OpenSSL's bsaes implementation credits Jussi Kivilinna with the lovely
   matrix decomposition

   | 0e 0b 0d 09 |   | 02 03 01 01 |   | 05 00 04 00 |
   | 09 0e 0b 0d | = | 01 02 03 01 | x | 00 05 00 04 |
   | 0d 09 0e 0b |   | 01 01 02 03 |   | 04 00 05 00 |
   | 0b 0d 09 0e |   | 03 01 01 02 |   | 00 04 00 05 |

   Notice the first component is simply the MixColumns matrix. So we can
   multiply first by (05,00,04,00) then perform MixColumns to get the equivalent
   of InvMixColumn.
   */
   const uint32_t X4[8] = {
      B[2],
      B[3],
      B[4] ^ B[0],
      B[5] ^ B[0] ^ B[1],
      B[6] ^ B[1],
      B[7] ^ B[0],
      B[0] ^ B[1],
      B[1],
   };

   for(size_t i = 0; i != 8; i++)
      {
      const uint32_t X5 = X4[i] ^ B[i];
      B[i] = X5 ^ rotr<16>(X4[i]);
      }

   mix_columns(B);
   }

/*
* AES Encryption
*/
void aes_encrypt_n(const uint8_t in[], uint8_t out[],
                   size_t blocks,
                   const secure_vector<uint32_t>& EK)
   {
   BOTAN_ASSERT(EK.size() == 44 || EK.size() == 52 || EK.size() == 60, "Key was set");

   const size_t rounds = (EK.size() - 4) / 4;

   uint32_t KS[13*8] = { 0 }; // actual maximum is (rounds - 1) * 8
   for(size_t i = 0; i < rounds - 1; i += 1)
      {
      ks_expand(&KS[8*i], EK.data(), 4*i + 4);
      }

   const size_t BLOCK_SIZE = 16;
   const size_t BITSLICED_BLOCKS = 8*sizeof(uint32_t) / BLOCK_SIZE;

   while(blocks > 0)
      {
      const size_t this_loop = std::min(blocks, BITSLICED_BLOCKS);

      uint32_t B[8] = { 0 };

      load_be(B, in, this_loop*4);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= EK[i % 4];

      bit_transpose(B);

      for(size_t r = 0; r != rounds - 1; ++r)
         {
         AES_SBOX(B);
         shift_rows(B);
         mix_columns(B);

         for(size_t i = 0; i != 8; ++i)
            B[i] ^= KS[8*r + i];
         }

      // Final round:
      AES_SBOX(B);
      shift_rows(B);
      bit_transpose(B);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= EK[4*rounds + i % 4];

      copy_out_be(out, this_loop*4*sizeof(uint32_t), B);

      in += this_loop * BLOCK_SIZE;
      out += this_loop * BLOCK_SIZE;
      blocks -= this_loop;
      }
   }

/*
* AES Decryption
*/
void aes_decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks,
                   const secure_vector<uint32_t>& DK)
   {
   BOTAN_ASSERT(DK.size() == 44 || DK.size() == 52 || DK.size() == 60, "Key was set");

   const size_t rounds = (DK.size() - 4) / 4;

   uint32_t KS[13*8] = { 0 }; // actual maximum is (rounds - 1) * 8
   for(size_t i = 0; i < rounds - 1; i += 1)
      {
      ks_expand(&KS[8*i], DK.data(), 4*i + 4);
      }

   const size_t BLOCK_SIZE = 16;
   const size_t BITSLICED_BLOCKS = 8*sizeof(uint32_t) / BLOCK_SIZE;

   while(blocks > 0)
      {
      const size_t this_loop = std::min(blocks, BITSLICED_BLOCKS);

      uint32_t B[8] = { 0 };

      load_be(B, in, this_loop*4);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= DK[i % 4];

      bit_transpose(B);

      for(size_t r = 0; r != rounds - 1; ++r)
         {
         AES_INV_SBOX(B);
         inv_shift_rows(B);
         inv_mix_columns(B);

         for(size_t i = 0; i != 8; ++i)
            B[i] ^= KS[8*r + i];
         }

      // Final round:
      AES_INV_SBOX(B);
      inv_shift_rows(B);
      bit_transpose(B);

      for(size_t i = 0; i != 8; ++i)
         B[i] ^= DK[4*rounds + i % 4];

      copy_out_be(out, this_loop*4*sizeof(uint32_t), B);

      in += this_loop * BLOCK_SIZE;
      out += this_loop * BLOCK_SIZE;
      blocks -= this_loop;
      }
   }

inline uint32_t xtime32(uint32_t s)
   {
   const uint32_t lo_bit = 0x01010101;
   const uint32_t mask = 0x7F7F7F7F;
   const uint32_t poly = 0x1B;

   return ((s & mask) << 1) ^ (((s >> 7) & lo_bit) * poly);
   }

inline uint32_t InvMixColumn(uint32_t s1)
   {
   const uint32_t s2 = xtime32(s1);
   const uint32_t s4 = xtime32(s2);
   const uint32_t s8 = xtime32(s4);
   const uint32_t s9 = s8 ^ s1;
   const uint32_t s11 = s9 ^ s2;
   const uint32_t s13 = s9 ^ s4;
   const uint32_t s14 = s8 ^ s4 ^ s2;

   return s14 ^ rotr<8>(s9) ^ rotr<16>(s13) ^ rotr<24>(s11);
   }

void InvMixColumn_x4(uint32_t x[4])
   {
   x[0] = InvMixColumn(x[0]);
   x[1] = InvMixColumn(x[1]);
   x[2] = InvMixColumn(x[2]);
   x[3] = InvMixColumn(x[3]);
   }

uint32_t SE_word(uint32_t x)
   {
   uint32_t I[8] = { 0 };

   for(size_t i = 0; i != 8; ++i)
      I[i] = (x >> (7-i)) & 0x01010101;

   AES_SBOX(I);

   x = 0;

   for(size_t i = 0; i != 8; ++i)
      x |= ((I[i] & 0x01010101) << (7-i));

   return x;
   }

void aes_key_schedule(const uint8_t key[], size_t length,
                      secure_vector<uint32_t>& EK,
                      secure_vector<uint32_t>& DK,
                      bool bswap_keys = false)
   {
   static const uint32_t RC[10] = {
      0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000,
      0x20000000, 0x40000000, 0x80000000, 0x1B000000, 0x36000000 };

   const size_t X = length / 4;

   // Can't happen, but make static analyzers happy
   BOTAN_ASSERT_NOMSG(X == 4 || X == 6 || X == 8);

   const size_t rounds = (length / 4) + 6;

   // Help the optimizer
   BOTAN_ASSERT_NOMSG(rounds == 10 || rounds == 12 || rounds == 14);

   CT::poison(key, length);

   EK.resize(length + 28);
   DK.resize(length + 28);

   for(size_t i = 0; i != X; ++i)
      EK[i] = load_be<uint32_t>(key, i);

   for(size_t i = X; i < 4*(rounds+1); i += X)
      {
      EK[i] = EK[i-X] ^ RC[(i-X)/X] ^ rotl<8>(SE_word(EK[i-1]));

      for(size_t j = 1; j != X && (i+j) < EK.size(); ++j)
         {
         EK[i+j] = EK[i+j-X];

         if(X == 8 && j == 4)
            EK[i+j] ^= SE_word(EK[i+j-1]);
         else
            EK[i+j] ^= EK[i+j-1];
         }
      }

   for(size_t i = 0; i != 4*(rounds+1); i += 4)
      {
      DK[i  ] = EK[4*rounds - i  ];
      DK[i+1] = EK[4*rounds - i+1];
      DK[i+2] = EK[4*rounds - i+2];
      DK[i+3] = EK[4*rounds - i+3];
      }

   for(size_t i = 4; i != 4*rounds; i += 4)
      {
      InvMixColumn_x4(&DK[i]);
      }

   if(bswap_keys)
      {
      // HW AES on little endian needs the subkeys to be byte reversed
      for(size_t i = 0; i != EK.size(); ++i)
         EK[i] = reverse_bytes(EK[i]);
      for(size_t i = 0; i != DK.size(); ++i)
         DK[i] = reverse_bytes(DK[i]);
      }

   CT::unpoison(EK.data(), EK.size());
   CT::unpoison(DK.data(), DK.size());
   CT::unpoison(key, length);
   }

size_t aes_parallelism()
   {
#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return 4; // pipelined
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return 2; // pipelined
      }
#endif

   // bitsliced:
   return 2;
   }

const char* aes_provider()
   {
#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return "cpu";
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return "vperm";
      }
#endif

   return "base";
   }

}

std::string AES_128::provider() const { return aes_provider(); }
std::string AES_192::provider() const { return aes_provider(); }
std::string AES_256::provider() const { return aes_provider(); }

size_t AES_128::parallelism() const { return aes_parallelism(); }
size_t AES_192::parallelism() const { return aes_parallelism(); }
size_t AES_256::parallelism() const { return aes_parallelism(); }

void AES_128::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK);
   }

void AES_128::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK);
   }

void AES_128::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, CPUID::is_little_endian());
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK);
   }

void AES_128::clear()
   {
   zap(m_EK);
   zap(m_DK);
   }

void AES_192::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK);
   }

void AES_192::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK);
   }

void AES_192::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, CPUID::is_little_endian());
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK);
   }

void AES_192::clear()
   {
   zap(m_EK);
   zap(m_DK);
   }

void AES_256::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_encrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_encrypt_n(in, out, blocks);
      }
#endif

   aes_encrypt_n(in, out, blocks, m_EK);
   }

void AES_256::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_DK.empty() == false);

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return hw_aes_decrypt_n(in, out, blocks);
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_decrypt_n(in, out, blocks);
      }
#endif

   aes_decrypt_n(in, out, blocks, m_DK);
   }

void AES_256::key_schedule(const uint8_t key[], size_t length)
   {
#if defined(BOTAN_HAS_AES_NI)
   if(CPUID::has_aes_ni())
      {
      return aesni_key_schedule(key, length);
      }
#endif

#if defined(BOTAN_HAS_HW_AES_SUPPORT)
   if(CPUID::has_hw_aes())
      {
      return aes_key_schedule(key, length, m_EK, m_DK, CPUID::is_little_endian());
      }
#endif

#if defined(BOTAN_HAS_AES_VPERM)
   if(CPUID::has_vperm())
      {
      return vperm_key_schedule(key, length);
      }
#endif

   aes_key_schedule(key, length, m_EK, m_DK);
   }

void AES_256::clear()
   {
   zap(m_EK);
   zap(m_DK);
   }

}
/*
* AES using AES-NI instructions
* (C) 2009,2012 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <wmmintrin.h>

namespace Botan {

namespace {

BOTAN_FUNC_ISA("ssse3")
__m128i aes_128_key_expansion(__m128i key, __m128i key_with_rcon)
   {
   key_with_rcon = _mm_shuffle_epi32(key_with_rcon, _MM_SHUFFLE(3,3,3,3));
   key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
   key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
   key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
   return _mm_xor_si128(key, key_with_rcon);
   }

BOTAN_FUNC_ISA("ssse3")
void aes_192_key_expansion(__m128i* K1, __m128i* K2, __m128i key2_with_rcon,
                           uint32_t out[], bool last)
   {
   __m128i key1 = *K1;
   __m128i key2 = *K2;

   key2_with_rcon  = _mm_shuffle_epi32(key2_with_rcon, _MM_SHUFFLE(1,1,1,1));
   key1 = _mm_xor_si128(key1, _mm_slli_si128(key1, 4));
   key1 = _mm_xor_si128(key1, _mm_slli_si128(key1, 4));
   key1 = _mm_xor_si128(key1, _mm_slli_si128(key1, 4));
   key1 = _mm_xor_si128(key1, key2_with_rcon);

   *K1 = key1;
   _mm_storeu_si128(reinterpret_cast<__m128i*>(out), key1);

   if(last)
      return;

   key2 = _mm_xor_si128(key2, _mm_slli_si128(key2, 4));
   key2 = _mm_xor_si128(key2, _mm_shuffle_epi32(key1, _MM_SHUFFLE(3,3,3,3)));

   *K2 = key2;
   out[4] = _mm_cvtsi128_si32(key2);
   out[5] = _mm_cvtsi128_si32(_mm_srli_si128(key2, 4));
   }

/*
* The second half of the AES-256 key expansion (other half same as AES-128)
*/
BOTAN_FUNC_ISA("ssse3,aes")
__m128i aes_256_key_expansion(__m128i key, __m128i key2)
   {
   __m128i key_with_rcon = _mm_aeskeygenassist_si128(key2, 0x00);
   key_with_rcon = _mm_shuffle_epi32(key_with_rcon, _MM_SHUFFLE(2,2,2,2));

   key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
   key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
   key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
   return _mm_xor_si128(key, key_with_rcon);
   }

}

#define AES_ENC_4_ROUNDS(K)                     \
   do                                           \
      {                                         \
      B0 = _mm_aesenc_si128(B0, K);             \
      B1 = _mm_aesenc_si128(B1, K);             \
      B2 = _mm_aesenc_si128(B2, K);             \
      B3 = _mm_aesenc_si128(B3, K);             \
      } while(0)

#define AES_ENC_4_LAST_ROUNDS(K)                \
   do                                           \
      {                                         \
      B0 = _mm_aesenclast_si128(B0, K);         \
      B1 = _mm_aesenclast_si128(B1, K);         \
      B2 = _mm_aesenclast_si128(B2, K);         \
      B3 = _mm_aesenclast_si128(B3, K);         \
      } while(0)

#define AES_DEC_4_ROUNDS(K)                     \
   do                                           \
      {                                         \
      B0 = _mm_aesdec_si128(B0, K);             \
      B1 = _mm_aesdec_si128(B1, K);             \
      B2 = _mm_aesdec_si128(B2, K);             \
      B3 = _mm_aesdec_si128(B3, K);             \
      } while(0)

#define AES_DEC_4_LAST_ROUNDS(K)                \
   do                                           \
      {                                         \
      B0 = _mm_aesdeclast_si128(B0, K);         \
      B1 = _mm_aesdeclast_si128(B1, K);         \
      B2 = _mm_aesdeclast_si128(B2, K);         \
      B3 = _mm_aesdeclast_si128(B3, K);         \
      } while(0)

/*
* AES-128 Encryption
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_128::hw_aes_encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   const __m128i* in_mm = reinterpret_cast<const __m128i*>(in);
   __m128i* out_mm = reinterpret_cast<__m128i*>(out);

   const __m128i* key_mm = reinterpret_cast<const __m128i*>(m_EK.data());

   const __m128i K0  = _mm_loadu_si128(key_mm);
   const __m128i K1  = _mm_loadu_si128(key_mm + 1);
   const __m128i K2  = _mm_loadu_si128(key_mm + 2);
   const __m128i K3  = _mm_loadu_si128(key_mm + 3);
   const __m128i K4  = _mm_loadu_si128(key_mm + 4);
   const __m128i K5  = _mm_loadu_si128(key_mm + 5);
   const __m128i K6  = _mm_loadu_si128(key_mm + 6);
   const __m128i K7  = _mm_loadu_si128(key_mm + 7);
   const __m128i K8  = _mm_loadu_si128(key_mm + 8);
   const __m128i K9  = _mm_loadu_si128(key_mm + 9);
   const __m128i K10 = _mm_loadu_si128(key_mm + 10);

   while(blocks >= 4)
      {
      __m128i B0 = _mm_loadu_si128(in_mm + 0);
      __m128i B1 = _mm_loadu_si128(in_mm + 1);
      __m128i B2 = _mm_loadu_si128(in_mm + 2);
      __m128i B3 = _mm_loadu_si128(in_mm + 3);

      B0 = _mm_xor_si128(B0, K0);
      B1 = _mm_xor_si128(B1, K0);
      B2 = _mm_xor_si128(B2, K0);
      B3 = _mm_xor_si128(B3, K0);

      AES_ENC_4_ROUNDS(K1);
      AES_ENC_4_ROUNDS(K2);
      AES_ENC_4_ROUNDS(K3);
      AES_ENC_4_ROUNDS(K4);
      AES_ENC_4_ROUNDS(K5);
      AES_ENC_4_ROUNDS(K6);
      AES_ENC_4_ROUNDS(K7);
      AES_ENC_4_ROUNDS(K8);
      AES_ENC_4_ROUNDS(K9);
      AES_ENC_4_LAST_ROUNDS(K10);

      _mm_storeu_si128(out_mm + 0, B0);
      _mm_storeu_si128(out_mm + 1, B1);
      _mm_storeu_si128(out_mm + 2, B2);
      _mm_storeu_si128(out_mm + 3, B3);

      blocks -= 4;
      in_mm += 4;
      out_mm += 4;
      }

   for(size_t i = 0; i != blocks; ++i)
      {
      __m128i B = _mm_loadu_si128(in_mm + i);

      B = _mm_xor_si128(B, K0);

      B = _mm_aesenc_si128(B, K1);
      B = _mm_aesenc_si128(B, K2);
      B = _mm_aesenc_si128(B, K3);
      B = _mm_aesenc_si128(B, K4);
      B = _mm_aesenc_si128(B, K5);
      B = _mm_aesenc_si128(B, K6);
      B = _mm_aesenc_si128(B, K7);
      B = _mm_aesenc_si128(B, K8);
      B = _mm_aesenc_si128(B, K9);
      B = _mm_aesenclast_si128(B, K10);

      _mm_storeu_si128(out_mm + i, B);
      }
   }

/*
* AES-128 Decryption
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_128::hw_aes_decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   const __m128i* in_mm = reinterpret_cast<const __m128i*>(in);
   __m128i* out_mm = reinterpret_cast<__m128i*>(out);

   const __m128i* key_mm = reinterpret_cast<const __m128i*>(m_DK.data());

   const __m128i K0  = _mm_loadu_si128(key_mm);
   const __m128i K1  = _mm_loadu_si128(key_mm + 1);
   const __m128i K2  = _mm_loadu_si128(key_mm + 2);
   const __m128i K3  = _mm_loadu_si128(key_mm + 3);
   const __m128i K4  = _mm_loadu_si128(key_mm + 4);
   const __m128i K5  = _mm_loadu_si128(key_mm + 5);
   const __m128i K6  = _mm_loadu_si128(key_mm + 6);
   const __m128i K7  = _mm_loadu_si128(key_mm + 7);
   const __m128i K8  = _mm_loadu_si128(key_mm + 8);
   const __m128i K9  = _mm_loadu_si128(key_mm + 9);
   const __m128i K10 = _mm_loadu_si128(key_mm + 10);

   while(blocks >= 4)
      {
      __m128i B0 = _mm_loadu_si128(in_mm + 0);
      __m128i B1 = _mm_loadu_si128(in_mm + 1);
      __m128i B2 = _mm_loadu_si128(in_mm + 2);
      __m128i B3 = _mm_loadu_si128(in_mm + 3);

      B0 = _mm_xor_si128(B0, K0);
      B1 = _mm_xor_si128(B1, K0);
      B2 = _mm_xor_si128(B2, K0);
      B3 = _mm_xor_si128(B3, K0);

      AES_DEC_4_ROUNDS(K1);
      AES_DEC_4_ROUNDS(K2);
      AES_DEC_4_ROUNDS(K3);
      AES_DEC_4_ROUNDS(K4);
      AES_DEC_4_ROUNDS(K5);
      AES_DEC_4_ROUNDS(K6);
      AES_DEC_4_ROUNDS(K7);
      AES_DEC_4_ROUNDS(K8);
      AES_DEC_4_ROUNDS(K9);
      AES_DEC_4_LAST_ROUNDS(K10);

      _mm_storeu_si128(out_mm + 0, B0);
      _mm_storeu_si128(out_mm + 1, B1);
      _mm_storeu_si128(out_mm + 2, B2);
      _mm_storeu_si128(out_mm + 3, B3);

      blocks -= 4;
      in_mm += 4;
      out_mm += 4;
      }

   for(size_t i = 0; i != blocks; ++i)
      {
      __m128i B = _mm_loadu_si128(in_mm + i);

      B = _mm_xor_si128(B, K0);

      B = _mm_aesdec_si128(B, K1);
      B = _mm_aesdec_si128(B, K2);
      B = _mm_aesdec_si128(B, K3);
      B = _mm_aesdec_si128(B, K4);
      B = _mm_aesdec_si128(B, K5);
      B = _mm_aesdec_si128(B, K6);
      B = _mm_aesdec_si128(B, K7);
      B = _mm_aesdec_si128(B, K8);
      B = _mm_aesdec_si128(B, K9);
      B = _mm_aesdeclast_si128(B, K10);

      _mm_storeu_si128(out_mm + i, B);
      }
   }

/*
* AES-128 Key Schedule
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_128::aesni_key_schedule(const uint8_t key[], size_t)
   {
   m_EK.resize(44);
   m_DK.resize(44);

   #define AES_128_key_exp(K, RCON) \
      aes_128_key_expansion(K, _mm_aeskeygenassist_si128(K, RCON))

   const __m128i K0  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key));
   const __m128i K1  = AES_128_key_exp(K0, 0x01);
   const __m128i K2  = AES_128_key_exp(K1, 0x02);
   const __m128i K3  = AES_128_key_exp(K2, 0x04);
   const __m128i K4  = AES_128_key_exp(K3, 0x08);
   const __m128i K5  = AES_128_key_exp(K4, 0x10);
   const __m128i K6  = AES_128_key_exp(K5, 0x20);
   const __m128i K7  = AES_128_key_exp(K6, 0x40);
   const __m128i K8  = AES_128_key_exp(K7, 0x80);
   const __m128i K9  = AES_128_key_exp(K8, 0x1B);
   const __m128i K10 = AES_128_key_exp(K9, 0x36);

   __m128i* EK_mm = reinterpret_cast<__m128i*>(m_EK.data());
   _mm_storeu_si128(EK_mm     , K0);
   _mm_storeu_si128(EK_mm +  1, K1);
   _mm_storeu_si128(EK_mm +  2, K2);
   _mm_storeu_si128(EK_mm +  3, K3);
   _mm_storeu_si128(EK_mm +  4, K4);
   _mm_storeu_si128(EK_mm +  5, K5);
   _mm_storeu_si128(EK_mm +  6, K6);
   _mm_storeu_si128(EK_mm +  7, K7);
   _mm_storeu_si128(EK_mm +  8, K8);
   _mm_storeu_si128(EK_mm +  9, K9);
   _mm_storeu_si128(EK_mm + 10, K10);

   // Now generate decryption keys

   __m128i* DK_mm = reinterpret_cast<__m128i*>(m_DK.data());
   _mm_storeu_si128(DK_mm     , K10);
   _mm_storeu_si128(DK_mm +  1, _mm_aesimc_si128(K9));
   _mm_storeu_si128(DK_mm +  2, _mm_aesimc_si128(K8));
   _mm_storeu_si128(DK_mm +  3, _mm_aesimc_si128(K7));
   _mm_storeu_si128(DK_mm +  4, _mm_aesimc_si128(K6));
   _mm_storeu_si128(DK_mm +  5, _mm_aesimc_si128(K5));
   _mm_storeu_si128(DK_mm +  6, _mm_aesimc_si128(K4));
   _mm_storeu_si128(DK_mm +  7, _mm_aesimc_si128(K3));
   _mm_storeu_si128(DK_mm +  8, _mm_aesimc_si128(K2));
   _mm_storeu_si128(DK_mm +  9, _mm_aesimc_si128(K1));
   _mm_storeu_si128(DK_mm + 10, K0);
   }

/*
* AES-192 Encryption
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_192::hw_aes_encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   const __m128i* in_mm = reinterpret_cast<const __m128i*>(in);
   __m128i* out_mm = reinterpret_cast<__m128i*>(out);

   const __m128i* key_mm = reinterpret_cast<const __m128i*>(m_EK.data());

   const __m128i K0  = _mm_loadu_si128(key_mm);
   const __m128i K1  = _mm_loadu_si128(key_mm + 1);
   const __m128i K2  = _mm_loadu_si128(key_mm + 2);
   const __m128i K3  = _mm_loadu_si128(key_mm + 3);
   const __m128i K4  = _mm_loadu_si128(key_mm + 4);
   const __m128i K5  = _mm_loadu_si128(key_mm + 5);
   const __m128i K6  = _mm_loadu_si128(key_mm + 6);
   const __m128i K7  = _mm_loadu_si128(key_mm + 7);
   const __m128i K8  = _mm_loadu_si128(key_mm + 8);
   const __m128i K9  = _mm_loadu_si128(key_mm + 9);
   const __m128i K10 = _mm_loadu_si128(key_mm + 10);
   const __m128i K11 = _mm_loadu_si128(key_mm + 11);
   const __m128i K12 = _mm_loadu_si128(key_mm + 12);

   while(blocks >= 4)
      {
      __m128i B0 = _mm_loadu_si128(in_mm + 0);
      __m128i B1 = _mm_loadu_si128(in_mm + 1);
      __m128i B2 = _mm_loadu_si128(in_mm + 2);
      __m128i B3 = _mm_loadu_si128(in_mm + 3);

      B0 = _mm_xor_si128(B0, K0);
      B1 = _mm_xor_si128(B1, K0);
      B2 = _mm_xor_si128(B2, K0);
      B3 = _mm_xor_si128(B3, K0);

      AES_ENC_4_ROUNDS(K1);
      AES_ENC_4_ROUNDS(K2);
      AES_ENC_4_ROUNDS(K3);
      AES_ENC_4_ROUNDS(K4);
      AES_ENC_4_ROUNDS(K5);
      AES_ENC_4_ROUNDS(K6);
      AES_ENC_4_ROUNDS(K7);
      AES_ENC_4_ROUNDS(K8);
      AES_ENC_4_ROUNDS(K9);
      AES_ENC_4_ROUNDS(K10);
      AES_ENC_4_ROUNDS(K11);
      AES_ENC_4_LAST_ROUNDS(K12);

      _mm_storeu_si128(out_mm + 0, B0);
      _mm_storeu_si128(out_mm + 1, B1);
      _mm_storeu_si128(out_mm + 2, B2);
      _mm_storeu_si128(out_mm + 3, B3);

      blocks -= 4;
      in_mm += 4;
      out_mm += 4;
      }

   for(size_t i = 0; i != blocks; ++i)
      {
      __m128i B = _mm_loadu_si128(in_mm + i);

      B = _mm_xor_si128(B, K0);

      B = _mm_aesenc_si128(B, K1);
      B = _mm_aesenc_si128(B, K2);
      B = _mm_aesenc_si128(B, K3);
      B = _mm_aesenc_si128(B, K4);
      B = _mm_aesenc_si128(B, K5);
      B = _mm_aesenc_si128(B, K6);
      B = _mm_aesenc_si128(B, K7);
      B = _mm_aesenc_si128(B, K8);
      B = _mm_aesenc_si128(B, K9);
      B = _mm_aesenc_si128(B, K10);
      B = _mm_aesenc_si128(B, K11);
      B = _mm_aesenclast_si128(B, K12);

      _mm_storeu_si128(out_mm + i, B);
      }
   }

/*
* AES-192 Decryption
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_192::hw_aes_decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   const __m128i* in_mm = reinterpret_cast<const __m128i*>(in);
   __m128i* out_mm = reinterpret_cast<__m128i*>(out);

   const __m128i* key_mm = reinterpret_cast<const __m128i*>(m_DK.data());

   const __m128i K0  = _mm_loadu_si128(key_mm);
   const __m128i K1  = _mm_loadu_si128(key_mm + 1);
   const __m128i K2  = _mm_loadu_si128(key_mm + 2);
   const __m128i K3  = _mm_loadu_si128(key_mm + 3);
   const __m128i K4  = _mm_loadu_si128(key_mm + 4);
   const __m128i K5  = _mm_loadu_si128(key_mm + 5);
   const __m128i K6  = _mm_loadu_si128(key_mm + 6);
   const __m128i K7  = _mm_loadu_si128(key_mm + 7);
   const __m128i K8  = _mm_loadu_si128(key_mm + 8);
   const __m128i K9  = _mm_loadu_si128(key_mm + 9);
   const __m128i K10 = _mm_loadu_si128(key_mm + 10);
   const __m128i K11 = _mm_loadu_si128(key_mm + 11);
   const __m128i K12 = _mm_loadu_si128(key_mm + 12);

   while(blocks >= 4)
      {
      __m128i B0 = _mm_loadu_si128(in_mm + 0);
      __m128i B1 = _mm_loadu_si128(in_mm + 1);
      __m128i B2 = _mm_loadu_si128(in_mm + 2);
      __m128i B3 = _mm_loadu_si128(in_mm + 3);

      B0 = _mm_xor_si128(B0, K0);
      B1 = _mm_xor_si128(B1, K0);
      B2 = _mm_xor_si128(B2, K0);
      B3 = _mm_xor_si128(B3, K0);

      AES_DEC_4_ROUNDS(K1);
      AES_DEC_4_ROUNDS(K2);
      AES_DEC_4_ROUNDS(K3);
      AES_DEC_4_ROUNDS(K4);
      AES_DEC_4_ROUNDS(K5);
      AES_DEC_4_ROUNDS(K6);
      AES_DEC_4_ROUNDS(K7);
      AES_DEC_4_ROUNDS(K8);
      AES_DEC_4_ROUNDS(K9);
      AES_DEC_4_ROUNDS(K10);
      AES_DEC_4_ROUNDS(K11);
      AES_DEC_4_LAST_ROUNDS(K12);

      _mm_storeu_si128(out_mm + 0, B0);
      _mm_storeu_si128(out_mm + 1, B1);
      _mm_storeu_si128(out_mm + 2, B2);
      _mm_storeu_si128(out_mm + 3, B3);

      blocks -= 4;
      in_mm += 4;
      out_mm += 4;
      }

   for(size_t i = 0; i != blocks; ++i)
      {
      __m128i B = _mm_loadu_si128(in_mm + i);

      B = _mm_xor_si128(B, K0);

      B = _mm_aesdec_si128(B, K1);
      B = _mm_aesdec_si128(B, K2);
      B = _mm_aesdec_si128(B, K3);
      B = _mm_aesdec_si128(B, K4);
      B = _mm_aesdec_si128(B, K5);
      B = _mm_aesdec_si128(B, K6);
      B = _mm_aesdec_si128(B, K7);
      B = _mm_aesdec_si128(B, K8);
      B = _mm_aesdec_si128(B, K9);
      B = _mm_aesdec_si128(B, K10);
      B = _mm_aesdec_si128(B, K11);
      B = _mm_aesdeclast_si128(B, K12);

      _mm_storeu_si128(out_mm + i, B);
      }
   }

/*
* AES-192 Key Schedule
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_192::aesni_key_schedule(const uint8_t key[], size_t)
   {
   m_EK.resize(52);
   m_DK.resize(52);

   __m128i K0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key));
   __m128i K1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key + 8));
   K1 = _mm_srli_si128(K1, 8);

   load_le(m_EK.data(), key, 6);

   #define AES_192_key_exp(RCON, EK_OFF)                         \
     aes_192_key_expansion(&K0, &K1,                             \
                           _mm_aeskeygenassist_si128(K1, RCON),  \
                           &m_EK[EK_OFF], EK_OFF == 48)

   AES_192_key_exp(0x01, 6);
   AES_192_key_exp(0x02, 12);
   AES_192_key_exp(0x04, 18);
   AES_192_key_exp(0x08, 24);
   AES_192_key_exp(0x10, 30);
   AES_192_key_exp(0x20, 36);
   AES_192_key_exp(0x40, 42);
   AES_192_key_exp(0x80, 48);

   #undef AES_192_key_exp

   // Now generate decryption keys
   const __m128i* EK_mm = reinterpret_cast<const __m128i*>(m_EK.data());

   __m128i* DK_mm = reinterpret_cast<__m128i*>(m_DK.data());
   _mm_storeu_si128(DK_mm     , _mm_loadu_si128(EK_mm + 12));
   _mm_storeu_si128(DK_mm +  1, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 11)));
   _mm_storeu_si128(DK_mm +  2, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 10)));
   _mm_storeu_si128(DK_mm +  3, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 9)));
   _mm_storeu_si128(DK_mm +  4, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 8)));
   _mm_storeu_si128(DK_mm +  5, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 7)));
   _mm_storeu_si128(DK_mm +  6, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 6)));
   _mm_storeu_si128(DK_mm +  7, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 5)));
   _mm_storeu_si128(DK_mm +  8, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 4)));
   _mm_storeu_si128(DK_mm +  9, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 3)));
   _mm_storeu_si128(DK_mm + 10, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 2)));
   _mm_storeu_si128(DK_mm + 11, _mm_aesimc_si128(_mm_loadu_si128(EK_mm + 1)));
   _mm_storeu_si128(DK_mm + 12, _mm_loadu_si128(EK_mm + 0));
   }

/*
* AES-256 Encryption
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_256::hw_aes_encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   const __m128i* in_mm = reinterpret_cast<const __m128i*>(in);
   __m128i* out_mm = reinterpret_cast<__m128i*>(out);

   const __m128i* key_mm = reinterpret_cast<const __m128i*>(m_EK.data());

   const __m128i K0  = _mm_loadu_si128(key_mm);
   const __m128i K1  = _mm_loadu_si128(key_mm + 1);
   const __m128i K2  = _mm_loadu_si128(key_mm + 2);
   const __m128i K3  = _mm_loadu_si128(key_mm + 3);
   const __m128i K4  = _mm_loadu_si128(key_mm + 4);
   const __m128i K5  = _mm_loadu_si128(key_mm + 5);
   const __m128i K6  = _mm_loadu_si128(key_mm + 6);
   const __m128i K7  = _mm_loadu_si128(key_mm + 7);
   const __m128i K8  = _mm_loadu_si128(key_mm + 8);
   const __m128i K9  = _mm_loadu_si128(key_mm + 9);
   const __m128i K10 = _mm_loadu_si128(key_mm + 10);
   const __m128i K11 = _mm_loadu_si128(key_mm + 11);
   const __m128i K12 = _mm_loadu_si128(key_mm + 12);
   const __m128i K13 = _mm_loadu_si128(key_mm + 13);
   const __m128i K14 = _mm_loadu_si128(key_mm + 14);

   while(blocks >= 4)
      {
      __m128i B0 = _mm_loadu_si128(in_mm + 0);
      __m128i B1 = _mm_loadu_si128(in_mm + 1);
      __m128i B2 = _mm_loadu_si128(in_mm + 2);
      __m128i B3 = _mm_loadu_si128(in_mm + 3);

      B0 = _mm_xor_si128(B0, K0);
      B1 = _mm_xor_si128(B1, K0);
      B2 = _mm_xor_si128(B2, K0);
      B3 = _mm_xor_si128(B3, K0);

      AES_ENC_4_ROUNDS(K1);
      AES_ENC_4_ROUNDS(K2);
      AES_ENC_4_ROUNDS(K3);
      AES_ENC_4_ROUNDS(K4);
      AES_ENC_4_ROUNDS(K5);
      AES_ENC_4_ROUNDS(K6);
      AES_ENC_4_ROUNDS(K7);
      AES_ENC_4_ROUNDS(K8);
      AES_ENC_4_ROUNDS(K9);
      AES_ENC_4_ROUNDS(K10);
      AES_ENC_4_ROUNDS(K11);
      AES_ENC_4_ROUNDS(K12);
      AES_ENC_4_ROUNDS(K13);
      AES_ENC_4_LAST_ROUNDS(K14);

      _mm_storeu_si128(out_mm + 0, B0);
      _mm_storeu_si128(out_mm + 1, B1);
      _mm_storeu_si128(out_mm + 2, B2);
      _mm_storeu_si128(out_mm + 3, B3);

      blocks -= 4;
      in_mm += 4;
      out_mm += 4;
      }

   for(size_t i = 0; i != blocks; ++i)
      {
      __m128i B = _mm_loadu_si128(in_mm + i);

      B = _mm_xor_si128(B, K0);

      B = _mm_aesenc_si128(B, K1);
      B = _mm_aesenc_si128(B, K2);
      B = _mm_aesenc_si128(B, K3);
      B = _mm_aesenc_si128(B, K4);
      B = _mm_aesenc_si128(B, K5);
      B = _mm_aesenc_si128(B, K6);
      B = _mm_aesenc_si128(B, K7);
      B = _mm_aesenc_si128(B, K8);
      B = _mm_aesenc_si128(B, K9);
      B = _mm_aesenc_si128(B, K10);
      B = _mm_aesenc_si128(B, K11);
      B = _mm_aesenc_si128(B, K12);
      B = _mm_aesenc_si128(B, K13);
      B = _mm_aesenclast_si128(B, K14);

      _mm_storeu_si128(out_mm + i, B);
      }
   }

/*
* AES-256 Decryption
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_256::hw_aes_decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   const __m128i* in_mm = reinterpret_cast<const __m128i*>(in);
   __m128i* out_mm = reinterpret_cast<__m128i*>(out);

   const __m128i* key_mm = reinterpret_cast<const __m128i*>(m_DK.data());

   const __m128i K0  = _mm_loadu_si128(key_mm);
   const __m128i K1  = _mm_loadu_si128(key_mm + 1);
   const __m128i K2  = _mm_loadu_si128(key_mm + 2);
   const __m128i K3  = _mm_loadu_si128(key_mm + 3);
   const __m128i K4  = _mm_loadu_si128(key_mm + 4);
   const __m128i K5  = _mm_loadu_si128(key_mm + 5);
   const __m128i K6  = _mm_loadu_si128(key_mm + 6);
   const __m128i K7  = _mm_loadu_si128(key_mm + 7);
   const __m128i K8  = _mm_loadu_si128(key_mm + 8);
   const __m128i K9  = _mm_loadu_si128(key_mm + 9);
   const __m128i K10 = _mm_loadu_si128(key_mm + 10);
   const __m128i K11 = _mm_loadu_si128(key_mm + 11);
   const __m128i K12 = _mm_loadu_si128(key_mm + 12);
   const __m128i K13 = _mm_loadu_si128(key_mm + 13);
   const __m128i K14 = _mm_loadu_si128(key_mm + 14);

   while(blocks >= 4)
      {
      __m128i B0 = _mm_loadu_si128(in_mm + 0);
      __m128i B1 = _mm_loadu_si128(in_mm + 1);
      __m128i B2 = _mm_loadu_si128(in_mm + 2);
      __m128i B3 = _mm_loadu_si128(in_mm + 3);

      B0 = _mm_xor_si128(B0, K0);
      B1 = _mm_xor_si128(B1, K0);
      B2 = _mm_xor_si128(B2, K0);
      B3 = _mm_xor_si128(B3, K0);

      AES_DEC_4_ROUNDS(K1);
      AES_DEC_4_ROUNDS(K2);
      AES_DEC_4_ROUNDS(K3);
      AES_DEC_4_ROUNDS(K4);
      AES_DEC_4_ROUNDS(K5);
      AES_DEC_4_ROUNDS(K6);
      AES_DEC_4_ROUNDS(K7);
      AES_DEC_4_ROUNDS(K8);
      AES_DEC_4_ROUNDS(K9);
      AES_DEC_4_ROUNDS(K10);
      AES_DEC_4_ROUNDS(K11);
      AES_DEC_4_ROUNDS(K12);
      AES_DEC_4_ROUNDS(K13);
      AES_DEC_4_LAST_ROUNDS(K14);

      _mm_storeu_si128(out_mm + 0, B0);
      _mm_storeu_si128(out_mm + 1, B1);
      _mm_storeu_si128(out_mm + 2, B2);
      _mm_storeu_si128(out_mm + 3, B3);

      blocks -= 4;
      in_mm += 4;
      out_mm += 4;
      }

   for(size_t i = 0; i != blocks; ++i)
      {
      __m128i B = _mm_loadu_si128(in_mm + i);

      B = _mm_xor_si128(B, K0);

      B = _mm_aesdec_si128(B, K1);
      B = _mm_aesdec_si128(B, K2);
      B = _mm_aesdec_si128(B, K3);
      B = _mm_aesdec_si128(B, K4);
      B = _mm_aesdec_si128(B, K5);
      B = _mm_aesdec_si128(B, K6);
      B = _mm_aesdec_si128(B, K7);
      B = _mm_aesdec_si128(B, K8);
      B = _mm_aesdec_si128(B, K9);
      B = _mm_aesdec_si128(B, K10);
      B = _mm_aesdec_si128(B, K11);
      B = _mm_aesdec_si128(B, K12);
      B = _mm_aesdec_si128(B, K13);
      B = _mm_aesdeclast_si128(B, K14);

      _mm_storeu_si128(out_mm + i, B);
      }
   }

/*
* AES-256 Key Schedule
*/
BOTAN_FUNC_ISA("ssse3,aes")
void AES_256::aesni_key_schedule(const uint8_t key[], size_t)
   {
   m_EK.resize(60);
   m_DK.resize(60);

   const __m128i K0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key));
   const __m128i K1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key + 16));

   const __m128i K2 = aes_128_key_expansion(K0, _mm_aeskeygenassist_si128(K1, 0x01));
   const __m128i K3 = aes_256_key_expansion(K1, K2);

   const __m128i K4 = aes_128_key_expansion(K2, _mm_aeskeygenassist_si128(K3, 0x02));
   const __m128i K5 = aes_256_key_expansion(K3, K4);

   const __m128i K6 = aes_128_key_expansion(K4, _mm_aeskeygenassist_si128(K5, 0x04));
   const __m128i K7 = aes_256_key_expansion(K5, K6);

   const __m128i K8 = aes_128_key_expansion(K6, _mm_aeskeygenassist_si128(K7, 0x08));
   const __m128i K9 = aes_256_key_expansion(K7, K8);

   const __m128i K10 = aes_128_key_expansion(K8, _mm_aeskeygenassist_si128(K9, 0x10));
   const __m128i K11 = aes_256_key_expansion(K9, K10);

   const __m128i K12 = aes_128_key_expansion(K10, _mm_aeskeygenassist_si128(K11, 0x20));
   const __m128i K13 = aes_256_key_expansion(K11, K12);

   const __m128i K14 = aes_128_key_expansion(K12, _mm_aeskeygenassist_si128(K13, 0x40));

   __m128i* EK_mm = reinterpret_cast<__m128i*>(m_EK.data());
   _mm_storeu_si128(EK_mm     , K0);
   _mm_storeu_si128(EK_mm +  1, K1);
   _mm_storeu_si128(EK_mm +  2, K2);
   _mm_storeu_si128(EK_mm +  3, K3);
   _mm_storeu_si128(EK_mm +  4, K4);
   _mm_storeu_si128(EK_mm +  5, K5);
   _mm_storeu_si128(EK_mm +  6, K6);
   _mm_storeu_si128(EK_mm +  7, K7);
   _mm_storeu_si128(EK_mm +  8, K8);
   _mm_storeu_si128(EK_mm +  9, K9);
   _mm_storeu_si128(EK_mm + 10, K10);
   _mm_storeu_si128(EK_mm + 11, K11);
   _mm_storeu_si128(EK_mm + 12, K12);
   _mm_storeu_si128(EK_mm + 13, K13);
   _mm_storeu_si128(EK_mm + 14, K14);

   // Now generate decryption keys
   __m128i* DK_mm = reinterpret_cast<__m128i*>(m_DK.data());
   _mm_storeu_si128(DK_mm     , K14);
   _mm_storeu_si128(DK_mm +  1, _mm_aesimc_si128(K13));
   _mm_storeu_si128(DK_mm +  2, _mm_aesimc_si128(K12));
   _mm_storeu_si128(DK_mm +  3, _mm_aesimc_si128(K11));
   _mm_storeu_si128(DK_mm +  4, _mm_aesimc_si128(K10));
   _mm_storeu_si128(DK_mm +  5, _mm_aesimc_si128(K9));
   _mm_storeu_si128(DK_mm +  6, _mm_aesimc_si128(K8));
   _mm_storeu_si128(DK_mm +  7, _mm_aesimc_si128(K7));
   _mm_storeu_si128(DK_mm +  8, _mm_aesimc_si128(K6));
   _mm_storeu_si128(DK_mm +  9, _mm_aesimc_si128(K5));
   _mm_storeu_si128(DK_mm + 10, _mm_aesimc_si128(K4));
   _mm_storeu_si128(DK_mm + 11, _mm_aesimc_si128(K3));
   _mm_storeu_si128(DK_mm + 12, _mm_aesimc_si128(K2));
   _mm_storeu_si128(DK_mm + 13, _mm_aesimc_si128(K1));
   _mm_storeu_si128(DK_mm + 14, K0);
   }

#undef AES_ENC_4_ROUNDS
#undef AES_ENC_4_LAST_ROUNDS
#undef AES_DEC_4_ROUNDS
#undef AES_DEC_4_LAST_ROUNDS

}
/**
* (C) 2018,2019 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

static const size_t SYNC_POINTS = 4;

secure_vector<uint8_t> argon2_H0(HashFunction& blake2b,
                                 size_t output_len,
                                 const char* password, size_t password_len,
                                 const uint8_t salt[], size_t salt_len,
                                 const uint8_t key[], size_t key_len,
                                 const uint8_t ad[], size_t ad_len,
                                 size_t y, size_t p, size_t M, size_t t)
   {
   const uint8_t v = 19; // Argon2 version code

   blake2b.update_le(static_cast<uint32_t>(p));
   blake2b.update_le(static_cast<uint32_t>(output_len));
   blake2b.update_le(static_cast<uint32_t>(M));
   blake2b.update_le(static_cast<uint32_t>(t));
   blake2b.update_le(static_cast<uint32_t>(v));
   blake2b.update_le(static_cast<uint32_t>(y));

   blake2b.update_le(static_cast<uint32_t>(password_len));
   blake2b.update(cast_char_ptr_to_uint8(password), password_len);

   blake2b.update_le(static_cast<uint32_t>(salt_len));
   blake2b.update(salt, salt_len);

   blake2b.update_le(static_cast<uint32_t>(key_len));
   blake2b.update(key, key_len);

   blake2b.update_le(static_cast<uint32_t>(ad_len));
   blake2b.update(ad, ad_len);

   return blake2b.final();
   }

void Htick(secure_vector<uint8_t>& T,
           uint8_t output[],
           size_t output_len,
           HashFunction& blake2b,
           const secure_vector<uint8_t>& H0,
           size_t p0, size_t p1)
   {
   BOTAN_ASSERT_NOMSG(output_len % 64 == 0);

   blake2b.update_le(static_cast<uint32_t>(output_len));
   blake2b.update(H0);
   blake2b.update_le(static_cast<uint32_t>(p0));
   blake2b.update_le(static_cast<uint32_t>(p1));

   blake2b.final(&T[0]);

   while(output_len > 64)
      {
      copy_mem(output, &T[0], 32);
      output_len -= 32;
      output += 32;

      blake2b.update(T);
      blake2b.final(&T[0]);
      }

   if(output_len > 0)
      copy_mem(output, &T[0], output_len);
   }

void extract_key(uint8_t output[], size_t output_len,
                 const secure_vector<uint64_t>& B,
                 size_t memory, size_t threads)
   {
   const size_t lanes = memory / threads;

   secure_vector<uint64_t> sum(128);

   for(size_t lane = 0; lane != threads; ++lane)
      {
      size_t start = 128*(lane * lanes + lanes - 1);
      size_t end = 128*(lane * lanes + lanes);

      for(size_t j = start; j != end; ++j)
         {
         sum[j % 128] ^= B[j];
         }
      }

   secure_vector<uint8_t> sum8(1024);
   copy_out_le(sum8.data(), 1024, sum.data());

   if(output_len <= 64)
      {
      std::unique_ptr<HashFunction> blake2b = HashFunction::create_or_throw("BLAKE2b(" + std::to_string(output_len*8) + ")");
      blake2b->update_le(static_cast<uint32_t>(output_len));
      blake2b->update(sum8.data(), sum8.size());
      blake2b->final(output);
      }
   else
      {
      secure_vector<uint8_t> T(64);

      std::unique_ptr<HashFunction> blake2b = HashFunction::create_or_throw("BLAKE2b(512)");
      blake2b->update_le(static_cast<uint32_t>(output_len));
      blake2b->update(sum8.data(), sum8.size());
      blake2b->final(&T[0]);

      while(output_len > 64)
         {
         copy_mem(output, &T[0], 32);
         output_len -= 32;
         output += 32;

         if(output_len > 64)
            {
            blake2b->update(T);
            blake2b->final(&T[0]);
            }
         }

      if(output_len == 64)
         {
         blake2b->update(T);
         blake2b->final(output);
         }
      else
         {
         std::unique_ptr<HashFunction> blake2b_f = HashFunction::create_or_throw("BLAKE2b(" + std::to_string(output_len*8) + ")");
         blake2b_f->update(T);
         blake2b_f->final(output);
         }
      }
   }

void init_blocks(secure_vector<uint64_t>& B,
                 HashFunction& blake2b,
                 const secure_vector<uint8_t>& H0,
                 size_t memory,
                 size_t threads)
   {
   BOTAN_ASSERT_NOMSG(B.size() >= threads*256);

   secure_vector<uint8_t> H(1024);
   secure_vector<uint8_t> T(blake2b.output_length());

   for(size_t i = 0; i != threads; ++i)
      {
      const size_t B_off = i * (memory / threads);

      BOTAN_ASSERT_NOMSG(B.size() >= 128*(B_off+2));

      Htick(T, &H[0], H.size(), blake2b, H0, 0, i);

      for(size_t j = 0; j != 128; ++j)
         {
         B[128*B_off+j] = load_le<uint64_t>(H.data(), j);
         }

      Htick(T, &H[0], H.size(), blake2b, H0, 1, i);

      for(size_t j = 0; j != 128; ++j)
         {
         B[128*(B_off+1)+j] = load_le<uint64_t>(H.data(), j);
         }
      }
   }

inline void blamka_G(uint64_t& A, uint64_t& B, uint64_t& C, uint64_t& D)
   {
   A += B + (static_cast<uint64_t>(2) * static_cast<uint32_t>(A)) * static_cast<uint32_t>(B);
   D = rotr<32>(A ^ D);

   C += D + (static_cast<uint64_t>(2) * static_cast<uint32_t>(C)) * static_cast<uint32_t>(D);
   B = rotr<24>(B ^ C);

   A += B + (static_cast<uint64_t>(2) * static_cast<uint32_t>(A)) * static_cast<uint32_t>(B);
   D = rotr<16>(A ^ D);

   C += D + (static_cast<uint64_t>(2) * static_cast<uint32_t>(C)) * static_cast<uint32_t>(D);
   B = rotr<63>(B ^ C);
   }

inline void blamka(uint64_t& V0, uint64_t& V1, uint64_t& V2, uint64_t& V3,
                   uint64_t& V4, uint64_t& V5, uint64_t& V6, uint64_t& V7,
                   uint64_t& V8, uint64_t& V9, uint64_t& VA, uint64_t& VB,
                   uint64_t& VC, uint64_t& VD, uint64_t& VE, uint64_t& VF)
   {
   blamka_G(V0, V4, V8, VC);
   blamka_G(V1, V5, V9, VD);
   blamka_G(V2, V6, VA, VE);
   blamka_G(V3, V7, VB, VF);

   blamka_G(V0, V5, VA, VF);
   blamka_G(V1, V6, VB, VC);
   blamka_G(V2, V7, V8, VD);
   blamka_G(V3, V4, V9, VE);
   }

void process_block_xor(secure_vector<uint64_t>& T,
                       secure_vector<uint64_t>& B,
                       size_t offset,
                       size_t prev,
                       size_t new_offset)
   {
   for(size_t i = 0; i != 128; ++i)
      T[i] = B[128*prev+i] ^ B[128*new_offset+i];

   for(size_t i = 0; i != 128; i += 16)
      {
      blamka(T[i+ 0], T[i+ 1], T[i+ 2], T[i+ 3],
             T[i+ 4], T[i+ 5], T[i+ 6], T[i+ 7],
             T[i+ 8], T[i+ 9], T[i+10], T[i+11],
             T[i+12], T[i+13], T[i+14], T[i+15]);
      }

   for(size_t i = 0; i != 128 / 8; i += 2)
      {
      blamka(T[    i], T[    i+1], T[ 16+i], T[ 16+i+1],
             T[ 32+i], T[ 32+i+1], T[ 48+i], T[ 48+i+1],
             T[ 64+i], T[ 64+i+1], T[ 80+i], T[ 80+i+1],
             T[ 96+i], T[ 96+i+1], T[112+i], T[112+i+1]);
      }

   for(size_t i = 0; i != 128; ++i)
      B[128*offset + i] ^= T[i] ^ B[128*prev+i] ^ B[128*new_offset+i];
   }

void gen_2i_addresses(secure_vector<uint64_t>& T, secure_vector<uint64_t>& B,
                      size_t n, size_t lane, size_t slice, size_t memory,
                      size_t time, size_t mode, size_t cnt)
   {
   BOTAN_ASSERT_NOMSG(B.size() == 128);
   BOTAN_ASSERT_NOMSG(T.size() == 128);

   clear_mem(B.data(), B.size());
   B[0] = n;
   B[1] = lane;
   B[2] = slice;
   B[3] = memory;
   B[4] = time;
   B[5] = mode;
   B[6] = cnt;

   for(size_t r = 0; r != 2; ++r)
      {
      copy_mem(T.data(), B.data(), B.size());

      for(size_t i = 0; i != 128; i += 16)
         {
         blamka(T[i+ 0], T[i+ 1], T[i+ 2], T[i+ 3],
                T[i+ 4], T[i+ 5], T[i+ 6], T[i+ 7],
                T[i+ 8], T[i+ 9], T[i+10], T[i+11],
                T[i+12], T[i+13], T[i+14], T[i+15]);
         }
      for(size_t i = 0; i != 128 / 8; i += 2)
         {
         blamka(T[    i], T[    i+1], T[ 16+i], T[ 16+i+1],
                T[ 32+i], T[ 32+i+1], T[ 48+i], T[ 48+i+1],
                T[ 64+i], T[ 64+i+1], T[ 80+i], T[ 80+i+1],
                T[ 96+i], T[ 96+i+1], T[112+i], T[112+i+1]);
         }

      for(size_t i = 0; i != 128; ++i)
         B[i] ^= T[i];
      }
   }

uint32_t index_alpha(uint64_t random,
                     size_t lanes,
                     size_t segments,
                     size_t threads,
                     size_t n,
                     size_t slice,
                     size_t lane,
                     size_t index)
   {
   size_t ref_lane = static_cast<uint32_t>(random >> 32) % threads;

   if(n == 0 && slice == 0)
      ref_lane = lane;

   size_t m = 3*segments;
   size_t s = ((slice+1) % 4)*segments;

   if(lane == ref_lane)
      m += index;

   if(n == 0) {
         m = slice*segments;
         s = 0;
         if(slice == 0 || lane == ref_lane)
            m += index;
   }

   if(index == 0 || lane == ref_lane)
      m -= 1;

   uint64_t p = static_cast<uint32_t>(random);
   p = (p * p) >> 32;
   p = (p * m) >> 32;

   return static_cast<uint32_t>(ref_lane*lanes + (s + m - (p+1)) % lanes);
   }

void process_block_argon2d(secure_vector<uint64_t>& T,
                           secure_vector<uint64_t>& B,
                           size_t n, size_t slice, size_t lane,
                           size_t lanes, size_t segments, size_t threads)
   {
   size_t index = 0;
   if(n == 0 && slice == 0)
      index = 2;

   while(index < segments)
      {
      const size_t offset = lane*lanes + slice*segments + index;

      size_t prev = offset - 1;
      if(index == 0 && slice == 0)
         prev += lanes;

      const uint64_t random = B.at(128*prev);
      const size_t new_offset = index_alpha(random, lanes, segments, threads, n, slice, lane, index);

      process_block_xor(T, B, offset, prev, new_offset);

      index += 1;
      }
   }

void process_block_argon2i(secure_vector<uint64_t>& T,
                           secure_vector<uint64_t>& B,
                           size_t n, size_t slice, size_t lane,
                           size_t lanes, size_t segments, size_t threads, uint8_t mode,
                           size_t memory, size_t time)
   {
   size_t index = 0;
   if(n == 0 && slice == 0)
      index = 2;

   secure_vector<uint64_t> addresses(128);
   size_t address_counter = 1;

   gen_2i_addresses(T, addresses, n, lane, slice, memory, time, mode, address_counter);

   while(index < segments)
      {
      const size_t offset = lane*lanes + slice*segments + index;

      size_t prev = offset - 1;
      if(index == 0 && slice == 0)
         prev += lanes;

      if(index > 0 && index % 128 == 0)
         {
         address_counter += 1;
         gen_2i_addresses(T, addresses, n, lane, slice, memory, time, mode, address_counter);
         }

      const uint64_t random = addresses[index % 128];
      const size_t new_offset = index_alpha(random, lanes, segments, threads, n, slice, lane, index);

      process_block_xor(T, B, offset, prev, new_offset);

      index += 1;
      }
   }

void process_blocks(secure_vector<uint64_t>& B,
                    size_t t,
                    size_t memory,
                    size_t threads,
                    uint8_t mode)
   {
   const size_t lanes = memory / threads;
   const size_t segments = lanes / SYNC_POINTS;

   secure_vector<uint64_t> T(128);
   for(size_t n = 0; n != t; ++n)
      {
      for(size_t slice = 0; slice != SYNC_POINTS; ++slice)
         {
         // TODO can run this in Thread_Pool
         for(size_t lane = 0; lane != threads; ++lane)
            {
            if(mode == 1 || (mode == 2 && n == 0 && slice < SYNC_POINTS/2))
               process_block_argon2i(T, B, n, slice, lane, lanes, segments, threads, mode, memory, t);
            else
               process_block_argon2d(T, B, n, slice, lane, lanes, segments, threads);
            }
         }
      }

   }

}

void argon2(uint8_t output[], size_t output_len,
            const char* password, size_t password_len,
            const uint8_t salt[], size_t salt_len,
            const uint8_t key[], size_t key_len,
            const uint8_t ad[], size_t ad_len,
            uint8_t mode, size_t threads, size_t M, size_t t)
   {
   BOTAN_ARG_CHECK(mode == 0 || mode == 1 || mode == 2, "Unknown Argon2 mode parameter");
   BOTAN_ARG_CHECK(output_len >= 4, "Invalid Argon2 output length");
   BOTAN_ARG_CHECK(threads >= 1 && threads <= 128, "Invalid Argon2 threads parameter");
   BOTAN_ARG_CHECK(M >= 8*threads && M <= 8192*1024, "Invalid Argon2 M parameter");
   BOTAN_ARG_CHECK(t >= 1, "Invalid Argon2 t parameter");

   std::unique_ptr<HashFunction> blake2 = HashFunction::create_or_throw("BLAKE2b");

   const auto H0 = argon2_H0(*blake2, output_len,
                             password, password_len,
                             salt, salt_len,
                             key, key_len,
                             ad, ad_len,
                             mode, threads, M, t);

   const size_t memory = (M / (SYNC_POINTS*threads)) * (SYNC_POINTS*threads);

   secure_vector<uint64_t> B(memory * 1024/8);

   init_blocks(B, *blake2, H0, memory, threads);
   process_blocks(B, t, memory, threads, mode);

   clear_mem(output, output_len);
   extract_key(output, output_len, B, memory, threads);
   }

}
/**
* (C) 2019 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

std::string strip_padding(std::string s)
   {
   while(s.size() > 0 && s[s.size()-1] == '=')
      s.resize(s.size() - 1);
   return s;
   }

}

std::string argon2_generate_pwhash(const char* password, size_t password_len,
                                   RandomNumberGenerator& rng,
                                   size_t p, size_t M, size_t t,
                                   uint8_t y, size_t salt_len, size_t output_len)
   {
   std::vector<uint8_t> salt(salt_len);
   rng.randomize(salt.data(), salt.size());

   std::vector<uint8_t> output(output_len);
   argon2(output.data(), output.size(),
          password, password_len,
          salt.data(), salt.size(),
          nullptr, 0,
          nullptr, 0,
          y, p, M, t);

   std::ostringstream oss;

   if(y == 0)
      oss << "$argon2d$";
   else if(y == 1)
      oss << "$argon2i$";
   else
      oss << "$argon2id$";

   oss << "v=19$m=" << M << ",t=" << t << ",p=" << p << "$";
   oss << strip_padding(base64_encode(salt)) << "$" << strip_padding(base64_encode(output));

   return oss.str();
   }

bool argon2_check_pwhash(const char* password, size_t password_len,
                         const std::string& input_hash)
   {
   const std::vector<std::string> parts = split_on(input_hash, '$');

   if(parts.size() != 5)
      return false;

   uint8_t family = 0;

   if(parts[0] == "argon2d")
      family = 0;
   else if(parts[0] == "argon2i")
      family = 1;
   else if(parts[0] == "argon2id")
      family = 2;
   else
      return false;

   if(parts[1] != "v=19")
      return false;

   const std::vector<std::string> params = split_on(parts[2], ',');

   if(params.size() != 3)
      return false;

   size_t M = 0, t = 0, p = 0;

   for(auto param_str : params)
      {
      const std::vector<std::string> param = split_on(param_str, '=');

      if(param.size() != 2)
         return false;

      const std::string key = param[0];
      const size_t val = to_u32bit(param[1]);
      if(key == "m")
         M = val;
      else if(key == "t")
         t = val;
      else if(key == "p")
         p = val;
      else
         return false;
      }

   std::vector<uint8_t> salt(base64_decode_max_output(parts[3].size()));
   salt.resize(base64_decode(salt.data(), parts[3], false));

   std::vector<uint8_t> hash(base64_decode_max_output(parts[4].size()));
   hash.resize(base64_decode(hash.data(), parts[4], false));

   if(hash.size() < 4)
      return false;

   std::vector<uint8_t> generated(hash.size());
   argon2(generated.data(), generated.size(),
          password, password_len,
          salt.data(), salt.size(),
          nullptr, 0,
          nullptr, 0,
          family, p, M, t);

   return constant_time_compare(generated.data(), hash.data(), generated.size());
   }

}
/**
* (C) 2019 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <algorithm>

namespace Botan {

Argon2::Argon2(uint8_t family, size_t M, size_t t, size_t p) :
   m_family(family),
   m_M(M),
   m_t(t),
   m_p(p)
   {}

void Argon2::derive_key(uint8_t output[], size_t output_len,
                        const char* password, size_t password_len,
                        const uint8_t salt[], size_t salt_len) const
   {
   argon2(output, output_len,
          password, password_len,
          salt, salt_len,
          nullptr, 0,
          nullptr, 0,
          m_family, m_p, m_M, m_t);
   }

namespace {

std::string argon2_family_name(uint8_t f)
   {
   switch(f)
      {
      case 0:
         return "Argon2d";
      case 1:
         return "Argon2i";
      case 2:
         return "Argon2id";
      default:
         throw Invalid_Argument("Unknown Argon2 parameter");
      }
   }

}

std::string Argon2::to_string() const
   {
   return argon2_family_name(m_family) + "(" +
      std::to_string(m_M) + "," +
      std::to_string(m_t) + "," +
      std::to_string(m_p) + ")";
  }

Argon2_Family::Argon2_Family(uint8_t family) : m_family(family)
   {
   if(m_family != 0 && m_family != 1 && m_family != 2)
      throw Invalid_Argument("Unknown Argon2 family identifier");
   }

std::string Argon2_Family::name() const
   {
   return argon2_family_name(m_family);
   }

std::unique_ptr<PasswordHash> Argon2_Family::tune(size_t /*output_length*/,
                                                  std::chrono::milliseconds msec,
                                                  size_t max_memory) const
   {
   const size_t max_kib = (max_memory == 0) ? 256*1024 : max_memory*1024;

   // Tune with a large memory otherwise we measure cache vs RAM speeds and underestimate
   // costs for larger params. Default is 36 MiB, or use 128 for long times.
   const size_t tune_M = (msec >= std::chrono::milliseconds(500) ? 128 : 36) * 1024;
   const size_t p = 1;
   size_t t = 1;

   Timer timer("Argon2");
   const auto tune_time = BOTAN_PBKDF_TUNING_TIME;

   timer.run_until_elapsed(tune_time, [&]() {
      uint8_t output[64] = { 0 };
      argon2(output, sizeof(output), "test", 4, nullptr, 0, nullptr, 0, nullptr, 0, m_family, p, tune_M, t);
      });

   if(timer.events() == 0 || timer.value() == 0)
      return default_params();

   size_t M = 4*1024;

   const uint64_t measured_time = timer.value() / (timer.events() * (tune_M / M));

   const uint64_t target_nsec = msec.count() * static_cast<uint64_t>(1000000);

   /*
   * Argon2 scaling rules:
   * k*M, k*t, k*p all increase cost by about k
   *
   * Since we don't even take advantage of p > 1, we prefer increasing
   * t or M instead.
   *
   * If possible to increase M, prefer that.
   */

   uint64_t est_nsec = measured_time;

   if(est_nsec < target_nsec && M < max_kib)
      {
      const uint64_t desired_cost_increase = (target_nsec + est_nsec - 1) / est_nsec;
      const uint64_t mem_headroom = max_kib / M;

      const uint64_t M_mult = std::min(desired_cost_increase, mem_headroom);
      M *= static_cast<size_t>(M_mult);
      est_nsec *= M_mult;
      }

   if(est_nsec < target_nsec)
      {
      const uint64_t desired_cost_increase = (target_nsec + est_nsec - 1) / est_nsec;
      t *= static_cast<size_t>(desired_cost_increase);
      }

   return this->from_params(M, t, p);
   }

std::unique_ptr<PasswordHash> Argon2_Family::default_params() const
   {
   return this->from_params(128*1024, 1, 1);
   }

std::unique_ptr<PasswordHash> Argon2_Family::from_iterations(size_t iter) const
   {
   /*
   These choices are arbitrary, but should not change in future
   releases since they will break applications expecting deterministic
   mapping from iteration count to params
   */
   const size_t M = iter;
   const size_t t = 1;
   const size_t p = 1;
   return this->from_params(M, t, p);
   }

std::unique_ptr<PasswordHash> Argon2_Family::from_params(size_t M, size_t t, size_t p) const
   {
   return std::unique_ptr<PasswordHash>(new Argon2(m_family, M, t, p));
   }

}
/*
* (C) 2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_SYSTEM_RNG)
#endif

#if !defined(BOTAN_AUTO_RNG_HMAC)
#error "No hash function defined for AutoSeeded_RNG in build.h (try enabling sha2_32)"
#endif

namespace Botan {

AutoSeeded_RNG::~AutoSeeded_RNG()
   {
   // for unique_ptr
   }

AutoSeeded_RNG::AutoSeeded_RNG(RandomNumberGenerator& underlying_rng,
                               size_t reseed_interval)
   {
   m_rng.reset(new HMAC_DRBG(MessageAuthenticationCode::create_or_throw(BOTAN_AUTO_RNG_HMAC),
                             underlying_rng,
                             reseed_interval));
   force_reseed();
   }

AutoSeeded_RNG::AutoSeeded_RNG(Entropy_Sources& entropy_sources,
                               size_t reseed_interval)
   {
   m_rng.reset(new HMAC_DRBG(MessageAuthenticationCode::create_or_throw(BOTAN_AUTO_RNG_HMAC),
                             entropy_sources,
                             reseed_interval));
   force_reseed();
   }

AutoSeeded_RNG::AutoSeeded_RNG(RandomNumberGenerator& underlying_rng,
                               Entropy_Sources& entropy_sources,
                               size_t reseed_interval)
   {
   m_rng.reset(new HMAC_DRBG(
                  MessageAuthenticationCode::create_or_throw(BOTAN_AUTO_RNG_HMAC),
                  underlying_rng, entropy_sources, reseed_interval));
   force_reseed();
   }

AutoSeeded_RNG::AutoSeeded_RNG(size_t reseed_interval) :
#if defined(BOTAN_HAS_SYSTEM_RNG)
   AutoSeeded_RNG(system_rng(), reseed_interval)
#else
   AutoSeeded_RNG(Entropy_Sources::global_sources(), reseed_interval)
#endif
   {
   }

void AutoSeeded_RNG::force_reseed()
   {
   m_rng->force_reseed();
   m_rng->next_byte();

   if(!m_rng->is_seeded())
      {
      throw Internal_Error("AutoSeeded_RNG reseeding failed");
      }
   }

bool AutoSeeded_RNG::is_seeded() const
   {
   return m_rng->is_seeded();
   }

void AutoSeeded_RNG::clear()
   {
   m_rng->clear();
   }

std::string AutoSeeded_RNG::name() const
   {
   return m_rng->name();
   }

void AutoSeeded_RNG::add_entropy(const uint8_t in[], size_t len)
   {
   m_rng->add_entropy(in, len);
   }

size_t AutoSeeded_RNG::reseed(Entropy_Sources& srcs,
                              size_t poll_bits,
                              std::chrono::milliseconds poll_timeout)
   {
   return m_rng->reseed(srcs, poll_bits, poll_timeout);
   }

void AutoSeeded_RNG::randomize(uint8_t output[], size_t output_len)
   {
   m_rng->randomize_with_ts_input(output, output_len);
   }

void AutoSeeded_RNG::randomize_with_input(uint8_t output[], size_t output_len,
                                          const uint8_t ad[], size_t ad_len)
   {
   m_rng->randomize_with_input(output, output_len, ad, ad_len);
   }

}
/*
* (C) 2019 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void Buffered_Computation::update_be(uint16_t val)
   {
   uint8_t inb[sizeof(val)];
   store_be(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_be(uint32_t val)
   {
   uint8_t inb[sizeof(val)];
   store_be(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_be(uint64_t val)
   {
   uint8_t inb[sizeof(val)];
   store_be(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_le(uint16_t val)
   {
   uint8_t inb[sizeof(val)];
   store_le(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_le(uint32_t val)
   {
   uint8_t inb[sizeof(val)];
   store_le(val, inb);
   add_data(inb, sizeof(inb));
   }

void Buffered_Computation::update_le(uint64_t val)
   {
   uint8_t inb[sizeof(val)];
   store_le(val, inb);
   add_data(inb, sizeof(inb));
   }

}
/*
* SCAN Name Abstraction
* (C) 2008-2009,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

std::string make_arg(const std::vector<std::pair<size_t, std::string>>& name, size_t start)
   {
   std::string output = name[start].second;
   size_t level = name[start].first;

   size_t paren_depth = 0;

   for(size_t i = start + 1; i != name.size(); ++i)
      {
      if(name[i].first <= name[start].first)
         break;

      if(name[i].first > level)
         {
         output += "(" + name[i].second;
         ++paren_depth;
         }
      else if(name[i].first < level)
         {
         for (size_t j = name[i].first; j < level; j++) {
            output += ")";
            --paren_depth;
         }
         output += "," + name[i].second;
         }
      else
         {
         if(output[output.size() - 1] != '(')
            output += ",";
         output += name[i].second;
         }

      level = name[i].first;
      }

   for(size_t i = 0; i != paren_depth; ++i)
      output += ")";

   return output;
   }

}

SCAN_Name::SCAN_Name(const char* algo_spec) : SCAN_Name(std::string(algo_spec))
   {
   }

SCAN_Name::SCAN_Name(std::string algo_spec) : m_orig_algo_spec(algo_spec), m_alg_name(), m_args(), m_mode_info()
   { 
   if(algo_spec.size() == 0)
      throw Invalid_Argument("Expected algorithm name, got empty string");

   std::vector<std::pair<size_t, std::string>> name;
   size_t level = 0;
   std::pair<size_t, std::string> accum = std::make_pair(level, "");

   const std::string decoding_error = "Bad SCAN name '" + algo_spec + "': ";

   for(size_t i = 0; i != algo_spec.size(); ++i)
      {
      char c = algo_spec[i];

      if(c == '/' || c == ',' || c == '(' || c == ')')
         {
         if(c == '(')
            ++level;
         else if(c == ')')
            {
            if(level == 0)
               throw Decoding_Error(decoding_error + "Mismatched parens");
            --level;
            }

         if(c == '/' && level > 0)
            accum.second.push_back(c);
         else
            {
            if(accum.second != "")
               name.push_back(accum);
            accum = std::make_pair(level, "");
            }
         }
      else
         accum.second.push_back(c);
      }

   if(accum.second != "")
      name.push_back(accum);

   if(level != 0)
      throw Decoding_Error(decoding_error + "Missing close paren");

   if(name.size() == 0)
      throw Decoding_Error(decoding_error + "Empty name");

   m_alg_name = name[0].second;

   bool in_modes = false;

   for(size_t i = 1; i != name.size(); ++i)
      {
      if(name[i].first == 0)
         {
         m_mode_info.push_back(make_arg(name, i));
         in_modes = true;
         }
      else if(name[i].first == 1 && !in_modes)
         m_args.push_back(make_arg(name, i));
      }
   }

std::string SCAN_Name::arg(size_t i) const
   {
   if(i >= arg_count())
      throw Invalid_Argument("SCAN_Name::arg " + std::to_string(i) +
                             " out of range for '" + to_string() + "'");
   return m_args[i];
   }

std::string SCAN_Name::arg(size_t i, const std::string& def_value) const
   {
   if(i >= arg_count())
      return def_value;
   return m_args[i];
   }

size_t SCAN_Name::arg_as_integer(size_t i, size_t def_value) const
   {
   if(i >= arg_count())
      return def_value;
   return to_u32bit(m_args[i]);
   }

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void SymmetricAlgorithm::throw_key_not_set_error() const
   {
   throw Key_Not_Set(name());
   }

void SymmetricAlgorithm::set_key(const uint8_t key[], size_t length)
   {
   if(!valid_keylength(length))
      throw Invalid_Key_Length(name(), length);
   key_schedule(key, length);
   }

}
/*
* OctetString
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* Create an OctetString from RNG output
*/
OctetString::OctetString(RandomNumberGenerator& rng,
                         size_t len)
   {
   rng.random_vec(m_data, len);
   }

/*
* Create an OctetString from a hex string
*/
OctetString::OctetString(const std::string& hex_string)
   {
   if(!hex_string.empty())
      {
      m_data.resize(1 + hex_string.length() / 2);
      m_data.resize(hex_decode(m_data.data(), hex_string));
      }
   }

/*
* Create an OctetString from a byte string
*/
OctetString::OctetString(const uint8_t in[], size_t n)
   {
   m_data.assign(in, in + n);
   }

/*
* Set the parity of each key byte to odd
*/
void OctetString::set_odd_parity()
   {
   const uint8_t ODD_PARITY[256] = {
      0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x07, 0x07, 0x08, 0x08, 0x0B, 0x0B,
      0x0D, 0x0D, 0x0E, 0x0E, 0x10, 0x10, 0x13, 0x13, 0x15, 0x15, 0x16, 0x16,
      0x19, 0x19, 0x1A, 0x1A, 0x1C, 0x1C, 0x1F, 0x1F, 0x20, 0x20, 0x23, 0x23,
      0x25, 0x25, 0x26, 0x26, 0x29, 0x29, 0x2A, 0x2A, 0x2C, 0x2C, 0x2F, 0x2F,
      0x31, 0x31, 0x32, 0x32, 0x34, 0x34, 0x37, 0x37, 0x38, 0x38, 0x3B, 0x3B,
      0x3D, 0x3D, 0x3E, 0x3E, 0x40, 0x40, 0x43, 0x43, 0x45, 0x45, 0x46, 0x46,
      0x49, 0x49, 0x4A, 0x4A, 0x4C, 0x4C, 0x4F, 0x4F, 0x51, 0x51, 0x52, 0x52,
      0x54, 0x54, 0x57, 0x57, 0x58, 0x58, 0x5B, 0x5B, 0x5D, 0x5D, 0x5E, 0x5E,
      0x61, 0x61, 0x62, 0x62, 0x64, 0x64, 0x67, 0x67, 0x68, 0x68, 0x6B, 0x6B,
      0x6D, 0x6D, 0x6E, 0x6E, 0x70, 0x70, 0x73, 0x73, 0x75, 0x75, 0x76, 0x76,
      0x79, 0x79, 0x7A, 0x7A, 0x7C, 0x7C, 0x7F, 0x7F, 0x80, 0x80, 0x83, 0x83,
      0x85, 0x85, 0x86, 0x86, 0x89, 0x89, 0x8A, 0x8A, 0x8C, 0x8C, 0x8F, 0x8F,
      0x91, 0x91, 0x92, 0x92, 0x94, 0x94, 0x97, 0x97, 0x98, 0x98, 0x9B, 0x9B,
      0x9D, 0x9D, 0x9E, 0x9E, 0xA1, 0xA1, 0xA2, 0xA2, 0xA4, 0xA4, 0xA7, 0xA7,
      0xA8, 0xA8, 0xAB, 0xAB, 0xAD, 0xAD, 0xAE, 0xAE, 0xB0, 0xB0, 0xB3, 0xB3,
      0xB5, 0xB5, 0xB6, 0xB6, 0xB9, 0xB9, 0xBA, 0xBA, 0xBC, 0xBC, 0xBF, 0xBF,
      0xC1, 0xC1, 0xC2, 0xC2, 0xC4, 0xC4, 0xC7, 0xC7, 0xC8, 0xC8, 0xCB, 0xCB,
      0xCD, 0xCD, 0xCE, 0xCE, 0xD0, 0xD0, 0xD3, 0xD3, 0xD5, 0xD5, 0xD6, 0xD6,
      0xD9, 0xD9, 0xDA, 0xDA, 0xDC, 0xDC, 0xDF, 0xDF, 0xE0, 0xE0, 0xE3, 0xE3,
      0xE5, 0xE5, 0xE6, 0xE6, 0xE9, 0xE9, 0xEA, 0xEA, 0xEC, 0xEC, 0xEF, 0xEF,
      0xF1, 0xF1, 0xF2, 0xF2, 0xF4, 0xF4, 0xF7, 0xF7, 0xF8, 0xF8, 0xFB, 0xFB,
      0xFD, 0xFD, 0xFE, 0xFE };

   for(size_t j = 0; j != m_data.size(); ++j)
      m_data[j] = ODD_PARITY[m_data[j]];
   }

/*
* Hex encode an OctetString
*/
std::string OctetString::to_string() const
   {
   return hex_encode(m_data.data(), m_data.size());
   }

/*
* XOR Operation for OctetStrings
*/
OctetString& OctetString::operator^=(const OctetString& k)
   {
   if(&k == this) { zeroise(m_data); return (*this); }
   xor_buf(m_data.data(), k.begin(), std::min(length(), k.length()));
   return (*this);
   }

/*
* Equality Operation for OctetStrings
*/
bool operator==(const OctetString& s1, const OctetString& s2)
   {
   return (s1.bits_of() == s2.bits_of());
   }

/*
* Unequality Operation for OctetStrings
*/
bool operator!=(const OctetString& s1, const OctetString& s2)
   {
   return !(s1 == s2);
   }

/*
* Append Operation for OctetStrings
*/
OctetString operator+(const OctetString& k1, const OctetString& k2)
   {
   secure_vector<uint8_t> out;
   out += k1.bits_of();
   out += k2.bits_of();
   return OctetString(out);
   }

/*
* XOR Operation for OctetStrings
*/
OctetString operator^(const OctetString& k1, const OctetString& k2)
   {
   secure_vector<uint8_t> out(std::max(k1.length(), k2.length()));

   copy_mem(out.data(), k1.begin(), k1.length());
   xor_buf(out.data(), k2.begin(), k2.length());
   return OctetString(out);
   }

}
/*
* Base64 Encoding and Decoding
* (C) 2010,2015,2020 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

class Base64 final
   {
   public:
      static inline std::string name() noexcept
         {
         return "base64";
         }

      static inline size_t encoding_bytes_in() noexcept
         {
         return m_encoding_bytes_in;
         }
      static inline size_t encoding_bytes_out() noexcept
         {
         return m_encoding_bytes_out;
         }

      static inline size_t decoding_bytes_in() noexcept
         {
         return m_encoding_bytes_out;
         }
      static inline size_t decoding_bytes_out() noexcept
         {
         return m_encoding_bytes_in;
         }

      static inline size_t bits_consumed() noexcept
         {
         return m_encoding_bits;
         }
      static inline size_t remaining_bits_before_padding() noexcept
         {
         return m_remaining_bits_before_padding;
         }

      static inline size_t encode_max_output(size_t input_length)
         {
         return (round_up(input_length, m_encoding_bytes_in) / m_encoding_bytes_in) * m_encoding_bytes_out;
         }
      static inline size_t decode_max_output(size_t input_length)
         {
         return (round_up(input_length, m_encoding_bytes_out) * m_encoding_bytes_in) / m_encoding_bytes_out;
         }

      static void encode(char out[8], const uint8_t in[5]) noexcept;

      static uint8_t lookup_binary_value(char input) noexcept;

      static bool check_bad_char(uint8_t bin, char input, bool ignore_ws);

      static void decode(uint8_t* out_ptr, const uint8_t decode_buf[4])
         {
         out_ptr[0] = (decode_buf[0] << 2) | (decode_buf[1] >> 4);
         out_ptr[1] = (decode_buf[1] << 4) | (decode_buf[2] >> 2);
         out_ptr[2] = (decode_buf[2] << 6) | decode_buf[3];
         }

      static inline size_t bytes_to_remove(size_t final_truncate)
         {
         return final_truncate;
         }

   private:
      static const size_t m_encoding_bits = 6;
      static const size_t m_remaining_bits_before_padding = 8;

      static const size_t m_encoding_bytes_in = 3;
      static const size_t m_encoding_bytes_out = 4;
   };

char lookup_base64_char(uint8_t x)
   {
   BOTAN_DEBUG_ASSERT(x < 64);

   const auto in_az = CT::Mask<uint8_t>::is_within_range(x, 26, 51);
   const auto in_09 = CT::Mask<uint8_t>::is_within_range(x, 52, 61);
   const auto eq_plus = CT::Mask<uint8_t>::is_equal(x, 62);
   const auto eq_slash = CT::Mask<uint8_t>::is_equal(x, 63);

   const char c_AZ = 'A' + x;
   const char c_az = 'a' + (x - 26);
   const char c_09 = '0' + (x - 2*26);
   const char c_plus = '+';
   const char c_slash = '/';

   char ret = c_AZ;
   ret = in_az.select(c_az, ret);
   ret = in_09.select(c_09, ret);
   ret = eq_plus.select(c_plus, ret);
   ret = eq_slash.select(c_slash, ret);

   return ret;
   }

//static
void Base64::encode(char out[8], const uint8_t in[5]) noexcept
   {
   const uint8_t b0 = (in[0] & 0xFC) >> 2;
   const uint8_t b1 = ((in[0] & 0x03) << 4) | (in[1] >> 4);
   const uint8_t b2 = ((in[1] & 0x0F) << 2) | (in[2] >> 6);
   const uint8_t b3 = in[2] & 0x3F;
   out[0] = lookup_base64_char(b0);
   out[1] = lookup_base64_char(b1);
   out[2] = lookup_base64_char(b2);
   out[3] = lookup_base64_char(b3);
   }

//static
uint8_t Base64::lookup_binary_value(char input) noexcept
   {
   const uint8_t c = static_cast<uint8_t>(input);

   const auto is_alpha_upper = CT::Mask<uint8_t>::is_within_range(c, uint8_t('A'), uint8_t('Z'));
   const auto is_alpha_lower = CT::Mask<uint8_t>::is_within_range(c, uint8_t('a'), uint8_t('z'));
   const auto is_decimal     = CT::Mask<uint8_t>::is_within_range(c, uint8_t('0'), uint8_t('9'));

   const auto is_plus        = CT::Mask<uint8_t>::is_equal(c, uint8_t('+'));
   const auto is_slash       = CT::Mask<uint8_t>::is_equal(c, uint8_t('/'));
   const auto is_equal       = CT::Mask<uint8_t>::is_equal(c, uint8_t('='));

   const auto is_whitespace  = CT::Mask<uint8_t>::is_any_of(c, {
         uint8_t(' '), uint8_t('\t'), uint8_t('\n'), uint8_t('\r')
      });

   const uint8_t c_upper = c - uint8_t('A');
   const uint8_t c_lower = c - uint8_t('a') + 26;
   const uint8_t c_decim = c - uint8_t('0') + 2*26;

   uint8_t ret = 0xFF; // default value

   ret = is_alpha_upper.select(c_upper, ret);
   ret = is_alpha_lower.select(c_lower, ret);
   ret = is_decimal.select(c_decim, ret);
   ret = is_plus.select(62, ret);
   ret = is_slash.select(63, ret);
   ret = is_equal.select(0x81, ret);
   ret = is_whitespace.select(0x80, ret);

   return ret;
   }

//static
bool Base64::check_bad_char(uint8_t bin, char input, bool ignore_ws)
   {
   if(bin <= 0x3F)
      {
      return true;
      }
   else if(!(bin == 0x81 || (bin == 0x80 && ignore_ws)))
      {
      std::string bad_char(1, input);
      if(bad_char == "\t")
         { bad_char = "\\t"; }
      else if(bad_char == "\n")
         { bad_char = "\\n"; }
      else if(bad_char == "\r")
         { bad_char = "\\r"; }

      throw Invalid_Argument(
         std::string("base64_decode: invalid base64 character '") +
         bad_char + "'");
      }
   return false;
   }

}

size_t base64_encode(char out[],
                     const uint8_t in[],
                     size_t input_length,
                     size_t& input_consumed,
                     bool final_inputs)
   {
   return base_encode(Base64(), out, in, input_length, input_consumed, final_inputs);
   }

std::string base64_encode(const uint8_t input[],
                          size_t input_length)
   {
   return base_encode_to_string(Base64(), input, input_length);
   }

size_t base64_decode(uint8_t out[],
                     const char in[],
                     size_t input_length,
                     size_t& input_consumed,
                     bool final_inputs,
                     bool ignore_ws)
   {
   return base_decode(Base64(), out, in, input_length, input_consumed, final_inputs, ignore_ws);
   }

size_t base64_decode(uint8_t output[],
                     const char input[],
                     size_t input_length,
                     bool ignore_ws)
   {
   return base_decode_full(Base64(), output, input, input_length, ignore_ws);
   }

size_t base64_decode(uint8_t output[],
                     const std::string& input,
                     bool ignore_ws)
   {
   return base64_decode(output, input.data(), input.length(), ignore_ws);
   }

secure_vector<uint8_t> base64_decode(const char input[],
                                     size_t input_length,
                                     bool ignore_ws)
   {
   return base_decode_to_vec<secure_vector<uint8_t>>(Base64(), input, input_length, ignore_ws);
   }

secure_vector<uint8_t> base64_decode(const std::string& input,
                                     bool ignore_ws)
   {
   return base64_decode(input.data(), input.size(), ignore_ws);
   }

size_t base64_encode_max_output(size_t input_length)
   {
   return Base64::encode_max_output(input_length);
   }

size_t base64_decode_max_output(size_t input_length)
   {
   return Base64::decode_max_output(input_length);
   }

}
/*
* BLAKE2b
* (C) 2016 cynecx
* (C) 2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

enum blake2b_constant {
  BLAKE2B_BLOCKBYTES = 128,
  BLAKE2B_IVU64COUNT = 8
};

const uint64_t blake2b_IV[BLAKE2B_IVU64COUNT] = {
   0x6a09e667f3bcc908, 0xbb67ae8584caa73b,
   0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
   0x510e527fade682d1, 0x9b05688c2b3e6c1f,
   0x1f83d9abfb41bd6b, 0x5be0cd19137e2179
};

}

BLAKE2b::BLAKE2b(size_t output_bits) :
   m_output_bits(output_bits),
   m_buffer(BLAKE2B_BLOCKBYTES),
   m_bufpos(0),
   m_H(BLAKE2B_IVU64COUNT)
   {
   if(output_bits == 0 || output_bits > 512 || output_bits % 8 != 0)
      {
      throw Invalid_Argument("Bad output bits size for BLAKE2b");
      }

   state_init();
   }

void BLAKE2b::state_init()
   {
   copy_mem(m_H.data(), blake2b_IV, BLAKE2B_IVU64COUNT);
   m_H[0] ^= 0x01010000 ^ static_cast<uint8_t>(output_length());
   m_T[0] = m_T[1] = 0;
   m_F[0] = m_F[1] = 0;
   m_bufpos = 0;
   }

namespace {

BOTAN_FORCE_INLINE void G(uint64_t& a, uint64_t& b, uint64_t& c, uint64_t& d,
                          uint64_t M0, uint64_t M1)
   {
   a = a + b + M0;
   d = rotr<32>(d ^ a);
   c = c + d;
   b = rotr<24>(b ^ c);
   a = a + b + M1;
   d = rotr<16>(d ^ a);
   c = c + d;
   b = rotr<63>(b ^ c);
   }

template<size_t i0, size_t i1, size_t i2, size_t i3, size_t i4, size_t i5, size_t i6, size_t i7,
         size_t i8, size_t i9, size_t iA, size_t iB, size_t iC, size_t iD, size_t iE, size_t iF>
BOTAN_FORCE_INLINE void ROUND(uint64_t* v, const uint64_t* M)
   {
   G(v[ 0], v[ 4], v[ 8], v[12], M[i0], M[i1]);
   G(v[ 1], v[ 5], v[ 9], v[13], M[i2], M[i3]);
   G(v[ 2], v[ 6], v[10], v[14], M[i4], M[i5]);
   G(v[ 3], v[ 7], v[11], v[15], M[i6], M[i7]);
   G(v[ 0], v[ 5], v[10], v[15], M[i8], M[i9]);
   G(v[ 1], v[ 6], v[11], v[12], M[iA], M[iB]);
   G(v[ 2], v[ 7], v[ 8], v[13], M[iC], M[iD]);
   G(v[ 3], v[ 4], v[ 9], v[14], M[iE], M[iF]);
   }


}

void BLAKE2b::compress(const uint8_t* input, size_t blocks, uint64_t increment)
   {
   for(size_t b = 0; b != blocks; ++b)
      {
      m_T[0] += increment;
      if(m_T[0] < increment)
         {
         m_T[1]++;
         }

      uint64_t M[16];
      uint64_t v[16];
      load_le(M, input, 16);

      input += BLAKE2B_BLOCKBYTES;

      for(size_t i = 0; i < 8; i++)
         v[i] = m_H[i];
      for(size_t i = 0; i != 8; ++i)
         v[i + 8] = blake2b_IV[i];

      v[12] ^= m_T[0];
      v[13] ^= m_T[1];
      v[14] ^= m_F[0];
      v[15] ^= m_F[1];

      ROUND< 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15>(v, M);
      ROUND<14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3>(v, M);
      ROUND<11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4>(v, M);
      ROUND< 7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8>(v, M);
      ROUND< 9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13>(v, M);
      ROUND< 2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9>(v, M);
      ROUND<12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11>(v, M);
      ROUND<13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10>(v, M);
      ROUND< 6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5>(v, M);
      ROUND<10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0>(v, M);
      ROUND< 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15>(v, M);
      ROUND<14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3>(v, M);

      for(size_t i = 0; i < 8; i++)
         {
         m_H[i] ^= v[i] ^ v[i + 8];
         }
      }
   }

void BLAKE2b::add_data(const uint8_t input[], size_t length)
   {
   if(length == 0)
      return;

   if(m_bufpos > 0)
      {
      if(m_bufpos < BLAKE2B_BLOCKBYTES)
         {
         const size_t take = std::min(BLAKE2B_BLOCKBYTES - m_bufpos, length);
         copy_mem(&m_buffer[m_bufpos], input, take);
         m_bufpos += take;
         length -= take;
         input += take;
         }

      if(m_bufpos == m_buffer.size() && length > 0)
         {
         compress(m_buffer.data(), 1, BLAKE2B_BLOCKBYTES);
         m_bufpos = 0;
         }
      }

   if(length > BLAKE2B_BLOCKBYTES)
      {
      const size_t full_blocks = ((length-1) / BLAKE2B_BLOCKBYTES);
      compress(input, full_blocks, BLAKE2B_BLOCKBYTES);

      input += full_blocks * BLAKE2B_BLOCKBYTES;
      length -= full_blocks * BLAKE2B_BLOCKBYTES;
      }

   if(length > 0)
      {
      copy_mem(&m_buffer[m_bufpos], input, length);
      m_bufpos += length;
      }
   }

void BLAKE2b::final_result(uint8_t output[])
   {
   if(m_bufpos != BLAKE2B_BLOCKBYTES)
      clear_mem(&m_buffer[m_bufpos], BLAKE2B_BLOCKBYTES - m_bufpos);
   m_F[0] = 0xFFFFFFFFFFFFFFFF;
   compress(m_buffer.data(), 1, m_bufpos);
   copy_out_vec_le(output, output_length(), m_H);
   state_init();
   }

std::string BLAKE2b::name() const
   {
   return "BLAKE2b(" + std::to_string(m_output_bits) + ")";
   }

HashFunction* BLAKE2b::clone() const
   {
   return new BLAKE2b(m_output_bits);
   }

std::unique_ptr<HashFunction> BLAKE2b::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new BLAKE2b(*this));
   }

void BLAKE2b::clear()
   {
   zeroise(m_H);
   zeroise(m_buffer);
   m_bufpos = 0;
   state_init();
   }

}
/*
* Block Ciphers
* (C) 2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_AES)
#endif

#if defined(BOTAN_HAS_ARIA)
#endif

#if defined(BOTAN_HAS_BLOWFISH)
#endif

#if defined(BOTAN_HAS_CAMELLIA)
#endif

#if defined(BOTAN_HAS_CAST_128)
#endif

#if defined(BOTAN_HAS_CAST_256)
#endif

#if defined(BOTAN_HAS_CASCADE)
#endif

#if defined(BOTAN_HAS_DES)
#endif

#if defined(BOTAN_HAS_GOST_28147_89)
#endif

#if defined(BOTAN_HAS_IDEA)
#endif

#if defined(BOTAN_HAS_KASUMI)
#endif

#if defined(BOTAN_HAS_LION)
#endif

#if defined(BOTAN_HAS_MISTY1)
#endif

#if defined(BOTAN_HAS_NOEKEON)
#endif

#if defined(BOTAN_HAS_SEED)
#endif

#if defined(BOTAN_HAS_SERPENT)
#endif

#if defined(BOTAN_HAS_SHACAL2)
#endif

#if defined(BOTAN_HAS_SM4)
#endif

#if defined(BOTAN_HAS_TWOFISH)
#endif

#if defined(BOTAN_HAS_THREEFISH_512)
#endif

#if defined(BOTAN_HAS_XTEA)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

#if defined(BOTAN_HAS_COMMONCRYPTO)
#endif

namespace Botan {

std::unique_ptr<BlockCipher>
BlockCipher::create(const std::string& algo,
                    const std::string& provider)
   {
#if defined(BOTAN_HAS_COMMONCRYPTO)
   if(provider.empty() || provider == "commoncrypto")
      {
      if(auto bc = make_commoncrypto_block_cipher(algo))
         return bc;

      if(!provider.empty())
         return nullptr;
      }
#endif

#if defined(BOTAN_HAS_OPENSSL)
   if(provider.empty() || provider == "openssl")
      {
      if(auto bc = make_openssl_block_cipher(algo))
         return bc;

      if(!provider.empty())
         return nullptr;
      }
#endif

   // TODO: CryptoAPI
   // TODO: /dev/crypto

   // Only base providers from here on out
   if(provider.empty() == false && provider != "base")
      return nullptr;

#if defined(BOTAN_HAS_AES)
   if(algo == "AES-128")
      {
      return std::unique_ptr<BlockCipher>(new AES_128);
      }

   if(algo == "AES-192")
      {
      return std::unique_ptr<BlockCipher>(new AES_192);
      }

   if(algo == "AES-256")
      {
      return std::unique_ptr<BlockCipher>(new AES_256);
      }
#endif

#if defined(BOTAN_HAS_ARIA)
   if(algo == "ARIA-128")
      {
      return std::unique_ptr<BlockCipher>(new ARIA_128);
      }

   if(algo == "ARIA-192")
      {
      return std::unique_ptr<BlockCipher>(new ARIA_192);
      }

   if(algo == "ARIA-256")
      {
      return std::unique_ptr<BlockCipher>(new ARIA_256);
      }
#endif

#if defined(BOTAN_HAS_SERPENT)
   if(algo == "Serpent")
      {
      return std::unique_ptr<BlockCipher>(new Serpent);
      }
#endif

#if defined(BOTAN_HAS_SHACAL2)
   if(algo == "SHACAL2")
      {
      return std::unique_ptr<BlockCipher>(new SHACAL2);
      }
#endif

#if defined(BOTAN_HAS_TWOFISH)
   if(algo == "Twofish")
      {
      return std::unique_ptr<BlockCipher>(new Twofish);
      }
#endif

#if defined(BOTAN_HAS_THREEFISH_512)
   if(algo == "Threefish-512")
      {
      return std::unique_ptr<BlockCipher>(new Threefish_512);
      }
#endif

#if defined(BOTAN_HAS_BLOWFISH)
   if(algo == "Blowfish")
      {
      return std::unique_ptr<BlockCipher>(new Blowfish);
      }
#endif

#if defined(BOTAN_HAS_CAMELLIA)
   if(algo == "Camellia-128")
      {
      return std::unique_ptr<BlockCipher>(new Camellia_128);
      }

   if(algo == "Camellia-192")
      {
      return std::unique_ptr<BlockCipher>(new Camellia_192);
      }

   if(algo == "Camellia-256")
      {
      return std::unique_ptr<BlockCipher>(new Camellia_256);
      }
#endif

#if defined(BOTAN_HAS_DES)
   if(algo == "DES")
      {
      return std::unique_ptr<BlockCipher>(new DES);
      }

   if(algo == "DESX")
      {
      return std::unique_ptr<BlockCipher>(new DESX);
      }

   if(algo == "TripleDES" || algo == "3DES" || algo == "DES-EDE")
      {
      return std::unique_ptr<BlockCipher>(new TripleDES);
      }
#endif

#if defined(BOTAN_HAS_NOEKEON)
   if(algo == "Noekeon")
      {
      return std::unique_ptr<BlockCipher>(new Noekeon);
      }
#endif

#if defined(BOTAN_HAS_CAST_128)
   if(algo == "CAST-128" || algo == "CAST5")
      {
      return std::unique_ptr<BlockCipher>(new CAST_128);
      }
#endif

#if defined(BOTAN_HAS_CAST_256)
   if(algo == "CAST-256")
      {
      return std::unique_ptr<BlockCipher>(new CAST_256);
      }
#endif

#if defined(BOTAN_HAS_IDEA)
   if(algo == "IDEA")
      {
      return std::unique_ptr<BlockCipher>(new IDEA);
      }
#endif

#if defined(BOTAN_HAS_KASUMI)
   if(algo == "KASUMI")
      {
      return std::unique_ptr<BlockCipher>(new KASUMI);
      }
#endif

#if defined(BOTAN_HAS_MISTY1)
   if(algo == "MISTY1")
      {
      return std::unique_ptr<BlockCipher>(new MISTY1);
      }
#endif

#if defined(BOTAN_HAS_SEED)
   if(algo == "SEED")
      {
      return std::unique_ptr<BlockCipher>(new SEED);
      }
#endif

#if defined(BOTAN_HAS_SM4)
   if(algo == "SM4")
      {
      return std::unique_ptr<BlockCipher>(new SM4);
      }
#endif

#if defined(BOTAN_HAS_XTEA)
   if(algo == "XTEA")
      {
      return std::unique_ptr<BlockCipher>(new XTEA);
      }
#endif

   const SCAN_Name req(algo);

#if defined(BOTAN_HAS_GOST_28147_89)
   if(req.algo_name() == "GOST-28147-89")
      {
      return std::unique_ptr<BlockCipher>(new GOST_28147_89(req.arg(0, "R3411_94_TestParam")));
      }
#endif

#if defined(BOTAN_HAS_CASCADE)
   if(req.algo_name() == "Cascade" && req.arg_count() == 2)
      {
      std::unique_ptr<BlockCipher> c1(BlockCipher::create(req.arg(0)));
      std::unique_ptr<BlockCipher> c2(BlockCipher::create(req.arg(1)));

      if(c1 && c2)
         return std::unique_ptr<BlockCipher>(new Cascade_Cipher(c1.release(), c2.release()));
      }
#endif

#if defined(BOTAN_HAS_LION)
   if(req.algo_name() == "Lion" && req.arg_count_between(2, 3))
      {
      std::unique_ptr<HashFunction> hash(HashFunction::create(req.arg(0)));
      std::unique_ptr<StreamCipher> stream(StreamCipher::create(req.arg(1)));

      if(hash && stream)
         {
         const size_t block_size = req.arg_as_integer(2, 1024);
         return std::unique_ptr<BlockCipher>(new Lion(hash.release(), stream.release(), block_size));
         }
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<BlockCipher>
BlockCipher::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto bc = BlockCipher::create(algo, provider))
      {
      return bc;
      }
   throw Lookup_Error("Block cipher", algo, provider);
   }

std::vector<std::string> BlockCipher::providers(const std::string& algo)
   {
   return probe_providers_of<BlockCipher>(algo, { "base", "openssl", "commoncrypto" });
   }

}
/*
* ChaCha
* (C) 2014,2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

#define CHACHA_QUARTER_ROUND(a, b, c, d) \
      do {                               \
      a += b; d ^= a; d = rotl<16>(d);   \
      c += d; b ^= c; b = rotl<12>(b);   \
      a += b; d ^= a; d = rotl<8>(d);    \
      c += d; b ^= c; b = rotl<7>(b);    \
      } while(0)

/*
* Generate HChaCha cipher stream (for XChaCha IV setup)
*/
void hchacha(uint32_t output[8], const uint32_t input[16], size_t rounds)
   {
   BOTAN_ASSERT(rounds % 2 == 0, "Valid rounds");

   uint32_t x00 = input[ 0], x01 = input[ 1], x02 = input[ 2], x03 = input[ 3],
            x04 = input[ 4], x05 = input[ 5], x06 = input[ 6], x07 = input[ 7],
            x08 = input[ 8], x09 = input[ 9], x10 = input[10], x11 = input[11],
            x12 = input[12], x13 = input[13], x14 = input[14], x15 = input[15];

   for(size_t i = 0; i != rounds / 2; ++i)
      {
      CHACHA_QUARTER_ROUND(x00, x04, x08, x12);
      CHACHA_QUARTER_ROUND(x01, x05, x09, x13);
      CHACHA_QUARTER_ROUND(x02, x06, x10, x14);
      CHACHA_QUARTER_ROUND(x03, x07, x11, x15);

      CHACHA_QUARTER_ROUND(x00, x05, x10, x15);
      CHACHA_QUARTER_ROUND(x01, x06, x11, x12);
      CHACHA_QUARTER_ROUND(x02, x07, x08, x13);
      CHACHA_QUARTER_ROUND(x03, x04, x09, x14);
      }

   output[0] = x00;
   output[1] = x01;
   output[2] = x02;
   output[3] = x03;
   output[4] = x12;
   output[5] = x13;
   output[6] = x14;
   output[7] = x15;
   }

}

ChaCha::ChaCha(size_t rounds) : m_rounds(rounds)
   {
   BOTAN_ARG_CHECK(m_rounds == 8 || m_rounds == 12 || m_rounds == 20,
                   "ChaCha only supports 8, 12 or 20 rounds");
   }

std::string ChaCha::provider() const
   {
#if defined(BOTAN_HAS_CHACHA_AVX2)
   if(CPUID::has_avx2())
      {
      return "avx2";
      }
#endif

#if defined(BOTAN_HAS_CHACHA_SIMD32)
   if(CPUID::has_simd_32())
      {
      return "simd32";
      }
#endif

   return "base";
   }

//static
void ChaCha::chacha_x8(uint8_t output[64*8], uint32_t input[16], size_t rounds)
   {
   BOTAN_ASSERT(rounds % 2 == 0, "Valid rounds");

#if defined(BOTAN_HAS_CHACHA_AVX2)
   if(CPUID::has_avx2())
      {
      return ChaCha::chacha_avx2_x8(output, input, rounds);
      }
#endif

#if defined(BOTAN_HAS_CHACHA_SIMD32)
   if(CPUID::has_simd_32())
      {
      ChaCha::chacha_simd32_x4(output, input, rounds);
      ChaCha::chacha_simd32_x4(output + 4*64, input, rounds);
      return;
      }
#endif

   // TODO interleave rounds
   for(size_t i = 0; i != 8; ++i)
      {
      uint32_t x00 = input[ 0], x01 = input[ 1], x02 = input[ 2], x03 = input[ 3],
               x04 = input[ 4], x05 = input[ 5], x06 = input[ 6], x07 = input[ 7],
               x08 = input[ 8], x09 = input[ 9], x10 = input[10], x11 = input[11],
               x12 = input[12], x13 = input[13], x14 = input[14], x15 = input[15];

      for(size_t r = 0; r != rounds / 2; ++r)
         {
         CHACHA_QUARTER_ROUND(x00, x04, x08, x12);
         CHACHA_QUARTER_ROUND(x01, x05, x09, x13);
         CHACHA_QUARTER_ROUND(x02, x06, x10, x14);
         CHACHA_QUARTER_ROUND(x03, x07, x11, x15);

         CHACHA_QUARTER_ROUND(x00, x05, x10, x15);
         CHACHA_QUARTER_ROUND(x01, x06, x11, x12);
         CHACHA_QUARTER_ROUND(x02, x07, x08, x13);
         CHACHA_QUARTER_ROUND(x03, x04, x09, x14);
         }

      x00 += input[0];
      x01 += input[1];
      x02 += input[2];
      x03 += input[3];
      x04 += input[4];
      x05 += input[5];
      x06 += input[6];
      x07 += input[7];
      x08 += input[8];
      x09 += input[9];
      x10 += input[10];
      x11 += input[11];
      x12 += input[12];
      x13 += input[13];
      x14 += input[14];
      x15 += input[15];

      store_le(x00, output + 64 * i + 4 *  0);
      store_le(x01, output + 64 * i + 4 *  1);
      store_le(x02, output + 64 * i + 4 *  2);
      store_le(x03, output + 64 * i + 4 *  3);
      store_le(x04, output + 64 * i + 4 *  4);
      store_le(x05, output + 64 * i + 4 *  5);
      store_le(x06, output + 64 * i + 4 *  6);
      store_le(x07, output + 64 * i + 4 *  7);
      store_le(x08, output + 64 * i + 4 *  8);
      store_le(x09, output + 64 * i + 4 *  9);
      store_le(x10, output + 64 * i + 4 * 10);
      store_le(x11, output + 64 * i + 4 * 11);
      store_le(x12, output + 64 * i + 4 * 12);
      store_le(x13, output + 64 * i + 4 * 13);
      store_le(x14, output + 64 * i + 4 * 14);
      store_le(x15, output + 64 * i + 4 * 15);

      input[12]++;
      input[13] += (input[12] == 0);
      }
   }

#undef CHACHA_QUARTER_ROUND

/*
* Combine cipher stream with message
*/
void ChaCha::cipher(const uint8_t in[], uint8_t out[], size_t length)
   {
   verify_key_set(m_state.empty() == false);

   while(length >= m_buffer.size() - m_position)
      {
      const size_t available = m_buffer.size() - m_position;

      xor_buf(out, in, &m_buffer[m_position], available);
      chacha_x8(m_buffer.data(), m_state.data(), m_rounds);

      length -= available;
      in += available;
      out += available;
      m_position = 0;
      }

   xor_buf(out, in, &m_buffer[m_position], length);

   m_position += length;
   }

void ChaCha::write_keystream(uint8_t out[], size_t length)
   {
   verify_key_set(m_state.empty() == false);

   while(length >= m_buffer.size() - m_position)
      {
      const size_t available = m_buffer.size() - m_position;

      copy_mem(out, &m_buffer[m_position], available);
      chacha_x8(m_buffer.data(), m_state.data(), m_rounds);

      length -= available;
      out += available;
      m_position = 0;
      }

   copy_mem(out, &m_buffer[m_position], length);

   m_position += length;
   }

void ChaCha::initialize_state()
   {
   static const uint32_t TAU[] =
      { 0x61707865, 0x3120646e, 0x79622d36, 0x6b206574 };

   static const uint32_t SIGMA[] =
      { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };

   m_state[4] = m_key[0];
   m_state[5] = m_key[1];
   m_state[6] = m_key[2];
   m_state[7] = m_key[3];

   if(m_key.size() == 4)
      {
      m_state[0] = TAU[0];
      m_state[1] = TAU[1];
      m_state[2] = TAU[2];
      m_state[3] = TAU[3];

      m_state[8] = m_key[0];
      m_state[9] = m_key[1];
      m_state[10] = m_key[2];
      m_state[11] = m_key[3];
      }
   else
      {
      m_state[0] = SIGMA[0];
      m_state[1] = SIGMA[1];
      m_state[2] = SIGMA[2];
      m_state[3] = SIGMA[3];

      m_state[8] = m_key[4];
      m_state[9] = m_key[5];
      m_state[10] = m_key[6];
      m_state[11] = m_key[7];
      }

   m_state[12] = 0;
   m_state[13] = 0;
   m_state[14] = 0;
   m_state[15] = 0;

   m_position = 0;
   }

/*
* ChaCha Key Schedule
*/
void ChaCha::key_schedule(const uint8_t key[], size_t length)
   {
   m_key.resize(length / 4);
   load_le<uint32_t>(m_key.data(), key, m_key.size());

   m_state.resize(16);

   const size_t chacha_parallelism = 8; // chacha_x8
   const size_t chacha_block = 64;
   m_buffer.resize(chacha_parallelism * chacha_block);

   set_iv(nullptr, 0);
   }

size_t ChaCha::default_iv_length() const
   {
   return 24;
   }

Key_Length_Specification ChaCha::key_spec() const
   {
   return Key_Length_Specification(16, 32, 16);
   }

StreamCipher* ChaCha::clone() const
   {
   return new ChaCha(m_rounds);
   }

bool ChaCha::valid_iv_length(size_t iv_len) const
   {
   return (iv_len == 0 || iv_len == 8 || iv_len == 12 || iv_len == 24);
   }

void ChaCha::set_iv(const uint8_t iv[], size_t length)
   {
   verify_key_set(m_state.empty() == false);

   if(!valid_iv_length(length))
      throw Invalid_IV_Length(name(), length);

   initialize_state();

   if(length == 0)
      {
      // Treat zero length IV same as an all-zero IV
      m_state[14] = 0;
      m_state[15] = 0;
      }
   else if(length == 8)
      {
      m_state[14] = load_le<uint32_t>(iv, 0);
      m_state[15] = load_le<uint32_t>(iv, 1);
      }
   else if(length == 12)
      {
      m_state[13] = load_le<uint32_t>(iv, 0);
      m_state[14] = load_le<uint32_t>(iv, 1);
      m_state[15] = load_le<uint32_t>(iv, 2);
      }
   else if(length == 24)
      {
      m_state[12] = load_le<uint32_t>(iv, 0);
      m_state[13] = load_le<uint32_t>(iv, 1);
      m_state[14] = load_le<uint32_t>(iv, 2);
      m_state[15] = load_le<uint32_t>(iv, 3);

      secure_vector<uint32_t> hc(8);
      hchacha(hc.data(), m_state.data(), m_rounds);

      m_state[ 4] = hc[0];
      m_state[ 5] = hc[1];
      m_state[ 6] = hc[2];
      m_state[ 7] = hc[3];
      m_state[ 8] = hc[4];
      m_state[ 9] = hc[5];
      m_state[10] = hc[6];
      m_state[11] = hc[7];
      m_state[12] = 0;
      m_state[13] = 0;
      m_state[14] = load_le<uint32_t>(iv, 4);
      m_state[15] = load_le<uint32_t>(iv, 5);
      }

   chacha_x8(m_buffer.data(), m_state.data(), m_rounds);
   m_position = 0;
   }

void ChaCha::clear()
   {
   zap(m_key);
   zap(m_state);
   zap(m_buffer);
   m_position = 0;
   }

std::string ChaCha::name() const
   {
   return "ChaCha(" + std::to_string(m_rounds) + ")";
   }

void ChaCha::seek(uint64_t offset)
   {
   verify_key_set(m_state.empty() == false);

   // Find the block offset
   const uint64_t counter = offset / 64;

   uint8_t out[8];

   store_le(counter, out);

   m_state[12] = load_le<uint32_t>(out, 0);
   m_state[13] += load_le<uint32_t>(out, 1);

   chacha_x8(m_buffer.data(), m_state.data(), m_rounds);
   m_position = offset % 64;
   }
}
/*
* ChaCha20Poly1305 AEAD
* (C) 2014,2016,2018 Jack Lloyd
* (C) 2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

ChaCha20Poly1305_Mode::ChaCha20Poly1305_Mode() :
   m_chacha(StreamCipher::create("ChaCha")),
   m_poly1305(MessageAuthenticationCode::create("Poly1305"))
   {
   if(!m_chacha || !m_poly1305)
      throw Algorithm_Not_Found("ChaCha20Poly1305");
   }

bool ChaCha20Poly1305_Mode::valid_nonce_length(size_t n) const
   {
   return (n == 8 || n == 12 || n == 24);
   }

void ChaCha20Poly1305_Mode::clear()
   {
   m_chacha->clear();
   m_poly1305->clear();
   reset();
   }

void ChaCha20Poly1305_Mode::reset()
   {
   m_ad.clear();
   m_ctext_len = 0;
   m_nonce_len = 0;
   }

void ChaCha20Poly1305_Mode::key_schedule(const uint8_t key[], size_t length)
   {
   m_chacha->set_key(key, length);
   }

void ChaCha20Poly1305_Mode::set_associated_data(const uint8_t ad[], size_t length)
   {
   if(m_ctext_len > 0 || m_nonce_len > 0)
      throw Invalid_State("Cannot set AD for ChaCha20Poly1305 while processing a message");
   m_ad.assign(ad, ad + length);
   }

void ChaCha20Poly1305_Mode::update_len(size_t len)
   {
   uint8_t len8[8] = { 0 };
   store_le(static_cast<uint64_t>(len), len8);
   m_poly1305->update(len8, 8);
   }

void ChaCha20Poly1305_Mode::start_msg(const uint8_t nonce[], size_t nonce_len)
   {
   if(!valid_nonce_length(nonce_len))
      throw Invalid_IV_Length(name(), nonce_len);

   m_ctext_len = 0;
   m_nonce_len = nonce_len;

   m_chacha->set_iv(nonce, nonce_len);

   uint8_t first_block[64];
   m_chacha->write_keystream(first_block, sizeof(first_block));

   m_poly1305->set_key(first_block, 32);
   // Remainder of first block is discarded
   secure_scrub_memory(first_block, sizeof(first_block));

   m_poly1305->update(m_ad);

   if(cfrg_version())
      {
      if(m_ad.size() % 16)
         {
         const uint8_t zeros[16] = { 0 };
         m_poly1305->update(zeros, 16 - m_ad.size() % 16);
         }
      }
   else
      {
      update_len(m_ad.size());
      }
   }

size_t ChaCha20Poly1305_Encryption::process(uint8_t buf[], size_t sz)
   {
   m_chacha->cipher1(buf, sz);
   m_poly1305->update(buf, sz); // poly1305 of ciphertext
   m_ctext_len += sz;
   return sz;
   }

void ChaCha20Poly1305_Encryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   update(buffer, offset);
   if(cfrg_version())
      {
      if(m_ctext_len % 16)
         {
         const uint8_t zeros[16] = { 0 };
         m_poly1305->update(zeros, 16 - m_ctext_len % 16);
         }
      update_len(m_ad.size());
      }
   update_len(m_ctext_len);

   buffer.resize(buffer.size() + tag_size());
   m_poly1305->final(&buffer[buffer.size() - tag_size()]);
   m_ctext_len = 0;
   m_nonce_len = 0;
   }

size_t ChaCha20Poly1305_Decryption::process(uint8_t buf[], size_t sz)
   {
   m_poly1305->update(buf, sz); // poly1305 of ciphertext
   m_chacha->cipher1(buf, sz);
   m_ctext_len += sz;
   return sz;
   }

void ChaCha20Poly1305_Decryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ASSERT(buffer.size() >= offset, "Offset is sane");
   const size_t sz = buffer.size() - offset;
   uint8_t* buf = buffer.data() + offset;

   BOTAN_ASSERT(sz >= tag_size(), "Have the tag as part of final input");

   const size_t remaining = sz - tag_size();

   if(remaining)
      {
      m_poly1305->update(buf, remaining); // poly1305 of ciphertext
      m_chacha->cipher1(buf, remaining);
      m_ctext_len += remaining;
      }

   if(cfrg_version())
      {
      if(m_ctext_len % 16)
         {
         const uint8_t zeros[16] = { 0 };
         m_poly1305->update(zeros, 16 - m_ctext_len % 16);
         }
      update_len(m_ad.size());
      }

   update_len(m_ctext_len);

   uint8_t mac[16];
   m_poly1305->final(mac);

   const uint8_t* included_tag = &buf[remaining];

   m_ctext_len = 0;
   m_nonce_len = 0;

   if(!constant_time_compare(mac, included_tag, tag_size()))
      throw Invalid_Authentication_Tag("ChaCha20Poly1305 tag check failed");
   buffer.resize(offset + remaining);
   }

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

//static
BOTAN_FUNC_ISA("avx2")
void ChaCha::chacha_avx2_x8(uint8_t output[64*8], uint32_t state[16], size_t rounds)
   {
   SIMD_8x32::reset_registers();

   BOTAN_ASSERT(rounds % 2 == 0, "Valid rounds");
   const SIMD_8x32 CTR0 = SIMD_8x32(0, 1, 2, 3, 4, 5, 6, 7);

   const uint32_t C = 0xFFFFFFFF - state[12];
   const SIMD_8x32 CTR1 = SIMD_8x32(0, C < 1, C < 2, C < 3, C < 4, C < 5, C < 6, C < 7);

   SIMD_8x32 R00 = SIMD_8x32::splat(state[ 0]);
   SIMD_8x32 R01 = SIMD_8x32::splat(state[ 1]);
   SIMD_8x32 R02 = SIMD_8x32::splat(state[ 2]);
   SIMD_8x32 R03 = SIMD_8x32::splat(state[ 3]);
   SIMD_8x32 R04 = SIMD_8x32::splat(state[ 4]);
   SIMD_8x32 R05 = SIMD_8x32::splat(state[ 5]);
   SIMD_8x32 R06 = SIMD_8x32::splat(state[ 6]);
   SIMD_8x32 R07 = SIMD_8x32::splat(state[ 7]);
   SIMD_8x32 R08 = SIMD_8x32::splat(state[ 8]);
   SIMD_8x32 R09 = SIMD_8x32::splat(state[ 9]);
   SIMD_8x32 R10 = SIMD_8x32::splat(state[10]);
   SIMD_8x32 R11 = SIMD_8x32::splat(state[11]);
   SIMD_8x32 R12 = SIMD_8x32::splat(state[12]) + CTR0;
   SIMD_8x32 R13 = SIMD_8x32::splat(state[13]) + CTR1;
   SIMD_8x32 R14 = SIMD_8x32::splat(state[14]);
   SIMD_8x32 R15 = SIMD_8x32::splat(state[15]);

   for(size_t r = 0; r != rounds / 2; ++r)
      {
      R00 += R04;
      R01 += R05;
      R02 += R06;
      R03 += R07;

      R12 ^= R00;
      R13 ^= R01;
      R14 ^= R02;
      R15 ^= R03;

      R12 = R12.rotl<16>();
      R13 = R13.rotl<16>();
      R14 = R14.rotl<16>();
      R15 = R15.rotl<16>();

      R08 += R12;
      R09 += R13;
      R10 += R14;
      R11 += R15;

      R04 ^= R08;
      R05 ^= R09;
      R06 ^= R10;
      R07 ^= R11;

      R04 = R04.rotl<12>();
      R05 = R05.rotl<12>();
      R06 = R06.rotl<12>();
      R07 = R07.rotl<12>();

      R00 += R04;
      R01 += R05;
      R02 += R06;
      R03 += R07;

      R12 ^= R00;
      R13 ^= R01;
      R14 ^= R02;
      R15 ^= R03;

      R12 = R12.rotl<8>();
      R13 = R13.rotl<8>();
      R14 = R14.rotl<8>();
      R15 = R15.rotl<8>();

      R08 += R12;
      R09 += R13;
      R10 += R14;
      R11 += R15;

      R04 ^= R08;
      R05 ^= R09;
      R06 ^= R10;
      R07 ^= R11;

      R04 = R04.rotl<7>();
      R05 = R05.rotl<7>();
      R06 = R06.rotl<7>();
      R07 = R07.rotl<7>();

      R00 += R05;
      R01 += R06;
      R02 += R07;
      R03 += R04;

      R15 ^= R00;
      R12 ^= R01;
      R13 ^= R02;
      R14 ^= R03;

      R15 = R15.rotl<16>();
      R12 = R12.rotl<16>();
      R13 = R13.rotl<16>();
      R14 = R14.rotl<16>();

      R10 += R15;
      R11 += R12;
      R08 += R13;
      R09 += R14;

      R05 ^= R10;
      R06 ^= R11;
      R07 ^= R08;
      R04 ^= R09;

      R05 = R05.rotl<12>();
      R06 = R06.rotl<12>();
      R07 = R07.rotl<12>();
      R04 = R04.rotl<12>();

      R00 += R05;
      R01 += R06;
      R02 += R07;
      R03 += R04;

      R15 ^= R00;
      R12 ^= R01;
      R13 ^= R02;
      R14 ^= R03;

      R15 = R15.rotl<8>();
      R12 = R12.rotl<8>();
      R13 = R13.rotl<8>();
      R14 = R14.rotl<8>();

      R10 += R15;
      R11 += R12;
      R08 += R13;
      R09 += R14;

      R05 ^= R10;
      R06 ^= R11;
      R07 ^= R08;
      R04 ^= R09;

      R05 = R05.rotl<7>();
      R06 = R06.rotl<7>();
      R07 = R07.rotl<7>();
      R04 = R04.rotl<7>();
      }

   R00 += SIMD_8x32::splat(state[0]);
   R01 += SIMD_8x32::splat(state[1]);
   R02 += SIMD_8x32::splat(state[2]);
   R03 += SIMD_8x32::splat(state[3]);
   R04 += SIMD_8x32::splat(state[4]);
   R05 += SIMD_8x32::splat(state[5]);
   R06 += SIMD_8x32::splat(state[6]);
   R07 += SIMD_8x32::splat(state[7]);
   R08 += SIMD_8x32::splat(state[8]);
   R09 += SIMD_8x32::splat(state[9]);
   R10 += SIMD_8x32::splat(state[10]);
   R11 += SIMD_8x32::splat(state[11]);
   R12 += SIMD_8x32::splat(state[12]) + CTR0;
   R13 += SIMD_8x32::splat(state[13]) + CTR1;
   R14 += SIMD_8x32::splat(state[14]);
   R15 += SIMD_8x32::splat(state[15]);

   SIMD_8x32::transpose(R00, R01, R02, R03, R04, R05, R06, R07);
   SIMD_8x32::transpose(R08, R09, R10, R11, R12, R13, R14, R15);

   R00.store_le(output);
   R08.store_le(output + 32*1);
   R01.store_le(output + 32*2);
   R09.store_le(output + 32*3);
   R02.store_le(output + 32*4);
   R10.store_le(output + 32*5);
   R03.store_le(output + 32*6);
   R11.store_le(output + 32*7);
   R04.store_le(output + 32*8);
   R12.store_le(output + 32*9);
   R05.store_le(output + 32*10);
   R13.store_le(output + 32*11);
   R06.store_le(output + 32*12);
   R14.store_le(output + 32*13);
   R07.store_le(output + 32*14);
   R15.store_le(output + 32*15);

   SIMD_8x32::zero_registers();

   state[12] += 8;
   if(state[12] < 8)
      state[13]++;
   }
}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

//static
void ChaCha::chacha_simd32_x4(uint8_t output[64*4], uint32_t state[16], size_t rounds)
   {
   BOTAN_ASSERT(rounds % 2 == 0, "Valid rounds");
   const SIMD_4x32 CTR0 = SIMD_4x32(0, 1, 2, 3);

   const uint32_t C = 0xFFFFFFFF - state[12];
   const SIMD_4x32 CTR1 = SIMD_4x32(0, C < 1, C < 2, C < 3);

   SIMD_4x32 R00 = SIMD_4x32::splat(state[ 0]);
   SIMD_4x32 R01 = SIMD_4x32::splat(state[ 1]);
   SIMD_4x32 R02 = SIMD_4x32::splat(state[ 2]);
   SIMD_4x32 R03 = SIMD_4x32::splat(state[ 3]);
   SIMD_4x32 R04 = SIMD_4x32::splat(state[ 4]);
   SIMD_4x32 R05 = SIMD_4x32::splat(state[ 5]);
   SIMD_4x32 R06 = SIMD_4x32::splat(state[ 6]);
   SIMD_4x32 R07 = SIMD_4x32::splat(state[ 7]);
   SIMD_4x32 R08 = SIMD_4x32::splat(state[ 8]);
   SIMD_4x32 R09 = SIMD_4x32::splat(state[ 9]);
   SIMD_4x32 R10 = SIMD_4x32::splat(state[10]);
   SIMD_4x32 R11 = SIMD_4x32::splat(state[11]);
   SIMD_4x32 R12 = SIMD_4x32::splat(state[12]) + CTR0;
   SIMD_4x32 R13 = SIMD_4x32::splat(state[13]) + CTR1;
   SIMD_4x32 R14 = SIMD_4x32::splat(state[14]);
   SIMD_4x32 R15 = SIMD_4x32::splat(state[15]);

   for(size_t r = 0; r != rounds / 2; ++r)
      {
      R00 += R04;
      R01 += R05;
      R02 += R06;
      R03 += R07;

      R12 ^= R00;
      R13 ^= R01;
      R14 ^= R02;
      R15 ^= R03;

      R12 = R12.rotl<16>();
      R13 = R13.rotl<16>();
      R14 = R14.rotl<16>();
      R15 = R15.rotl<16>();

      R08 += R12;
      R09 += R13;
      R10 += R14;
      R11 += R15;

      R04 ^= R08;
      R05 ^= R09;
      R06 ^= R10;
      R07 ^= R11;

      R04 = R04.rotl<12>();
      R05 = R05.rotl<12>();
      R06 = R06.rotl<12>();
      R07 = R07.rotl<12>();

      R00 += R04;
      R01 += R05;
      R02 += R06;
      R03 += R07;

      R12 ^= R00;
      R13 ^= R01;
      R14 ^= R02;
      R15 ^= R03;

      R12 = R12.rotl<8>();
      R13 = R13.rotl<8>();
      R14 = R14.rotl<8>();
      R15 = R15.rotl<8>();

      R08 += R12;
      R09 += R13;
      R10 += R14;
      R11 += R15;

      R04 ^= R08;
      R05 ^= R09;
      R06 ^= R10;
      R07 ^= R11;

      R04 = R04.rotl<7>();
      R05 = R05.rotl<7>();
      R06 = R06.rotl<7>();
      R07 = R07.rotl<7>();

      R00 += R05;
      R01 += R06;
      R02 += R07;
      R03 += R04;

      R15 ^= R00;
      R12 ^= R01;
      R13 ^= R02;
      R14 ^= R03;

      R15 = R15.rotl<16>();
      R12 = R12.rotl<16>();
      R13 = R13.rotl<16>();
      R14 = R14.rotl<16>();

      R10 += R15;
      R11 += R12;
      R08 += R13;
      R09 += R14;

      R05 ^= R10;
      R06 ^= R11;
      R07 ^= R08;
      R04 ^= R09;

      R05 = R05.rotl<12>();
      R06 = R06.rotl<12>();
      R07 = R07.rotl<12>();
      R04 = R04.rotl<12>();

      R00 += R05;
      R01 += R06;
      R02 += R07;
      R03 += R04;

      R15 ^= R00;
      R12 ^= R01;
      R13 ^= R02;
      R14 ^= R03;

      R15 = R15.rotl<8>();
      R12 = R12.rotl<8>();
      R13 = R13.rotl<8>();
      R14 = R14.rotl<8>();

      R10 += R15;
      R11 += R12;
      R08 += R13;
      R09 += R14;

      R05 ^= R10;
      R06 ^= R11;
      R07 ^= R08;
      R04 ^= R09;

      R05 = R05.rotl<7>();
      R06 = R06.rotl<7>();
      R07 = R07.rotl<7>();
      R04 = R04.rotl<7>();
      }

   R00 += SIMD_4x32::splat(state[0]);
   R01 += SIMD_4x32::splat(state[1]);
   R02 += SIMD_4x32::splat(state[2]);
   R03 += SIMD_4x32::splat(state[3]);
   R04 += SIMD_4x32::splat(state[4]);
   R05 += SIMD_4x32::splat(state[5]);
   R06 += SIMD_4x32::splat(state[6]);
   R07 += SIMD_4x32::splat(state[7]);
   R08 += SIMD_4x32::splat(state[8]);
   R09 += SIMD_4x32::splat(state[9]);
   R10 += SIMD_4x32::splat(state[10]);
   R11 += SIMD_4x32::splat(state[11]);
   R12 += SIMD_4x32::splat(state[12]) + CTR0;
   R13 += SIMD_4x32::splat(state[13]) + CTR1;
   R14 += SIMD_4x32::splat(state[14]);
   R15 += SIMD_4x32::splat(state[15]);

   SIMD_4x32::transpose(R00, R01, R02, R03);
   SIMD_4x32::transpose(R04, R05, R06, R07);
   SIMD_4x32::transpose(R08, R09, R10, R11);
   SIMD_4x32::transpose(R12, R13, R14, R15);

   R00.store_le(output + 0*16);
   R04.store_le(output + 1*16);
   R08.store_le(output + 2*16);
   R12.store_le(output + 3*16);
   R01.store_le(output + 4*16);
   R05.store_le(output + 5*16);
   R09.store_le(output + 6*16);
   R13.store_le(output + 7*16);
   R02.store_le(output + 8*16);
   R06.store_le(output + 9*16);
   R10.store_le(output + 10*16);
   R14.store_le(output + 11*16);
   R03.store_le(output + 12*16);
   R07.store_le(output + 13*16);
   R11.store_le(output + 14*16);
   R15.store_le(output + 15*16);

   state[12] += 4;
   if(state[12] < 4)
      state[13]++;
   }

}
/*
* CMAC
* (C) 1999-2007,2014 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* Perform CMAC's multiplication in GF(2^n)
*/
secure_vector<uint8_t> CMAC::poly_double(const secure_vector<uint8_t>& in)
   {
   secure_vector<uint8_t> out(in.size());
   poly_double_n(out.data(), in.data(), out.size());
   return out;
   }

/*
* Update an CMAC Calculation
*/
void CMAC::add_data(const uint8_t input[], size_t length)
   {
   const size_t bs = output_length();

   buffer_insert(m_buffer, m_position, input, length);
   if(m_position + length > bs)
      {
      xor_buf(m_state, m_buffer, bs);
      m_cipher->encrypt(m_state);
      input += (bs - m_position);
      length -= (bs - m_position);
      while(length > bs)
         {
         xor_buf(m_state, input, bs);
         m_cipher->encrypt(m_state);
         input += bs;
         length -= bs;
         }
      copy_mem(m_buffer.data(), input, length);
      m_position = 0;
      }
   m_position += length;
   }

/*
* Finalize an CMAC Calculation
*/
void CMAC::final_result(uint8_t mac[])
   {
   xor_buf(m_state, m_buffer, m_position);

   if(m_position == output_length())
      {
      xor_buf(m_state, m_B, output_length());
      }
   else
      {
      m_state[m_position] ^= 0x80;
      xor_buf(m_state, m_P, output_length());
      }

   m_cipher->encrypt(m_state);

   copy_mem(mac, m_state.data(), output_length());

   zeroise(m_state);
   zeroise(m_buffer);
   m_position = 0;
   }

/*
* CMAC Key Schedule
*/
void CMAC::key_schedule(const uint8_t key[], size_t length)
   {
   clear();
   m_cipher->set_key(key, length);
   m_cipher->encrypt(m_B);
   poly_double_n(m_B.data(), m_B.size());
   poly_double_n(m_P.data(), m_B.data(), m_P.size());
   }

/*
* Clear memory of sensitive data
*/
void CMAC::clear()
   {
   m_cipher->clear();
   zeroise(m_state);
   zeroise(m_buffer);
   zeroise(m_B);
   zeroise(m_P);
   m_position = 0;
   }

/*
* Return the name of this type
*/
std::string CMAC::name() const
   {
   return "CMAC(" + m_cipher->name() + ")";
   }

/*
* Return a clone of this object
*/
MessageAuthenticationCode* CMAC::clone() const
   {
   return new CMAC(m_cipher->clone());
   }

/*
* CMAC Constructor
*/
CMAC::CMAC(BlockCipher* cipher) :
   m_cipher(cipher),
   m_block_size(m_cipher->block_size())
   {
   if(poly_double_supported_size(m_block_size) == false)
      {
      throw Invalid_Argument("CMAC cannot use the " +
                             std::to_string(m_block_size * 8) +
                             " bit cipher " + m_cipher->name());
      }

   m_state.resize(output_length());
   m_buffer.resize(output_length());
   m_B.resize(output_length());
   m_P.resize(output_length());
   m_position = 0;
   }

}
/*
* Runtime CPU detection
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <ostream>

namespace Botan {

bool CPUID::has_simd_32()
   {
#if defined(BOTAN_TARGET_SUPPORTS_SSE2)
   return CPUID::has_sse2();
#elif defined(BOTAN_TARGET_SUPPORTS_ALTIVEC)
   return CPUID::has_altivec();
#elif defined(BOTAN_TARGET_SUPPORTS_NEON)
   return CPUID::has_neon();
#else
   return true;
#endif
   }

//static
std::string CPUID::to_string()
   {
   std::vector<std::string> flags;

#define CPUID_PRINT(flag) do { if(has_##flag()) { flags.push_back(#flag); } } while(0)

#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)
   CPUID_PRINT(sse2);
   CPUID_PRINT(ssse3);
   CPUID_PRINT(sse41);
   CPUID_PRINT(sse42);
   CPUID_PRINT(avx2);
   CPUID_PRINT(avx512f);
   CPUID_PRINT(avx512dq);
   CPUID_PRINT(avx512bw);
   CPUID_PRINT(avx512_icelake);

   CPUID_PRINT(rdtsc);
   CPUID_PRINT(bmi1);
   CPUID_PRINT(bmi2);
   CPUID_PRINT(adx);

   CPUID_PRINT(aes_ni);
   CPUID_PRINT(clmul);
   CPUID_PRINT(rdrand);
   CPUID_PRINT(rdseed);
   CPUID_PRINT(intel_sha);
   CPUID_PRINT(avx512_aes);
   CPUID_PRINT(avx512_clmul);
#endif

#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)
   CPUID_PRINT(altivec);
   CPUID_PRINT(power_crypto);
   CPUID_PRINT(darn_rng);
#endif

#if defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)
   CPUID_PRINT(neon);
   CPUID_PRINT(arm_sve);

   CPUID_PRINT(arm_sha1);
   CPUID_PRINT(arm_sha2);
   CPUID_PRINT(arm_aes);
   CPUID_PRINT(arm_pmull);
   CPUID_PRINT(arm_sha2_512);
   CPUID_PRINT(arm_sha3);
   CPUID_PRINT(arm_sm3);
   CPUID_PRINT(arm_sm4);
#endif

#undef CPUID_PRINT

   return string_join(flags, ' ');
   }

//static
void CPUID::print(std::ostream& o)
   {
   o << "CPUID flags: " << CPUID::to_string() << "\n";
   }

//static
void CPUID::initialize()
   {
   state() = CPUID_Data();
   }

CPUID::CPUID_Data::CPUID_Data()
   {
   m_cache_line_size = 0;
   m_processor_features = 0;

#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY) || \
    defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY) || \
    defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

   m_processor_features = detect_cpu_features(&m_cache_line_size);

#endif

   m_processor_features |= CPUID::CPUID_INITIALIZED_BIT;

   if(m_cache_line_size == 0)
      m_cache_line_size = BOTAN_TARGET_CPU_DEFAULT_CACHE_LINE_SIZE;

   m_endian_status = runtime_check_endian();
   }

//static
CPUID::Endian_Status CPUID::CPUID_Data::runtime_check_endian()
   {
   // Check runtime endian
   const uint32_t endian32 = 0x01234567;
   const uint8_t* e8 = reinterpret_cast<const uint8_t*>(&endian32);

   CPUID::Endian_Status endian = CPUID::Endian_Status::Unknown;

   if(e8[0] == 0x01 && e8[1] == 0x23 && e8[2] == 0x45 && e8[3] == 0x67)
      {
      endian = CPUID::Endian_Status::Big;
      }
   else if(e8[0] == 0x67 && e8[1] == 0x45 && e8[2] == 0x23 && e8[3] == 0x01)
      {
      endian = CPUID::Endian_Status::Little;
      }
   else
      {
      throw Internal_Error("Unexpected endian at runtime, neither big nor little");
      }

   // If we were compiled with a known endian, verify it matches at runtime
#if defined(BOTAN_TARGET_CPU_IS_LITTLE_ENDIAN)
   BOTAN_ASSERT(endian == CPUID::Endian_Status::Little, "Build and runtime endian match");
#elif defined(BOTAN_TARGET_CPU_IS_BIG_ENDIAN)
   BOTAN_ASSERT(endian == CPUID::Endian_Status::Big, "Build and runtime endian match");
#endif

   return endian;
   }

std::vector<Botan::CPUID::CPUID_bits>
CPUID::bit_from_string(const std::string& tok)
   {
#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)
   if(tok == "sse2" || tok == "simd")
      return {Botan::CPUID::CPUID_SSE2_BIT};
   if(tok == "ssse3")
      return {Botan::CPUID::CPUID_SSSE3_BIT};
   if(tok == "sse41")
      return {Botan::CPUID::CPUID_SSE41_BIT};
   if(tok == "sse42")
      return {Botan::CPUID::CPUID_SSE42_BIT};
   // aes_ni is the string printed on the console when running "botan cpuid"
   if(tok == "aesni" || tok == "aes_ni")
      return {Botan::CPUID::CPUID_AESNI_BIT};
   if(tok == "clmul")
      return {Botan::CPUID::CPUID_CLMUL_BIT};
   if(tok == "avx2")
      return {Botan::CPUID::CPUID_AVX2_BIT};
   if(tok == "avx512f")
      return {Botan::CPUID::CPUID_AVX512F_BIT};
   if(tok == "avx512_icelake")
      return {Botan::CPUID::CPUID_AVX512_ICL_BIT};
   // there were two if statements testing "sha" and "intel_sha" separately; combined
   if(tok == "sha" || tok=="intel_sha")
      return {Botan::CPUID::CPUID_SHA_BIT};
   if(tok == "rdtsc")
      return {Botan::CPUID::CPUID_RDTSC_BIT};
   if(tok == "bmi1")
      return {Botan::CPUID::CPUID_BMI1_BIT};
   if(tok == "bmi2")
      return {Botan::CPUID::CPUID_BMI2_BIT};
   if(tok == "adx")
      return {Botan::CPUID::CPUID_ADX_BIT};
   if(tok == "rdrand")
      return {Botan::CPUID::CPUID_RDRAND_BIT};
   if(tok == "rdseed")
      return {Botan::CPUID::CPUID_RDSEED_BIT};
   if(tok == "avx512_aes")
      return {Botan::CPUID::CPUID_AVX512_AES_BIT};
   if(tok == "avx512_clmul")
      return {Botan::CPUID::CPUID_AVX512_CLMUL_BIT};

#elif defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)
   if(tok == "altivec" || tok == "simd")
      return {Botan::CPUID::CPUID_ALTIVEC_BIT};
   if(tok == "power_crypto")
      return {Botan::CPUID::CPUID_POWER_CRYPTO_BIT};
   if(tok == "darn_rng")
      return {Botan::CPUID::CPUID_DARN_BIT};

#elif defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)
   if(tok == "neon" || tok == "simd")
      return {Botan::CPUID::CPUID_ARM_NEON_BIT};
   if(tok == "arm_sve")
      return {Botan::CPUID::CPUID_ARM_SVE_BIT};
   if(tok == "armv8sha1" || tok == "arm_sha1")
      return {Botan::CPUID::CPUID_ARM_SHA1_BIT};
   if(tok == "armv8sha2" || tok == "arm_sha2")
      return {Botan::CPUID::CPUID_ARM_SHA2_BIT};
   if(tok == "armv8aes" || tok == "arm_aes")
      return {Botan::CPUID::CPUID_ARM_AES_BIT};
   if(tok == "armv8pmull" || tok == "arm_pmull")
      return {Botan::CPUID::CPUID_ARM_PMULL_BIT};
   if(tok == "armv8sha3" || tok == "arm_sha3")
      return {Botan::CPUID::CPUID_ARM_SHA3_BIT};
   if(tok == "armv8sha2_512" || tok == "arm_sha2_512")
      return {Botan::CPUID::CPUID_ARM_SHA2_512_BIT};
   if(tok == "armv8sm3" || tok == "arm_sm3")
      return {Botan::CPUID::CPUID_ARM_SM3_BIT};
   if(tok == "armv8sm4" || tok == "arm_sm4")
      return {Botan::CPUID::CPUID_ARM_SM4_BIT};

#else
   BOTAN_UNUSED(tok);
#endif

   return {};
   }

}
/*
* Runtime CPU detection for ARM
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)

#if defined(BOTAN_TARGET_OS_IS_IOS)
  #include <sys/types.h>
  #include <sys/sysctl.h>

#else

#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
  #include <sys/auxv.h>
#endif

#endif

#endif

namespace Botan {

#if defined(BOTAN_TARGET_CPU_IS_ARM_FAMILY)

#if defined(BOTAN_TARGET_OS_IS_IOS)

namespace {

uint64_t flags_by_ios_machine_type(const std::string& machine)
   {
   /*
   * This relies on a map of known machine names to features. This
   * will quickly grow out of date as new products are introduced, but
   * is apparently the best we can do for iOS.
   */

   struct version_info {
      std::string name;
      size_t min_version_neon;
      size_t min_version_armv8;
      };

   static const version_info min_versions[] = {
      { "iPhone", 2, 6 },
      { "iPad", 1, 4 },
      { "iPod", 4, 7 },
      { "AppleTV", 2, 5 },
   };

   if(machine.size() < 3)
      return 0;

   auto comma = machine.find(',');

   // Simulator, or something we don't know about
   if(comma == std::string::npos)
      return 0;

   std::string product = machine.substr(0, comma);

   size_t version = 0;
   size_t place = 1;
   while(product.size() > 1 && ::isdigit(product.back()))
      {
      const size_t digit = product.back() - '0';
      version += digit * place;
      place *= 10;
      product.pop_back();
      }

   if(version == 0)
      return 0;

   for(const version_info& info : min_versions)
      {
      if(info.name != product)
         continue;

      if(version >= info.min_version_armv8)
         {
         return CPUID::CPUID_ARM_AES_BIT |
                CPUID::CPUID_ARM_PMULL_BIT |
                CPUID::CPUID_ARM_SHA1_BIT |
                CPUID::CPUID_ARM_SHA2_BIT |
                CPUID::CPUID_ARM_NEON_BIT;
         }

      if(version >= info.min_version_neon)
         return CPUID::CPUID_ARM_NEON_BIT;
      }

   // Some other product we don't know about
   return 0;
   }

}

#endif

uint64_t CPUID::CPUID_Data::detect_cpu_features(size_t* cache_line_size)
   {
   BOTAN_UNUSED(cache_line_size);

   uint64_t detected_features = 0;

#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
   /*
   * On systems with getauxval these bits should normally be defined
   * in bits/auxv.h but some buggy? glibc installs seem to miss them.
   * These following values are all fixed, for the Linux ELF format,
   * so we just hardcode them in ARM_hwcap_bit enum.
   */

   enum ARM_hwcap_bit {
#if defined(BOTAN_TARGET_ARCH_IS_ARM32)
      NEON_bit  = (1 << 12),
      AES_bit   = (1 << 0),
      PMULL_bit = (1 << 1),
      SHA1_bit  = (1 << 2),
      SHA2_bit  = (1 << 3),

      ARCH_hwcap_neon   = 16, // AT_HWCAP
      ARCH_hwcap_crypto = 26, // AT_HWCAP2
#elif defined(BOTAN_TARGET_ARCH_IS_ARM64)
      NEON_bit  = (1 << 1),
      AES_bit   = (1 << 3),
      PMULL_bit = (1 << 4),
      SHA1_bit  = (1 << 5),
      SHA2_bit  = (1 << 6),
      SHA3_bit  = (1 << 17),
      SM3_bit  = (1 << 18),
      SM4_bit  = (1 << 19),
      SHA2_512_bit  = (1 << 21),
      SVE_bit = (1 << 22),

      ARCH_hwcap_neon   = 16, // AT_HWCAP
      ARCH_hwcap_crypto = 16, // AT_HWCAP
#endif
   };

#if defined(AT_DCACHEBSIZE)
   // Exists only on Linux
   const unsigned long dcache_line = OS::get_auxval(AT_DCACHEBSIZE);

   // plausibility check
   if(dcache_line == 32 || dcache_line == 64 || dcache_line == 128)
      *cache_line_size = static_cast<size_t>(dcache_line);
#endif

   const unsigned long hwcap_neon = OS::get_auxval(ARM_hwcap_bit::ARCH_hwcap_neon);
   if(hwcap_neon & ARM_hwcap_bit::NEON_bit)
      detected_features |= CPUID::CPUID_ARM_NEON_BIT;

   /*
   On aarch64 this ends up calling getauxval twice with AT_HWCAP
   It doesn't seem worth optimizing this out, since getauxval is
   just reading a field in the ELF header.
   */
   const unsigned long hwcap_crypto = OS::get_auxval(ARM_hwcap_bit::ARCH_hwcap_crypto);
   if(hwcap_crypto & ARM_hwcap_bit::AES_bit)
      detected_features |= CPUID::CPUID_ARM_AES_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::PMULL_bit)
      detected_features |= CPUID::CPUID_ARM_PMULL_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SHA1_bit)
      detected_features |= CPUID::CPUID_ARM_SHA1_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SHA2_bit)
      detected_features |= CPUID::CPUID_ARM_SHA2_BIT;

#if defined(BOTAN_TARGET_ARCH_IS_ARM64)
   if(hwcap_crypto & ARM_hwcap_bit::SHA3_bit)
      detected_features |= CPUID::CPUID_ARM_SHA3_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SM3_bit)
      detected_features |= CPUID::CPUID_ARM_SM3_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SM4_bit)
      detected_features |= CPUID::CPUID_ARM_SM4_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SHA2_512_bit)
      detected_features |= CPUID::CPUID_ARM_SHA2_512_BIT;
   if(hwcap_crypto & ARM_hwcap_bit::SVE_bit)
      detected_features |= CPUID::CPUID_ARM_SVE_BIT;
#endif

#elif defined(BOTAN_TARGET_OS_IS_IOS)

   char machine[64] = { 0 };
   size_t size = sizeof(machine) - 1;
   ::sysctlbyname("hw.machine", machine, &size, nullptr, 0);

   detected_features = flags_by_ios_machine_type(machine);
   // No way to detect cache line size on iOS?

#elif defined(BOTAN_USE_GCC_INLINE_ASM) && defined(BOTAN_TARGET_ARCH_IS_ARM64)

   /*
   No getauxval API available, fall back on probe functions. We only
   bother with Aarch64 here to simplify the code and because going to
   extreme contortions to support detect NEON on devices that probably
   don't support it doesn't seem worthwhile.

   NEON registers v0-v7 are caller saved in Aarch64
   */

   auto neon_probe  = []() noexcept -> int { asm("and v0.16b, v0.16b, v0.16b"); return 1; };
   auto aes_probe   = []() noexcept -> int { asm(".word 0x4e284800"); return 1; };
   auto pmull_probe = []() noexcept -> int { asm(".word 0x0ee0e000"); return 1; };
   auto sha1_probe  = []() noexcept -> int { asm(".word 0x5e280800"); return 1; };
   auto sha2_probe  = []() noexcept -> int { asm(".word 0x5e282800"); return 1; };

   // Only bother running the crypto detection if we found NEON

   if(OS::run_cpu_instruction_probe(neon_probe) == 1)
      {
      detected_features |= CPUID::CPUID_ARM_NEON_BIT;

      if(OS::run_cpu_instruction_probe(aes_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_AES_BIT;
      if(OS::run_cpu_instruction_probe(pmull_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_PMULL_BIT;
      if(OS::run_cpu_instruction_probe(sha1_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_SHA1_BIT;
      if(OS::run_cpu_instruction_probe(sha2_probe) == 1)
         detected_features |= CPUID::CPUID_ARM_SHA2_BIT;
      }

#endif

   return detected_features;
   }

#endif

}
/*
* Runtime CPU detection for POWER/PowerPC
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)

/*
* On macOS and OpenBSD ppc, use sysctl to detect AltiVec
*/
#if defined(BOTAN_TARGET_OS_IS_MACOS)
  #include <sys/sysctl.h>
#elif defined(BOTAN_TARGET_OS_IS_OPENBSD)
  #include <sys/param.h>
  #include <sys/sysctl.h>
  #include <machine/cpu.h>
#endif

#endif

namespace Botan {

#if defined(BOTAN_TARGET_CPU_IS_PPC_FAMILY)

/*
* PowerPC specific block: check for AltiVec using either
* sysctl or by reading processor version number register.
*/
uint64_t CPUID::CPUID_Data::detect_cpu_features(size_t* cache_line_size)
   {
   BOTAN_UNUSED(cache_line_size);

#if defined(BOTAN_TARGET_OS_IS_MACOS) || defined(BOTAN_TARGET_OS_IS_OPENBSD)
   // On macOS and OpenBSD, use sysctl

   int sels[2] = {
#if defined(BOTAN_TARGET_OS_IS_OPENBSD)
      CTL_MACHDEP, CPU_ALTIVEC
#else
      CTL_HW, HW_VECTORUNIT
#endif
   };

   int vector_type = 0;
   size_t length = sizeof(vector_type);
   int error = ::sysctl(sels, 2, &vector_type, &length, NULL, 0);

   if(error == 0 && vector_type > 0)
      return CPUID::CPUID_ALTIVEC_BIT;

#elif (defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_HAS_ELF_AUX_INFO)) && defined(BOTAN_TARGET_ARCH_IS_PPC64)

   enum PPC_hwcap_bit {
      ALTIVEC_bit  = (1 << 28),
      CRYPTO_bit   = (1 << 25),
      DARN_bit     = (1 << 21),

      ARCH_hwcap_altivec = 16, // AT_HWCAP
      ARCH_hwcap_crypto  = 26, // AT_HWCAP2
   };

   uint64_t detected_features = 0;

   const unsigned long hwcap_altivec = OS::get_auxval(PPC_hwcap_bit::ARCH_hwcap_altivec);
   if(hwcap_altivec & PPC_hwcap_bit::ALTIVEC_bit)
      detected_features |= CPUID::CPUID_ALTIVEC_BIT;

   const unsigned long hwcap_crypto = OS::get_auxval(PPC_hwcap_bit::ARCH_hwcap_crypto);
   if(hwcap_crypto & PPC_hwcap_bit::CRYPTO_bit)
     detected_features |= CPUID::CPUID_POWER_CRYPTO_BIT;
   if(hwcap_crypto & PPC_hwcap_bit::DARN_bit)
     detected_features |= CPUID::CPUID_DARN_BIT;

   return detected_features;

#else

   /*
   On PowerPC, MSR 287 is PVR, the Processor Version Number
   Normally it is only accessible to ring 0, but Linux and NetBSD
   (others, too, maybe?) will trap and emulate it for us.
   */

   int pvr = OS::run_cpu_instruction_probe([]() noexcept -> int {
      uint32_t pvr = 0;
      asm volatile("mfspr %0, 287" : "=r" (pvr));
      // Top 16 bits suffice to identify the model
      return static_cast<int>(pvr >> 16);
      });

   if(pvr > 0)
      {
      const uint16_t ALTIVEC_PVR[] = {
         0x003E, // IBM POWER6
         0x003F, // IBM POWER7
         0x004A, // IBM POWER7p
         0x004B, // IBM POWER8E
         0x004C, // IBM POWER8 NVL
         0x004D, // IBM POWER8
         0x004E, // IBM POWER9
         0x000C, // G4-7400
         0x0039, // G5 970
         0x003C, // G5 970FX
         0x0044, // G5 970MP
         0x0070, // Cell PPU
         0, // end
      };

      for(size_t i = 0; ALTIVEC_PVR[i]; ++i)
         {
         if(pvr == ALTIVEC_PVR[i])
            return CPUID::CPUID_ALTIVEC_BIT;
         }

      return 0;
      }

   // TODO try direct instruction probing

#endif

   return 0;
   }

#endif

}
/*
* Runtime CPU detection for x86
* (C) 2009,2010,2013,2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

#if defined(BOTAN_BUILD_COMPILER_IS_MSVC)
  #include <intrin.h>
#elif defined(BOTAN_BUILD_COMPILER_IS_INTEL)
  #include <ia32intrin.h>
#elif defined(BOTAN_BUILD_COMPILER_IS_GCC) || defined(BOTAN_BUILD_COMPILER_IS_CLANG)
  #include <cpuid.h>
#endif

#endif

namespace Botan {

#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

uint64_t CPUID::CPUID_Data::detect_cpu_features(size_t* cache_line_size)
   {
#if defined(BOTAN_BUILD_COMPILER_IS_MSVC)
  #define X86_CPUID(type, out) do { __cpuid((int*)out, type); } while(0)
  #define X86_CPUID_SUBLEVEL(type, level, out) do { __cpuidex((int*)out, type, level); } while(0)

#elif defined(BOTAN_BUILD_COMPILER_IS_INTEL)
  #define X86_CPUID(type, out) do { __cpuid(out, type); } while(0)
  #define X86_CPUID_SUBLEVEL(type, level, out) do { __cpuidex((int*)out, type, level); } while(0)

#elif defined(BOTAN_TARGET_ARCH_IS_X86_64) && defined(BOTAN_USE_GCC_INLINE_ASM)
  #define X86_CPUID(type, out)                                                    \
     asm("cpuid\n\t" : "=a" (out[0]), "=b" (out[1]), "=c" (out[2]), "=d" (out[3]) \
         : "0" (type))

  #define X86_CPUID_SUBLEVEL(type, level, out)                                    \
     asm("cpuid\n\t" : "=a" (out[0]), "=b" (out[1]), "=c" (out[2]), "=d" (out[3]) \
         : "0" (type), "2" (level))

#elif defined(BOTAN_BUILD_COMPILER_IS_GCC) || defined(BOTAN_BUILD_COMPILER_IS_CLANG)
  #define X86_CPUID(type, out) do { __get_cpuid(type, out, out+1, out+2, out+3); } while(0)

  #define X86_CPUID_SUBLEVEL(type, level, out) \
     do { __cpuid_count(type, level, out[0], out[1], out[2], out[3]); } while(0)
#else
  #warning "No way of calling x86 cpuid instruction for this compiler"
  #define X86_CPUID(type, out) do { clear_mem(out, 4); } while(0)
  #define X86_CPUID_SUBLEVEL(type, level, out) do { clear_mem(out, 4); } while(0)
#endif

   uint64_t features_detected = 0;
   uint32_t cpuid[4] = { 0 };
   bool has_avx = 0;

   // CPUID 0: vendor identification, max sublevel
   X86_CPUID(0, cpuid);

   const uint32_t max_supported_sublevel = cpuid[0];

   const uint32_t INTEL_CPUID[3] = { 0x756E6547, 0x6C65746E, 0x49656E69 };
   const uint32_t AMD_CPUID[3] = { 0x68747541, 0x444D4163, 0x69746E65 };
   const bool is_intel = same_mem(cpuid + 1, INTEL_CPUID, 3);
   const bool is_amd = same_mem(cpuid + 1, AMD_CPUID, 3);

   if(max_supported_sublevel >= 1)
      {
      // CPUID 1: feature bits
      X86_CPUID(1, cpuid);
      const uint64_t flags0 = (static_cast<uint64_t>(cpuid[2]) << 32) | cpuid[3];

      enum x86_CPUID_1_bits : uint64_t {
         RDTSC = (1ULL << 4),
         SSE2 = (1ULL << 26),
         CLMUL = (1ULL << 33),
         SSSE3 = (1ULL << 41),
         SSE41 = (1ULL << 51),
         SSE42 = (1ULL << 52),
         AESNI = (1ULL << 57),
         OSXSAVE = (1ULL << 59),
         AVX = (1ULL << 60),
         RDRAND = (1ULL << 62)
      };

      if(flags0 & x86_CPUID_1_bits::RDTSC)
         features_detected |= CPUID::CPUID_RDTSC_BIT;
      if(flags0 & x86_CPUID_1_bits::SSE2)
         features_detected |= CPUID::CPUID_SSE2_BIT;
      if(flags0 & x86_CPUID_1_bits::CLMUL)
         features_detected |= CPUID::CPUID_CLMUL_BIT;
      if(flags0 & x86_CPUID_1_bits::SSSE3)
         features_detected |= CPUID::CPUID_SSSE3_BIT;
      if(flags0 & x86_CPUID_1_bits::SSE41)
         features_detected |= CPUID::CPUID_SSE41_BIT;
      if(flags0 & x86_CPUID_1_bits::SSE42)
         features_detected |= CPUID::CPUID_SSE42_BIT;
      if(flags0 & x86_CPUID_1_bits::AESNI)
         features_detected |= CPUID::CPUID_AESNI_BIT;
      if(flags0 & x86_CPUID_1_bits::RDRAND)
         features_detected |= CPUID::CPUID_RDRAND_BIT;
      if((flags0 & x86_CPUID_1_bits::AVX) &&
         (flags0 & x86_CPUID_1_bits::OSXSAVE))
         has_avx = 1;
      }

   if(is_intel)
      {
      // Intel cache line size is in cpuid(1) output
      *cache_line_size = 8 * get_byte(2, cpuid[1]);
      }
   else if(is_amd)
      {
      // AMD puts it in vendor zone
      X86_CPUID(0x80000005, cpuid);
      *cache_line_size = get_byte(3, cpuid[2]);
      }

   if(max_supported_sublevel >= 7)
      {
      clear_mem(cpuid, 4);
      X86_CPUID_SUBLEVEL(7, 0, cpuid);

      enum x86_CPUID_7_bits : uint64_t {
         BMI1 = (1ULL << 3),
         AVX2 = (1ULL << 5),
         BMI2 = (1ULL << 8),
         AVX512_F = (1ULL << 16),
         AVX512_DQ = (1ULL << 17),
         RDSEED = (1ULL << 18),
         ADX = (1ULL << 19),
         AVX512_IFMA = (1ULL << 21),
         SHA = (1ULL << 29),
         AVX512_BW = (1ULL << 30),
         AVX512_VL = (1ULL << 31),
         AVX512_VBMI = (1ULL << 33),
         AVX512_VBMI2 = (1ULL << 38),
         AVX512_VAES = (1ULL << 41),
         AVX512_VCLMUL = (1ULL << 42),
         AVX512_VBITALG = (1ULL << 44),
      };

      const uint64_t flags7 = (static_cast<uint64_t>(cpuid[2]) << 32) | cpuid[1];

      if((flags7 & x86_CPUID_7_bits::AVX2) && has_avx)
         features_detected |= CPUID::CPUID_AVX2_BIT;
      if(flags7 & x86_CPUID_7_bits::BMI1)
         {
         features_detected |= CPUID::CPUID_BMI1_BIT;
         /*
         We only set the BMI2 bit if BMI1 is also supported, so BMI2
         code can safely use both extensions. No known processor
         implements BMI2 but not BMI1.
         */
         if(flags7 & x86_CPUID_7_bits::BMI2)
            features_detected |= CPUID::CPUID_BMI2_BIT;
         }

      if((flags7 & x86_CPUID_7_bits::AVX512_F) && has_avx)
         {
         features_detected |= CPUID::CPUID_AVX512F_BIT;

         if(flags7 & x86_CPUID_7_bits::AVX512_DQ)
            features_detected |= CPUID::CPUID_AVX512DQ_BIT;
         if(flags7 & x86_CPUID_7_bits::AVX512_BW)
            features_detected |= CPUID::CPUID_AVX512BW_BIT;

         const uint64_t ICELAKE_FLAGS =
            x86_CPUID_7_bits::AVX512_F |
            x86_CPUID_7_bits::AVX512_DQ |
            x86_CPUID_7_bits::AVX512_IFMA |
            x86_CPUID_7_bits::AVX512_BW |
            x86_CPUID_7_bits::AVX512_VL |
            x86_CPUID_7_bits::AVX512_VBMI |
            x86_CPUID_7_bits::AVX512_VBMI2 |
            x86_CPUID_7_bits::AVX512_VBITALG;

         if((flags7 & ICELAKE_FLAGS) == ICELAKE_FLAGS)
            features_detected |= CPUID::CPUID_AVX512_ICL_BIT;

         if(flags7 & x86_CPUID_7_bits::AVX512_VAES)
            features_detected |= CPUID::CPUID_AVX512_AES_BIT;
         if(flags7 & x86_CPUID_7_bits::AVX512_VCLMUL)
            features_detected |= CPUID::CPUID_AVX512_CLMUL_BIT;
         }

      if(flags7 & x86_CPUID_7_bits::RDSEED)
         features_detected |= CPUID::CPUID_RDSEED_BIT;
      if(flags7 & x86_CPUID_7_bits::ADX)
         features_detected |= CPUID::CPUID_ADX_BIT;
      if(flags7 & x86_CPUID_7_bits::SHA)
         features_detected |= CPUID::CPUID_SHA_BIT;
      }

#undef X86_CPUID
#undef X86_CPUID_SUBLEVEL

   /*
   * If we don't have access to CPUID, we can still safely assume that
   * any x86-64 processor has SSE2 and RDTSC
   */
#if defined(BOTAN_TARGET_ARCH_IS_X86_64)
   if(features_detected == 0)
      {
      features_detected |= CPUID::CPUID_SSE2_BIT;
      features_detected |= CPUID::CPUID_RDTSC_BIT;
      }
#endif

   return features_detected;
   }

#endif

}
/*
* CRC24
* (C) 1999-2007 Jack Lloyd
* (C) 2017 [Ribose Inc](https://www.ribose.com). Performed by Krzysztof Kwiatkowski.
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

alignas(64) const uint32_t CRC24_T0[256] = {
   0x00000000, 0x00FB4C86, 0x000DD58A, 0x00F6990C, 0x00E1E693, 0x001AAA15, 0x00EC3319,
   0x00177F9F, 0x003981A1, 0x00C2CD27, 0x0034542B, 0x00CF18AD, 0x00D86732, 0x00232BB4,
   0x00D5B2B8, 0x002EFE3E, 0x00894EC5, 0x00720243, 0x00849B4F, 0x007FD7C9, 0x0068A856,
   0x0093E4D0, 0x00657DDC, 0x009E315A, 0x00B0CF64, 0x004B83E2, 0x00BD1AEE, 0x00465668,
   0x005129F7, 0x00AA6571, 0x005CFC7D, 0x00A7B0FB, 0x00E9D10C, 0x00129D8A, 0x00E40486,
   0x001F4800, 0x0008379F, 0x00F37B19, 0x0005E215, 0x00FEAE93, 0x00D050AD, 0x002B1C2B,
   0x00DD8527, 0x0026C9A1, 0x0031B63E, 0x00CAFAB8, 0x003C63B4, 0x00C72F32, 0x00609FC9,
   0x009BD34F, 0x006D4A43, 0x009606C5, 0x0081795A, 0x007A35DC, 0x008CACD0, 0x0077E056,
   0x00591E68, 0x00A252EE, 0x0054CBE2, 0x00AF8764, 0x00B8F8FB, 0x0043B47D, 0x00B52D71,
   0x004E61F7, 0x00D2A319, 0x0029EF9F, 0x00DF7693, 0x00243A15, 0x0033458A, 0x00C8090C,
   0x003E9000, 0x00C5DC86, 0x00EB22B8, 0x00106E3E, 0x00E6F732, 0x001DBBB4, 0x000AC42B,
   0x00F188AD, 0x000711A1, 0x00FC5D27, 0x005BEDDC, 0x00A0A15A, 0x00563856, 0x00AD74D0,
   0x00BA0B4F, 0x004147C9, 0x00B7DEC5, 0x004C9243, 0x00626C7D, 0x009920FB, 0x006FB9F7,
   0x0094F571, 0x00838AEE, 0x0078C668, 0x008E5F64, 0x007513E2, 0x003B7215, 0x00C03E93,
   0x0036A79F, 0x00CDEB19, 0x00DA9486, 0x0021D800, 0x00D7410C, 0x002C0D8A, 0x0002F3B4,
   0x00F9BF32, 0x000F263E, 0x00F46AB8, 0x00E31527, 0x001859A1, 0x00EEC0AD, 0x00158C2B,
   0x00B23CD0, 0x00497056, 0x00BFE95A, 0x0044A5DC, 0x0053DA43, 0x00A896C5, 0x005E0FC9,
   0x00A5434F, 0x008BBD71, 0x0070F1F7, 0x008668FB, 0x007D247D, 0x006A5BE2, 0x00911764,
   0x00678E68, 0x009CC2EE, 0x00A44733, 0x005F0BB5, 0x00A992B9, 0x0052DE3F, 0x0045A1A0,
   0x00BEED26, 0x0048742A, 0x00B338AC, 0x009DC692, 0x00668A14, 0x00901318, 0x006B5F9E,
   0x007C2001, 0x00876C87, 0x0071F58B, 0x008AB90D, 0x002D09F6, 0x00D64570, 0x0020DC7C,
   0x00DB90FA, 0x00CCEF65, 0x0037A3E3, 0x00C13AEF, 0x003A7669, 0x00148857, 0x00EFC4D1,
   0x00195DDD, 0x00E2115B, 0x00F56EC4, 0x000E2242, 0x00F8BB4E, 0x0003F7C8, 0x004D963F,
   0x00B6DAB9, 0x004043B5, 0x00BB0F33, 0x00AC70AC, 0x00573C2A, 0x00A1A526, 0x005AE9A0,
   0x0074179E, 0x008F5B18, 0x0079C214, 0x00828E92, 0x0095F10D, 0x006EBD8B, 0x00982487,
   0x00636801, 0x00C4D8FA, 0x003F947C, 0x00C90D70, 0x003241F6, 0x00253E69, 0x00DE72EF,
   0x0028EBE3, 0x00D3A765, 0x00FD595B, 0x000615DD, 0x00F08CD1, 0x000BC057, 0x001CBFC8,
   0x00E7F34E, 0x00116A42, 0x00EA26C4, 0x0076E42A, 0x008DA8AC, 0x007B31A0, 0x00807D26,
   0x009702B9, 0x006C4E3F, 0x009AD733, 0x00619BB5, 0x004F658B, 0x00B4290D, 0x0042B001,
   0x00B9FC87, 0x00AE8318, 0x0055CF9E, 0x00A35692, 0x00581A14, 0x00FFAAEF, 0x0004E669,
   0x00F27F65, 0x000933E3, 0x001E4C7C, 0x00E500FA, 0x001399F6, 0x00E8D570, 0x00C62B4E,
   0x003D67C8, 0x00CBFEC4, 0x0030B242, 0x0027CDDD, 0x00DC815B, 0x002A1857, 0x00D154D1,
   0x009F3526, 0x006479A0, 0x0092E0AC, 0x0069AC2A, 0x007ED3B5, 0x00859F33, 0x0073063F,
   0x00884AB9, 0x00A6B487, 0x005DF801, 0x00AB610D, 0x00502D8B, 0x00475214, 0x00BC1E92,
   0x004A879E, 0x00B1CB18, 0x00167BE3, 0x00ED3765, 0x001BAE69, 0x00E0E2EF, 0x00F79D70,
   0x000CD1F6, 0x00FA48FA, 0x0001047C, 0x002FFA42, 0x00D4B6C4, 0x00222FC8, 0x00D9634E,
   0x00CE1CD1, 0x00355057, 0x00C3C95B, 0x003885DD };

alignas(64) const uint32_t CRC24_T1[256] = {
   0x00000000, 0x00488F66, 0x00901ECD, 0x00D891AB, 0x00DB711C, 0x0093FE7A, 0x004B6FD1,
   0x0003E0B7, 0x00B6E338, 0x00FE6C5E, 0x0026FDF5, 0x006E7293, 0x006D9224, 0x00251D42,
   0x00FD8CE9, 0x00B5038F, 0x006CC771, 0x00244817, 0x00FCD9BC, 0x00B456DA, 0x00B7B66D,
   0x00FF390B, 0x0027A8A0, 0x006F27C6, 0x00DA2449, 0x0092AB2F, 0x004A3A84, 0x0002B5E2,
   0x00015555, 0x0049DA33, 0x00914B98, 0x00D9C4FE, 0x00D88EE3, 0x00900185, 0x0048902E,
   0x00001F48, 0x0003FFFF, 0x004B7099, 0x0093E132, 0x00DB6E54, 0x006E6DDB, 0x0026E2BD,
   0x00FE7316, 0x00B6FC70, 0x00B51CC7, 0x00FD93A1, 0x0025020A, 0x006D8D6C, 0x00B44992,
   0x00FCC6F4, 0x0024575F, 0x006CD839, 0x006F388E, 0x0027B7E8, 0x00FF2643, 0x00B7A925,
   0x0002AAAA, 0x004A25CC, 0x0092B467, 0x00DA3B01, 0x00D9DBB6, 0x009154D0, 0x0049C57B,
   0x00014A1D, 0x004B5141, 0x0003DE27, 0x00DB4F8C, 0x0093C0EA, 0x0090205D, 0x00D8AF3B,
   0x00003E90, 0x0048B1F6, 0x00FDB279, 0x00B53D1F, 0x006DACB4, 0x002523D2, 0x0026C365,
   0x006E4C03, 0x00B6DDA8, 0x00FE52CE, 0x00279630, 0x006F1956, 0x00B788FD, 0x00FF079B,
   0x00FCE72C, 0x00B4684A, 0x006CF9E1, 0x00247687, 0x00917508, 0x00D9FA6E, 0x00016BC5,
   0x0049E4A3, 0x004A0414, 0x00028B72, 0x00DA1AD9, 0x009295BF, 0x0093DFA2, 0x00DB50C4,
   0x0003C16F, 0x004B4E09, 0x0048AEBE, 0x000021D8, 0x00D8B073, 0x00903F15, 0x00253C9A,
   0x006DB3FC, 0x00B52257, 0x00FDAD31, 0x00FE4D86, 0x00B6C2E0, 0x006E534B, 0x0026DC2D,
   0x00FF18D3, 0x00B797B5, 0x006F061E, 0x00278978, 0x002469CF, 0x006CE6A9, 0x00B47702,
   0x00FCF864, 0x0049FBEB, 0x0001748D, 0x00D9E526, 0x00916A40, 0x00928AF7, 0x00DA0591,
   0x0002943A, 0x004A1B5C, 0x0096A282, 0x00DE2DE4, 0x0006BC4F, 0x004E3329, 0x004DD39E,
   0x00055CF8, 0x00DDCD53, 0x00954235, 0x002041BA, 0x0068CEDC, 0x00B05F77, 0x00F8D011,
   0x00FB30A6, 0x00B3BFC0, 0x006B2E6B, 0x0023A10D, 0x00FA65F3, 0x00B2EA95, 0x006A7B3E,
   0x0022F458, 0x002114EF, 0x00699B89, 0x00B10A22, 0x00F98544, 0x004C86CB, 0x000409AD,
   0x00DC9806, 0x00941760, 0x0097F7D7, 0x00DF78B1, 0x0007E91A, 0x004F667C, 0x004E2C61,
   0x0006A307, 0x00DE32AC, 0x0096BDCA, 0x00955D7D, 0x00DDD21B, 0x000543B0, 0x004DCCD6,
   0x00F8CF59, 0x00B0403F, 0x0068D194, 0x00205EF2, 0x0023BE45, 0x006B3123, 0x00B3A088,
   0x00FB2FEE, 0x0022EB10, 0x006A6476, 0x00B2F5DD, 0x00FA7ABB, 0x00F99A0C, 0x00B1156A,
   0x006984C1, 0x00210BA7, 0x00940828, 0x00DC874E, 0x000416E5, 0x004C9983, 0x004F7934,
   0x0007F652, 0x00DF67F9, 0x0097E89F, 0x00DDF3C3, 0x00957CA5, 0x004DED0E, 0x00056268,
   0x000682DF, 0x004E0DB9, 0x00969C12, 0x00DE1374, 0x006B10FB, 0x00239F9D, 0x00FB0E36,
   0x00B38150, 0x00B061E7, 0x00F8EE81, 0x00207F2A, 0x0068F04C, 0x00B134B2, 0x00F9BBD4,
   0x00212A7F, 0x0069A519, 0x006A45AE, 0x0022CAC8, 0x00FA5B63, 0x00B2D405, 0x0007D78A,
   0x004F58EC, 0x0097C947, 0x00DF4621, 0x00DCA696, 0x009429F0, 0x004CB85B, 0x0004373D,
   0x00057D20, 0x004DF246, 0x009563ED, 0x00DDEC8B, 0x00DE0C3C, 0x0096835A, 0x004E12F1,
   0x00069D97, 0x00B39E18, 0x00FB117E, 0x002380D5, 0x006B0FB3, 0x0068EF04, 0x00206062,
   0x00F8F1C9, 0x00B07EAF, 0x0069BA51, 0x00213537, 0x00F9A49C, 0x00B12BFA, 0x00B2CB4D,
   0x00FA442B, 0x0022D580, 0x006A5AE6, 0x00DF5969, 0x0097D60F, 0x004F47A4, 0x0007C8C2,
   0x00042875, 0x004CA713, 0x009436B8, 0x00DCB9DE };

alignas(64) const uint32_t CRC24_T2[256] = {
   0x00000000, 0x00D70983, 0x00555F80, 0x00825603, 0x0051F286, 0x0086FB05, 0x0004AD06,
   0x00D3A485, 0x0059A88B, 0x008EA108, 0x000CF70B, 0x00DBFE88, 0x00085A0D, 0x00DF538E,
   0x005D058D, 0x008A0C0E, 0x00491C91, 0x009E1512, 0x001C4311, 0x00CB4A92, 0x0018EE17,
   0x00CFE794, 0x004DB197, 0x009AB814, 0x0010B41A, 0x00C7BD99, 0x0045EB9A, 0x0092E219,
   0x0041469C, 0x00964F1F, 0x0014191C, 0x00C3109F, 0x006974A4, 0x00BE7D27, 0x003C2B24,
   0x00EB22A7, 0x00388622, 0x00EF8FA1, 0x006DD9A2, 0x00BAD021, 0x0030DC2F, 0x00E7D5AC,
   0x006583AF, 0x00B28A2C, 0x00612EA9, 0x00B6272A, 0x00347129, 0x00E378AA, 0x00206835,
   0x00F761B6, 0x007537B5, 0x00A23E36, 0x00719AB3, 0x00A69330, 0x0024C533, 0x00F3CCB0,
   0x0079C0BE, 0x00AEC93D, 0x002C9F3E, 0x00FB96BD, 0x00283238, 0x00FF3BBB, 0x007D6DB8,
   0x00AA643B, 0x0029A4CE, 0x00FEAD4D, 0x007CFB4E, 0x00ABF2CD, 0x00785648, 0x00AF5FCB,
   0x002D09C8, 0x00FA004B, 0x00700C45, 0x00A705C6, 0x002553C5, 0x00F25A46, 0x0021FEC3,
   0x00F6F740, 0x0074A143, 0x00A3A8C0, 0x0060B85F, 0x00B7B1DC, 0x0035E7DF, 0x00E2EE5C,
   0x00314AD9, 0x00E6435A, 0x00641559, 0x00B31CDA, 0x003910D4, 0x00EE1957, 0x006C4F54,
   0x00BB46D7, 0x0068E252, 0x00BFEBD1, 0x003DBDD2, 0x00EAB451, 0x0040D06A, 0x0097D9E9,
   0x00158FEA, 0x00C28669, 0x001122EC, 0x00C62B6F, 0x00447D6C, 0x009374EF, 0x001978E1,
   0x00CE7162, 0x004C2761, 0x009B2EE2, 0x00488A67, 0x009F83E4, 0x001DD5E7, 0x00CADC64,
   0x0009CCFB, 0x00DEC578, 0x005C937B, 0x008B9AF8, 0x00583E7D, 0x008F37FE, 0x000D61FD,
   0x00DA687E, 0x00506470, 0x00876DF3, 0x00053BF0, 0x00D23273, 0x000196F6, 0x00D69F75,
   0x0054C976, 0x0083C0F5, 0x00A9041B, 0x007E0D98, 0x00FC5B9B, 0x002B5218, 0x00F8F69D,
   0x002FFF1E, 0x00ADA91D, 0x007AA09E, 0x00F0AC90, 0x0027A513, 0x00A5F310, 0x0072FA93,
   0x00A15E16, 0x00765795, 0x00F40196, 0x00230815, 0x00E0188A, 0x00371109, 0x00B5470A,
   0x00624E89, 0x00B1EA0C, 0x0066E38F, 0x00E4B58C, 0x0033BC0F, 0x00B9B001, 0x006EB982,
   0x00ECEF81, 0x003BE602, 0x00E84287, 0x003F4B04, 0x00BD1D07, 0x006A1484, 0x00C070BF,
   0x0017793C, 0x00952F3F, 0x004226BC, 0x00918239, 0x00468BBA, 0x00C4DDB9, 0x0013D43A,
   0x0099D834, 0x004ED1B7, 0x00CC87B4, 0x001B8E37, 0x00C82AB2, 0x001F2331, 0x009D7532,
   0x004A7CB1, 0x00896C2E, 0x005E65AD, 0x00DC33AE, 0x000B3A2D, 0x00D89EA8, 0x000F972B,
   0x008DC128, 0x005AC8AB, 0x00D0C4A5, 0x0007CD26, 0x00859B25, 0x005292A6, 0x00813623,
   0x00563FA0, 0x00D469A3, 0x00036020, 0x0080A0D5, 0x0057A956, 0x00D5FF55, 0x0002F6D6,
   0x00D15253, 0x00065BD0, 0x00840DD3, 0x00530450, 0x00D9085E, 0x000E01DD, 0x008C57DE,
   0x005B5E5D, 0x0088FAD8, 0x005FF35B, 0x00DDA558, 0x000AACDB, 0x00C9BC44, 0x001EB5C7,
   0x009CE3C4, 0x004BEA47, 0x00984EC2, 0x004F4741, 0x00CD1142, 0x001A18C1, 0x009014CF,
   0x00471D4C, 0x00C54B4F, 0x001242CC, 0x00C1E649, 0x0016EFCA, 0x0094B9C9, 0x0043B04A,
   0x00E9D471, 0x003EDDF2, 0x00BC8BF1, 0x006B8272, 0x00B826F7, 0x006F2F74, 0x00ED7977,
   0x003A70F4, 0x00B07CFA, 0x00677579, 0x00E5237A, 0x00322AF9, 0x00E18E7C, 0x003687FF,
   0x00B4D1FC, 0x0063D87F, 0x00A0C8E0, 0x0077C163, 0x00F59760, 0x00229EE3, 0x00F13A66,
   0x002633E5, 0x00A465E6, 0x00736C65, 0x00F9606B, 0x002E69E8, 0x00AC3FEB, 0x007B3668,
   0x00A892ED, 0x007F9B6E, 0x00FDCD6D, 0x002AC4EE };

alignas(64) const uint32_t CRC24_T3[256] = {
   0x00000000, 0x00520936, 0x00A4126C, 0x00F61B5A, 0x004825D8, 0x001A2CEE, 0x00EC37B4,
   0x00BE3E82, 0x006B0636, 0x00390F00, 0x00CF145A, 0x009D1D6C, 0x002323EE, 0x00712AD8,
   0x00873182, 0x00D538B4, 0x00D60C6C, 0x0084055A, 0x00721E00, 0x00201736, 0x009E29B4,
   0x00CC2082, 0x003A3BD8, 0x006832EE, 0x00BD0A5A, 0x00EF036C, 0x00191836, 0x004B1100,
   0x00F52F82, 0x00A726B4, 0x00513DEE, 0x000334D8, 0x00AC19D8, 0x00FE10EE, 0x00080BB4,
   0x005A0282, 0x00E43C00, 0x00B63536, 0x00402E6C, 0x0012275A, 0x00C71FEE, 0x009516D8,
   0x00630D82, 0x003104B4, 0x008F3A36, 0x00DD3300, 0x002B285A, 0x0079216C, 0x007A15B4,
   0x00281C82, 0x00DE07D8, 0x008C0EEE, 0x0032306C, 0x0060395A, 0x00962200, 0x00C42B36,
   0x00111382, 0x00431AB4, 0x00B501EE, 0x00E708D8, 0x0059365A, 0x000B3F6C, 0x00FD2436,
   0x00AF2D00, 0x00A37F36, 0x00F17600, 0x00076D5A, 0x0055646C, 0x00EB5AEE, 0x00B953D8,
   0x004F4882, 0x001D41B4, 0x00C87900, 0x009A7036, 0x006C6B6C, 0x003E625A, 0x00805CD8,
   0x00D255EE, 0x00244EB4, 0x00764782, 0x0075735A, 0x00277A6C, 0x00D16136, 0x00836800,
   0x003D5682, 0x006F5FB4, 0x009944EE, 0x00CB4DD8, 0x001E756C, 0x004C7C5A, 0x00BA6700,
   0x00E86E36, 0x005650B4, 0x00045982, 0x00F242D8, 0x00A04BEE, 0x000F66EE, 0x005D6FD8,
   0x00AB7482, 0x00F97DB4, 0x00474336, 0x00154A00, 0x00E3515A, 0x00B1586C, 0x006460D8,
   0x003669EE, 0x00C072B4, 0x00927B82, 0x002C4500, 0x007E4C36, 0x0088576C, 0x00DA5E5A,
   0x00D96A82, 0x008B63B4, 0x007D78EE, 0x002F71D8, 0x00914F5A, 0x00C3466C, 0x00355D36,
   0x00675400, 0x00B26CB4, 0x00E06582, 0x00167ED8, 0x004477EE, 0x00FA496C, 0x00A8405A,
   0x005E5B00, 0x000C5236, 0x0046FF6C, 0x0014F65A, 0x00E2ED00, 0x00B0E436, 0x000EDAB4,
   0x005CD382, 0x00AAC8D8, 0x00F8C1EE, 0x002DF95A, 0x007FF06C, 0x0089EB36, 0x00DBE200,
   0x0065DC82, 0x0037D5B4, 0x00C1CEEE, 0x0093C7D8, 0x0090F300, 0x00C2FA36, 0x0034E16C,
   0x0066E85A, 0x00D8D6D8, 0x008ADFEE, 0x007CC4B4, 0x002ECD82, 0x00FBF536, 0x00A9FC00,
   0x005FE75A, 0x000DEE6C, 0x00B3D0EE, 0x00E1D9D8, 0x0017C282, 0x0045CBB4, 0x00EAE6B4,
   0x00B8EF82, 0x004EF4D8, 0x001CFDEE, 0x00A2C36C, 0x00F0CA5A, 0x0006D100, 0x0054D836,
   0x0081E082, 0x00D3E9B4, 0x0025F2EE, 0x0077FBD8, 0x00C9C55A, 0x009BCC6C, 0x006DD736,
   0x003FDE00, 0x003CEAD8, 0x006EE3EE, 0x0098F8B4, 0x00CAF182, 0x0074CF00, 0x0026C636,
   0x00D0DD6C, 0x0082D45A, 0x0057ECEE, 0x0005E5D8, 0x00F3FE82, 0x00A1F7B4, 0x001FC936,
   0x004DC000, 0x00BBDB5A, 0x00E9D26C, 0x00E5805A, 0x00B7896C, 0x00419236, 0x00139B00,
   0x00ADA582, 0x00FFACB4, 0x0009B7EE, 0x005BBED8, 0x008E866C, 0x00DC8F5A, 0x002A9400,
   0x00789D36, 0x00C6A3B4, 0x0094AA82, 0x0062B1D8, 0x0030B8EE, 0x00338C36, 0x00618500,
   0x00979E5A, 0x00C5976C, 0x007BA9EE, 0x0029A0D8, 0x00DFBB82, 0x008DB2B4, 0x00588A00,
   0x000A8336, 0x00FC986C, 0x00AE915A, 0x0010AFD8, 0x0042A6EE, 0x00B4BDB4, 0x00E6B482,
   0x00499982, 0x001B90B4, 0x00ED8BEE, 0x00BF82D8, 0x0001BC5A, 0x0053B56C, 0x00A5AE36,
   0x00F7A700, 0x00229FB4, 0x00709682, 0x00868DD8, 0x00D484EE, 0x006ABA6C, 0x0038B35A,
   0x00CEA800, 0x009CA136, 0x009F95EE, 0x00CD9CD8, 0x003B8782, 0x00698EB4, 0x00D7B036,
   0x0085B900, 0x0073A25A, 0x0021AB6C, 0x00F493D8, 0x00A69AEE, 0x005081B4, 0x00028882,
   0x00BCB600, 0x00EEBF36, 0x0018A46C, 0x004AAD5A };

inline uint32_t process8(uint32_t crc, uint8_t data)
   {
   return (crc >> 8) ^ CRC24_T0[(crc & 0xff) ^ data];
   }

inline uint32_t process32(uint32_t crc, uint32_t word)
   {
   crc ^= word;
   crc = CRC24_T3[(crc >>  0) & 0xff]
       ^ CRC24_T2[(crc >>  8) & 0xff]
       ^ CRC24_T1[(crc >> 16) & 0xff]
       ^ CRC24_T0[(crc >> 24) & 0xff];
   return crc;
   }
}

std::unique_ptr<HashFunction> CRC24::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new CRC24(*this));
   }

/*
* Update a CRC24 Checksum
*
* Implementation uses Slicing-by-N algorithm described in
* "Novel Table Lookup-Based Algorithms for High-Performance
* CRC Generation", by M.Kounavis.
*
* This algorithm uses 4 precomputed look-up tables. First
* table T0 is computed same way as in a method proposed
* by D. Sarwate (1988). Then T_1, T2 and T3 are computed
* in following way:
*
*    T1[j] = (T0[j] >> 8) ^ T0[ T0[j] & 0xFF ]
*    T2[j] = (T1[j] >> 8) ^ T0[ T1[j] & 0xFF ]
*    T3[j] = (T2[j] >> 8) ^ T0[ T2[j] & 0xFF ]
*
*/
void CRC24::add_data(const uint8_t input[], size_t length)
   {
   uint32_t d[4];
   uint32_t tmp = m_crc;

   // Input is word aligned if WA & input == 0
   static const uint8_t WA = (BOTAN_MP_WORD_BITS/8) - 1;

   // Ensure input is word aligned before processing in parallel
   for(;length && (reinterpret_cast<uintptr_t>(input) & WA); length--)
      tmp = process8(tmp, *input++);

   while(length >= 16)
      {
      load_le(d, input, 4);
      tmp = process32(tmp, d[0]);
      tmp = process32(tmp, d[1]);
      tmp = process32(tmp, d[2]);
      tmp = process32(tmp, d[3]);

      input += 16;
      length -= 16;
      }

   while(length--)
      tmp = process8(tmp, *input++);

   m_crc = tmp & 0xffffff;
   }

/*
* Finalize a CRC24 Checksum
*/
void CRC24::final_result(uint8_t output[])
   {
   output[0] = get_byte(3, m_crc);
   output[1] = get_byte(2, m_crc);
   output[2] = get_byte(1, m_crc);
   clear();
   }

}
/*
* CRC32
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> CRC32::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new CRC32(*this));
   }

namespace {

const uint32_t CRC32_T0[256] = {
   0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
   0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
   0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
   0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
   0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
   0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
   0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
   0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
   0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
   0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
   0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
   0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
   0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
   0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
   0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
   0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
   0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
   0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
   0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
   0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
   0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
   0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
   0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
   0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
   0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
   0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
   0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
   0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
   0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
   0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
   0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
   0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
   0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
   0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
   0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
   0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
   0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
   0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
   0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
   0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
   0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
   0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
   0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D };

}

/*
* Update a CRC32 Checksum
*/
void CRC32::add_data(const uint8_t input[], size_t length)
   {
   uint32_t tmp = m_crc;
   while(length >= 16)
      {
      tmp = CRC32_T0[(tmp ^ input[ 0]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 1]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 2]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 3]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 4]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 5]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 6]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 7]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 8]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[ 9]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[10]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[11]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[12]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[13]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[14]) & 0xFF] ^ (tmp >> 8);
      tmp = CRC32_T0[(tmp ^ input[15]) & 0xFF] ^ (tmp >> 8);
      input += 16;
      length -= 16;
      }

   for(size_t i = 0; i != length; ++i)
      tmp = CRC32_T0[(tmp ^ input[i]) & 0xFF] ^ (tmp >> 8);

   m_crc = tmp;
   }

/*
* Finalize a CRC32 Checksum
*/
void CRC32::final_result(uint8_t output[])
   {
   m_crc ^= 0xFFFFFFFF;
   store_be(m_crc, output);
   clear();
   }

}
/*
* Counter mode
* (C) 1999-2011,2014 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

CTR_BE::CTR_BE(BlockCipher* ciph) :
   m_cipher(ciph),
   m_block_size(m_cipher->block_size()),
   m_ctr_size(m_block_size),
   m_ctr_blocks(m_cipher->parallel_bytes() / m_block_size),
   m_counter(m_cipher->parallel_bytes()),
   m_pad(m_counter.size()),
   m_pad_pos(0)
   {
   }

CTR_BE::CTR_BE(BlockCipher* cipher, size_t ctr_size) :
   m_cipher(cipher),
   m_block_size(m_cipher->block_size()),
   m_ctr_size(ctr_size),
   m_ctr_blocks(m_cipher->parallel_bytes() / m_block_size),
   m_counter(m_cipher->parallel_bytes()),
   m_pad(m_counter.size()),
   m_pad_pos(0)
   {
   BOTAN_ARG_CHECK(m_ctr_size >= 4 && m_ctr_size <= m_block_size,
                   "Invalid CTR-BE counter size");
   }

void CTR_BE::clear()
   {
   m_cipher->clear();
   zeroise(m_pad);
   zeroise(m_counter);
   zap(m_iv);
   m_pad_pos = 0;
   }

size_t CTR_BE::default_iv_length() const
   {
   return m_block_size;
   }

bool CTR_BE::valid_iv_length(size_t iv_len) const
   {
   return (iv_len <= m_block_size);
   }

Key_Length_Specification CTR_BE::key_spec() const
   {
   return m_cipher->key_spec();
   }

CTR_BE* CTR_BE::clone() const
   {
   return new CTR_BE(m_cipher->clone(), m_ctr_size);
   }

void CTR_BE::key_schedule(const uint8_t key[], size_t key_len)
   {
   m_cipher->set_key(key, key_len);

   // Set a default all-zeros IV
   set_iv(nullptr, 0);
   }

std::string CTR_BE::name() const
   {
   if(m_ctr_size == m_block_size)
      return ("CTR-BE(" + m_cipher->name() + ")");
   else
      return ("CTR-BE(" + m_cipher->name() + "," + std::to_string(m_ctr_size) + ")");

   }

void CTR_BE::cipher(const uint8_t in[], uint8_t out[], size_t length)
   {
   verify_key_set(m_iv.empty() == false);

   const uint8_t* pad_bits = &m_pad[0];
   const size_t pad_size = m_pad.size();

   if(m_pad_pos > 0)
      {
      const size_t avail = pad_size - m_pad_pos;
      const size_t take = std::min(length, avail);
      xor_buf(out, in, pad_bits + m_pad_pos, take);
      length -= take;
      in += take;
      out += take;
      m_pad_pos += take;

      if(take == avail)
         {
         add_counter(m_ctr_blocks);
         m_cipher->encrypt_n(m_counter.data(), m_pad.data(), m_ctr_blocks);
         m_pad_pos = 0;
         }
      }

   while(length >= pad_size)
      {
      xor_buf(out, in, pad_bits, pad_size);
      length -= pad_size;
      in += pad_size;
      out += pad_size;

      add_counter(m_ctr_blocks);
      m_cipher->encrypt_n(m_counter.data(), m_pad.data(), m_ctr_blocks);
      }

   xor_buf(out, in, pad_bits, length);
   m_pad_pos += length;
   }

void CTR_BE::set_iv(const uint8_t iv[], size_t iv_len)
   {
   if(!valid_iv_length(iv_len))
      throw Invalid_IV_Length(name(), iv_len);

   m_iv.resize(m_block_size);
   zeroise(m_iv);
   buffer_insert(m_iv, 0, iv, iv_len);

   seek(0);
   }

void CTR_BE::add_counter(const uint64_t counter)
   {
   const size_t ctr_size = m_ctr_size;
   const size_t ctr_blocks = m_ctr_blocks;
   const size_t BS = m_block_size;

   if(ctr_size == 4)
      {
      const size_t off = (BS - 4);
      const uint32_t low32 = static_cast<uint32_t>(counter + load_be<uint32_t>(&m_counter[off], 0));

      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         store_be(uint32_t(low32 + i), &m_counter[i*BS+off]);
         }
      }
   else if(ctr_size == 8)
      {
      const size_t off = (BS - 8);
      const uint64_t low64 = counter + load_be<uint64_t>(&m_counter[off], 0);

      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         store_be(uint64_t(low64 + i), &m_counter[i*BS+off]);
         }
      }
   else if(ctr_size == 16)
      {
      const size_t off = (BS - 16);
      uint64_t b0 = load_be<uint64_t>(&m_counter[off], 0);
      uint64_t b1 = load_be<uint64_t>(&m_counter[off], 1);
      b1 += counter;
      b0 += (b1 < counter) ? 1 : 0; // carry

      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         store_be(b0, &m_counter[i*BS+off]);
         store_be(b1, &m_counter[i*BS+off+8]);
         b1 += 1;
         b0 += (b1 == 0); // carry
         }
      }
   else
      {
      for(size_t i = 0; i != ctr_blocks; ++i)
         {
         uint64_t local_counter = counter;
         uint16_t carry = static_cast<uint8_t>(local_counter);
         for(size_t j = 0; (carry || local_counter) && j != ctr_size; ++j)
            {
            const size_t off = i*BS + (BS-1-j);
            const uint16_t cnt = static_cast<uint16_t>(m_counter[off]) + carry;
            m_counter[off] = static_cast<uint8_t>(cnt);
            local_counter = (local_counter >> 8);
            carry = (cnt >> 8) + static_cast<uint8_t>(local_counter);
            }
         }
      }
   }

void CTR_BE::seek(uint64_t offset)
   {
   verify_key_set(m_iv.empty() == false);

   const uint64_t base_counter = m_ctr_blocks * (offset / m_counter.size());

   zeroise(m_counter);
   buffer_insert(m_counter, 0, m_iv);

   const size_t BS = m_block_size;

   // Set m_counter blocks to IV, IV + 1, ... IV + n

   if(m_ctr_size == 4 && BS >= 8)
      {
      const uint32_t low32 = load_be<uint32_t>(&m_counter[BS-4], 0);

      if(m_ctr_blocks >= 4 && is_power_of_2(m_ctr_blocks))
         {
         size_t written = 1;
         while(written < m_ctr_blocks)
            {
            copy_mem(&m_counter[written*BS], &m_counter[0], BS*written);
            written *= 2;
            }
         }
      else
         {
         for(size_t i = 1; i != m_ctr_blocks; ++i)
            {
            copy_mem(&m_counter[i*BS], &m_counter[0], BS - 4);
            }
         }

      for(size_t i = 1; i != m_ctr_blocks; ++i)
         {
         const uint32_t c = static_cast<uint32_t>(low32 + i);
         store_be(c, &m_counter[(BS-4)+i*BS]);
         }
      }
   else
      {
      // do everything sequentially:
      for(size_t i = 1; i != m_ctr_blocks; ++i)
         {
         buffer_insert(m_counter, i*BS, &m_counter[(i-1)*BS], BS);

         for(size_t j = 0; j != m_ctr_size; ++j)
            if(++m_counter[i*BS + (BS - 1 - j)])
               break;
         }
      }

   if(base_counter > 0)
      add_counter(base_counter);

   m_cipher->encrypt_n(m_counter.data(), m_pad.data(), m_ctr_blocks);
   m_pad_pos = offset % m_counter.size();
   }
}
/*
* EAX Mode Encryption
* (C) 1999-2007 Jack Lloyd
* (C) 2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

/*
* EAX MAC-based PRF
*/
secure_vector<uint8_t> eax_prf(uint8_t tag, size_t block_size,
                           MessageAuthenticationCode& mac,
                           const uint8_t in[], size_t length)
   {
   for(size_t i = 0; i != block_size - 1; ++i)
      {
      mac.update(0);
      }
   mac.update(tag);
   mac.update(in, length);
   return mac.final();
   }

}

/*
* EAX_Mode Constructor
*/
EAX_Mode::EAX_Mode(BlockCipher* cipher, size_t tag_size) :
   m_tag_size(tag_size),
   m_cipher(cipher),
   m_ctr(new CTR_BE(m_cipher->clone())),
   m_cmac(new CMAC(m_cipher->clone()))
   {
   if(m_tag_size < 8 || m_tag_size > m_cmac->output_length())
      throw Invalid_Argument(name() + ": Bad tag size " + std::to_string(tag_size));
   }

void EAX_Mode::clear()
   {
   m_cipher->clear();
   m_ctr->clear();
   m_cmac->clear();
   reset();
   }

void EAX_Mode::reset()
   {
   m_ad_mac.clear();
   m_nonce_mac.clear();

   // Clear out any data added to the CMAC calculation
   try {
      m_cmac->final();
   }
   catch(Key_Not_Set&) {}
   }

std::string EAX_Mode::name() const
   {
   return (m_cipher->name() + "/EAX");
   }

size_t EAX_Mode::update_granularity() const
   {
   /*
   * For EAX this actually can be as low as 1 but that causes problems
   * for applications which use update_granularity as the buffer size.
   */
   return m_cipher->parallel_bytes();
   }

Key_Length_Specification EAX_Mode::key_spec() const
   {
   return m_cipher->key_spec();
   }

/*
* Set the EAX key
*/
void EAX_Mode::key_schedule(const uint8_t key[], size_t length)
   {
   /*
   * These could share the key schedule, which is one nice part of EAX,
   * but it's much easier to ignore that here...
   */
   m_ctr->set_key(key, length);
   m_cmac->set_key(key, length);
   }

/*
* Set the EAX associated data
*/
void EAX_Mode::set_associated_data(const uint8_t ad[], size_t length)
   {
   if(m_nonce_mac.empty() == false)
      throw Invalid_State("Cannot set AD for EAX while processing a message");
   m_ad_mac = eax_prf(1, block_size(), *m_cmac, ad, length);
   }

void EAX_Mode::start_msg(const uint8_t nonce[], size_t nonce_len)
   {
   if(!valid_nonce_length(nonce_len))
      throw Invalid_IV_Length(name(), nonce_len);

   m_nonce_mac = eax_prf(0, block_size(), *m_cmac, nonce, nonce_len);

   m_ctr->set_iv(m_nonce_mac.data(), m_nonce_mac.size());

   for(size_t i = 0; i != block_size() - 1; ++i)
      m_cmac->update(0);
   m_cmac->update(2);
   }

size_t EAX_Encryption::process(uint8_t buf[], size_t sz)
   {
   BOTAN_STATE_CHECK(m_nonce_mac.size() > 0);
   m_ctr->cipher(buf, buf, sz);
   m_cmac->update(buf, sz);
   return sz;
   }

void EAX_Encryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ASSERT_NOMSG(m_nonce_mac.empty() == false);
   update(buffer, offset);

   secure_vector<uint8_t> data_mac = m_cmac->final();
   xor_buf(data_mac, m_nonce_mac, data_mac.size());

   if(m_ad_mac.empty())
      {
      m_ad_mac = eax_prf(1, block_size(), *m_cmac, nullptr, 0);
      }

   xor_buf(data_mac, m_ad_mac, data_mac.size());

   buffer += std::make_pair(data_mac.data(), tag_size());
   }

size_t EAX_Decryption::process(uint8_t buf[], size_t sz)
   {
   BOTAN_STATE_CHECK(m_nonce_mac.size() > 0);
   m_cmac->update(buf, sz);
   m_ctr->cipher(buf, buf, sz);
   return sz;
   }

void EAX_Decryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ASSERT(buffer.size() >= offset, "Offset is sane");
   const size_t sz = buffer.size() - offset;
   uint8_t* buf = buffer.data() + offset;

   BOTAN_ASSERT(sz >= tag_size(), "Have the tag as part of final input");

   const size_t remaining = sz - tag_size();

   if(remaining)
      {
      m_cmac->update(buf, remaining);
      m_ctr->cipher(buf, buf, remaining);
      }

   const uint8_t* included_tag = &buf[remaining];

   secure_vector<uint8_t> mac = m_cmac->final();
   mac ^= m_nonce_mac;

   if(m_ad_mac.empty())
      {
      m_ad_mac = eax_prf(1, block_size(), *m_cmac, nullptr, 0);
      }

   mac ^= m_ad_mac;

   if(!constant_time_compare(mac.data(), included_tag, tag_size()))
      throw Invalid_Authentication_Tag("EAX tag check failed");

   buffer.resize(offset + remaining);

   m_nonce_mac.clear();
   }

}
/*
* Entropy Source Polling
* (C) 2008-2010,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_SYSTEM_RNG)
#endif

#if defined(BOTAN_HAS_PROCESSOR_RNG)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_RDRAND)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_RDSEED)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_DARN)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_DEV_RANDOM)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_WIN32)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_PROC_WALKER)
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_GETENTROPY)
#endif

namespace Botan {

namespace {

#if defined(BOTAN_HAS_SYSTEM_RNG)

class System_RNG_EntropySource final : public Entropy_Source
   {
   public:
      size_t poll(RandomNumberGenerator& rng) override
         {
         const size_t poll_bits = BOTAN_RNG_RESEED_POLL_BITS;
         rng.reseed_from_rng(system_rng(), poll_bits);
         return poll_bits;
         }

      std::string name() const override { return "system_rng"; }
   };

#endif

#if defined(BOTAN_HAS_PROCESSOR_RNG)

class Processor_RNG_EntropySource final : public Entropy_Source
   {
   public:
      size_t poll(RandomNumberGenerator& rng) override
         {
         /*
         * Intel's documentation for RDRAND at
         * https://software.intel.com/en-us/articles/intel-digital-random-number-generator-drng-software-implementation-guide
         * claims that software can guarantee a reseed event by polling enough data:
         * "There is an upper bound of 511 samples per seed in the implementation
         * where samples are 128 bits in size and can provide two 64-bit random
         * numbers each."
         *
         * By requesting 65536 bits we are asking for 512 samples and thus are assured
         * that at some point in producing the output, at least one reseed of the
         * internal state will occur.
         *
         * The reseeding conditions of the POWER and ARM processor RNGs are not known
         * but probably work in a somewhat similar manner. The exact amount requested
         * may be tweaked if and when such conditions become publically known.
         */
         const size_t poll_bits = 65536;
         rng.reseed_from_rng(m_hwrng, poll_bits);
         // Avoid trusting a black box, don't count this as contributing entropy:
         return 0;
         }

      std::string name() const override { return m_hwrng.name(); }
   private:
      Processor_RNG m_hwrng;
   };

#endif

}

std::unique_ptr<Entropy_Source> Entropy_Source::create(const std::string& name)
   {
#if defined(BOTAN_HAS_SYSTEM_RNG)
   if(name == "system_rng" || name == "win32_cryptoapi")
      {
      return std::unique_ptr<Entropy_Source>(new System_RNG_EntropySource);
      }
#endif

#if defined(BOTAN_HAS_PROCESSOR_RNG)
   if(name == "hwrng" || name == "rdrand" || name == "p9_darn")
      {
      if(Processor_RNG::available())
         {
         return std::unique_ptr<Entropy_Source>(new Processor_RNG_EntropySource);
         }
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_RDSEED)
   if(name == "rdseed")
      {
      return std::unique_ptr<Entropy_Source>(new Intel_Rdseed);
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_GETENTROPY)
   if(name == "getentropy")
      {
      return std::unique_ptr<Entropy_Source>(new Getentropy);
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_DEV_RANDOM)
   if(name == "dev_random")
      {
      return std::unique_ptr<Entropy_Source>(new Device_EntropySource(BOTAN_SYSTEM_RNG_POLL_DEVICES));
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_PROC_WALKER)
   if(name == "proc_walk" && OS::running_in_privileged_state() == false)
      {
      const std::string root_dir = BOTAN_ENTROPY_PROC_FS_PATH;
      if(!root_dir.empty())
         return std::unique_ptr<Entropy_Source>(new ProcWalking_EntropySource(root_dir));
      }
#endif

#if defined(BOTAN_HAS_ENTROPY_SRC_WIN32)
   if(name == "system_stats")
      {
      return std::unique_ptr<Entropy_Source>(new Win32_EntropySource);
      }
#endif

   BOTAN_UNUSED(name);
   return std::unique_ptr<Entropy_Source>();
   }

void Entropy_Sources::add_source(std::unique_ptr<Entropy_Source> src)
   {
   if(src.get())
      {
      m_srcs.push_back(std::move(src));
      }
   }

std::vector<std::string> Entropy_Sources::enabled_sources() const
   {
   std::vector<std::string> sources;
   for(size_t i = 0; i != m_srcs.size(); ++i)
      {
      sources.push_back(m_srcs[i]->name());
      }
   return sources;
   }

size_t Entropy_Sources::poll(RandomNumberGenerator& rng,
                             size_t poll_bits,
                             std::chrono::milliseconds timeout)
   {
   typedef std::chrono::system_clock clock;

   auto deadline = clock::now() + timeout;

   size_t bits_collected = 0;

   for(size_t i = 0; i != m_srcs.size(); ++i)
      {
      bits_collected += m_srcs[i]->poll(rng);

      if (bits_collected >= poll_bits || clock::now() > deadline)
         break;
      }

   return bits_collected;
   }

size_t Entropy_Sources::poll_just(RandomNumberGenerator& rng, const std::string& the_src)
   {
   for(size_t i = 0; i != m_srcs.size(); ++i)
      {
      if(m_srcs[i]->name() == the_src)
         {
         return m_srcs[i]->poll(rng);
         }
      }

   return 0;
   }

Entropy_Sources::Entropy_Sources(const std::vector<std::string>& sources)
   {
   for(auto&& src_name : sources)
      {
      add_source(Entropy_Source::create(src_name));
      }
   }

Entropy_Sources& Entropy_Sources::global_sources()
   {
   static Entropy_Sources global_entropy_sources(BOTAN_ENTROPY_DEFAULT_SOURCES);

   return global_entropy_sources;
   }

}

/*
* GCM Mode Encryption
* (C) 2013,2015 Jack Lloyd
* (C) 2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* GCM_Mode Constructor
*/
GCM_Mode::GCM_Mode(BlockCipher* cipher, size_t tag_size) :
   m_tag_size(tag_size),
   m_cipher_name(cipher->name()),
   m_ctr(new CTR_BE(cipher, 4)),
   m_ghash(new GHASH)
   {
   if(cipher->block_size() != GCM_BS)
      throw Invalid_Argument("Invalid block cipher for GCM");

   /* We allow any of the values 128, 120, 112, 104, or 96 bits as a tag size */
   /* 64 bit tag is still supported but deprecated and will be removed in the future */
   if(m_tag_size != 8 && (m_tag_size < 12 || m_tag_size > 16))
      throw Invalid_Argument(name() + ": Bad tag size " + std::to_string(m_tag_size));
   }

GCM_Mode::~GCM_Mode() { /* for unique_ptr */ }

void GCM_Mode::clear()
   {
   m_ctr->clear();
   m_ghash->clear();
   reset();
   }

void GCM_Mode::reset()
   {
   m_ghash->reset();
   }

std::string GCM_Mode::name() const
   {
   return (m_cipher_name + "/GCM(" + std::to_string(tag_size()) + ")");
   }

std::string GCM_Mode::provider() const
   {
   return m_ghash->provider();
   }

size_t GCM_Mode::update_granularity() const
   {
   return GCM_BS * std::max<size_t>(2, BOTAN_BLOCK_CIPHER_PAR_MULT);
   }

bool GCM_Mode::valid_nonce_length(size_t len) const
   {
   // GCM does not support empty nonces
   return (len > 0);
   }

Key_Length_Specification GCM_Mode::key_spec() const
   {
   return m_ctr->key_spec();
   }

void GCM_Mode::key_schedule(const uint8_t key[], size_t keylen)
   {
   m_ctr->set_key(key, keylen);

   const std::vector<uint8_t> zeros(GCM_BS);
   m_ctr->set_iv(zeros.data(), zeros.size());

   secure_vector<uint8_t> H(GCM_BS);
   m_ctr->encipher(H);
   m_ghash->set_key(H);
   }

void GCM_Mode::set_associated_data(const uint8_t ad[], size_t ad_len)
   {
   m_ghash->set_associated_data(ad, ad_len);
   }

void GCM_Mode::start_msg(const uint8_t nonce[], size_t nonce_len)
   {
   if(!valid_nonce_length(nonce_len))
      throw Invalid_IV_Length(name(), nonce_len);

   if(m_y0.size() != GCM_BS)
      m_y0.resize(GCM_BS);

   clear_mem(m_y0.data(), m_y0.size());

   if(nonce_len == 12)
      {
      copy_mem(m_y0.data(), nonce, nonce_len);
      m_y0[15] = 1;
      }
   else
      {
      m_ghash->nonce_hash(m_y0, nonce, nonce_len);
      }

   m_ctr->set_iv(m_y0.data(), m_y0.size());

   clear_mem(m_y0.data(), m_y0.size());
   m_ctr->encipher(m_y0);

   m_ghash->start(m_y0.data(), m_y0.size());
   clear_mem(m_y0.data(), m_y0.size());
   }

size_t GCM_Encryption::process(uint8_t buf[], size_t sz)
   {
   BOTAN_ARG_CHECK(sz % update_granularity() == 0, "Invalid buffer size");
   m_ctr->cipher(buf, buf, sz);
   m_ghash->update(buf, sz);
   return sz;
   }

void GCM_Encryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ARG_CHECK(offset <= buffer.size(), "Invalid offset");
   const size_t sz = buffer.size() - offset;
   uint8_t* buf = buffer.data() + offset;

   m_ctr->cipher(buf, buf, sz);
   m_ghash->update(buf, sz);

   uint8_t mac[16] = { 0 };
   m_ghash->final(mac, tag_size());
   buffer += std::make_pair(mac, tag_size());
   }

size_t GCM_Decryption::process(uint8_t buf[], size_t sz)
   {
   BOTAN_ARG_CHECK(sz % update_granularity() == 0, "Invalid buffer size");
   m_ghash->update(buf, sz);
   m_ctr->cipher(buf, buf, sz);
   return sz;
   }

void GCM_Decryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ARG_CHECK(offset <= buffer.size(), "Invalid offset");
   const size_t sz = buffer.size() - offset;
   uint8_t* buf = buffer.data() + offset;

   if(sz < tag_size())
      throw Decoding_Error("Insufficient input for GCM decryption, tag missing");

   const size_t remaining = sz - tag_size();

   // handle any final input before the tag
   if(remaining)
      {
      m_ghash->update(buf, remaining);
      m_ctr->cipher(buf, buf, remaining);
      }

   uint8_t mac[16] = { 0 };
   m_ghash->final(mac, tag_size());

   const uint8_t* included_tag = &buffer[remaining+offset];

   if(!constant_time_compare(mac, included_tag, tag_size()))
      throw Invalid_Authentication_Tag("GCM tag check failed");

   buffer.resize(offset + remaining);
   }

}
/*
* GCM GHASH
* (C) 2013,2015,2017 Jack Lloyd
* (C) 2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::string GHASH::provider() const
   {
#if defined(BOTAN_HAS_GHASH_CLMUL_CPU)
   if(CPUID::has_carryless_multiply())
      return "clmul";
#endif

#if defined(BOTAN_HAS_GHASH_CLMUL_VPERM)
   if(CPUID::has_vperm())
      return "vperm";
#endif

   return "base";
   }

void GHASH::ghash_multiply(secure_vector<uint8_t>& x,
                         const uint8_t input[],
                         size_t blocks)
   {
#if defined(BOTAN_HAS_GHASH_CLMUL_CPU)
   if(CPUID::has_carryless_multiply())
      {
      return ghash_multiply_cpu(x.data(), m_H_pow.data(), input, blocks);
      }
#endif

#if defined(BOTAN_HAS_GHASH_CLMUL_VPERM)
   if(CPUID::has_vperm())
      {
      return ghash_multiply_vperm(x.data(), m_HM.data(), input, blocks);
      }
#endif

   CT::poison(x.data(), x.size());

   const uint64_t ALL_BITS = 0xFFFFFFFFFFFFFFFF;

   uint64_t X[2] = {
      load_be<uint64_t>(x.data(), 0),
      load_be<uint64_t>(x.data(), 1)
   };

   for(size_t b = 0; b != blocks; ++b)
      {
      X[0] ^= load_be<uint64_t>(input, 2*b);
      X[1] ^= load_be<uint64_t>(input, 2*b+1);

      uint64_t Z[2] = { 0, 0 };

      for(size_t i = 0; i != 64; ++i)
         {
         const uint64_t X0MASK = (ALL_BITS + (X[0] >> 63)) ^ ALL_BITS;
         const uint64_t X1MASK = (ALL_BITS + (X[1] >> 63)) ^ ALL_BITS;

         X[0] <<= 1;
         X[1] <<= 1;

         Z[0] ^= m_HM[4*i  ] & X0MASK;
         Z[1] ^= m_HM[4*i+1] & X0MASK;
         Z[0] ^= m_HM[4*i+2] & X1MASK;
         Z[1] ^= m_HM[4*i+3] & X1MASK;
         }

      X[0] = Z[0];
      X[1] = Z[1];
      }

   store_be<uint64_t>(x.data(), X[0], X[1]);
   CT::unpoison(x.data(), x.size());
   }

void GHASH::ghash_update(secure_vector<uint8_t>& ghash,
                         const uint8_t input[], size_t length)
   {
   verify_key_set(!m_HM.empty());

   /*
   This assumes if less than block size input then we're just on the
   final block and should pad with zeros
   */

   const size_t full_blocks = length / GCM_BS;
   const size_t final_bytes = length - (full_blocks * GCM_BS);

   if(full_blocks > 0)
      {
      ghash_multiply(ghash, input, full_blocks);
      }

   if(final_bytes)
      {
      uint8_t last_block[GCM_BS] = { 0 };
      copy_mem(last_block, input + full_blocks * GCM_BS, final_bytes);
      ghash_multiply(ghash, last_block, 1);
      secure_scrub_memory(last_block, final_bytes);
      }
   }

void GHASH::key_schedule(const uint8_t key[], size_t length)
   {
   m_H.assign(key, key+length);
   m_H_ad.resize(GCM_BS);
   m_ad_len = 0;
   m_text_len = 0;

   uint64_t H0 = load_be<uint64_t>(m_H.data(), 0);
   uint64_t H1 = load_be<uint64_t>(m_H.data(), 1);

   const uint64_t R = 0xE100000000000000;

   m_HM.resize(256);

   // precompute the multiples of H
   for(size_t i = 0; i != 2; ++i)
      {
      for(size_t j = 0; j != 64; ++j)
         {
         /*
         we interleave H^1, H^65, H^2, H^66, H3, H67, H4, H68
         to make indexing nicer in the multiplication code
         */
         m_HM[4*j+2*i] = H0;
         m_HM[4*j+2*i+1] = H1;

         // GCM's bit ops are reversed so we carry out of the bottom
         const uint64_t carry = R * (H1 & 1);
         H1 = (H1 >> 1) | (H0 << 63);
         H0 = (H0 >> 1) ^ carry;
         }
      }

#if defined(BOTAN_HAS_GHASH_CLMUL_CPU)
   if(CPUID::has_carryless_multiply())
      {
      m_H_pow.resize(8);
      ghash_precompute_cpu(m_H.data(), m_H_pow.data());
      }
#endif
   }

void GHASH::start(const uint8_t nonce[], size_t len)
   {
   BOTAN_ARG_CHECK(len == 16, "GHASH requires a 128-bit nonce");
   m_nonce.assign(nonce, nonce + len);
   m_ghash = m_H_ad;
   }

void GHASH::set_associated_data(const uint8_t input[], size_t length)
   {
   if(m_ghash.empty() == false)
      throw Invalid_State("Too late to set AD in GHASH");

   zeroise(m_H_ad);

   ghash_update(m_H_ad, input, length);
   m_ad_len = length;
   }

void GHASH::update_associated_data(const uint8_t ad[], size_t length)
   {
   verify_key_set(m_ghash.size() == GCM_BS);
   m_ad_len += length;
   ghash_update(m_ghash, ad, length);
   }

void GHASH::update(const uint8_t input[], size_t length)
   {
   verify_key_set(m_ghash.size() == GCM_BS);
   m_text_len += length;
   ghash_update(m_ghash, input, length);
   }

void GHASH::add_final_block(secure_vector<uint8_t>& hash,
                            size_t ad_len, size_t text_len)
   {
   /*
   * stack buffer is fine here since the text len is public
   * and the length of the AD is probably not sensitive either.
   */
   uint8_t final_block[GCM_BS];
   store_be<uint64_t>(final_block, 8*ad_len, 8*text_len);
   ghash_update(hash, final_block, GCM_BS);
   }

void GHASH::final(uint8_t mac[], size_t mac_len)
   {
   BOTAN_ARG_CHECK(mac_len > 0 && mac_len <= 16, "GHASH output length");
   add_final_block(m_ghash, m_ad_len, m_text_len);

   for(size_t i = 0; i != mac_len; ++i)
      mac[i] = m_ghash[i] ^ m_nonce[i];

   m_ghash.clear();
   m_text_len = 0;
   }

void GHASH::nonce_hash(secure_vector<uint8_t>& y0, const uint8_t nonce[], size_t nonce_len)
   {
   BOTAN_ASSERT(m_ghash.size() == 0, "nonce_hash called during wrong time");

   ghash_update(y0, nonce, nonce_len);
   add_final_block(y0, 0, nonce_len);
   }

void GHASH::clear()
   {
   zap(m_H);
   zap(m_HM);
   reset();
   }

void GHASH::reset()
   {
   zeroise(m_H_ad);
   m_ghash.clear();
   m_nonce.clear();
   m_text_len = m_ad_len = 0;
   }

}
/*
* GOST 28147-89
* (C) 1999-2009,2011 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

uint8_t GOST_28147_89_Params::sbox_entry(size_t row, size_t col) const
   {
   const uint8_t x = m_sboxes[4 * col + (row / 2)];
   return (row % 2 == 0) ? (x >> 4) : (x & 0x0F);
   }

uint8_t GOST_28147_89_Params::sbox_pair(size_t row, size_t col) const
   {
   const uint8_t x = m_sboxes[4 * (col % 16) + row];
   const uint8_t y = m_sboxes[4 * (col / 16) + row];
   return (x >> 4) | (y << 4);
   }

GOST_28147_89_Params::GOST_28147_89_Params(const std::string& n) : m_name(n)
   {
   // Encoded in the packed fromat from RFC 4357

   // GostR3411_94_TestParamSet (OID 1.2.643.2.2.31.0)
   static const uint8_t GOST_R_3411_TEST_PARAMS[64] = {
      0x4E, 0x57, 0x64, 0xD1, 0xAB, 0x8D, 0xCB, 0xBF, 0x94, 0x1A, 0x7A,
      0x4D, 0x2C, 0xD1, 0x10, 0x10, 0xD6, 0xA0, 0x57, 0x35, 0x8D, 0x38,
      0xF2, 0xF7, 0x0F, 0x49, 0xD1, 0x5A, 0xEA, 0x2F, 0x8D, 0x94, 0x62,
      0xEE, 0x43, 0x09, 0xB3, 0xF4, 0xA6, 0xA2, 0x18, 0xC6, 0x98, 0xE3,
      0xC1, 0x7C, 0xE5, 0x7E, 0x70, 0x6B, 0x09, 0x66, 0xF7, 0x02, 0x3C,
      0x8B, 0x55, 0x95, 0xBF, 0x28, 0x39, 0xB3, 0x2E, 0xCC };

   // GostR3411-94-CryptoProParamSet (OID 1.2.643.2.2.31.1)
   static const uint8_t GOST_R_3411_CRYPTOPRO_PARAMS[64] = {
      0xA5, 0x74, 0x77, 0xD1, 0x4F, 0xFA, 0x66, 0xE3, 0x54, 0xC7, 0x42,
      0x4A, 0x60, 0xEC, 0xB4, 0x19, 0x82, 0x90, 0x9D, 0x75, 0x1D, 0x4F,
      0xC9, 0x0B, 0x3B, 0x12, 0x2F, 0x54, 0x79, 0x08, 0xA0, 0xAF, 0xD1,
      0x3E, 0x1A, 0x38, 0xC7, 0xB1, 0x81, 0xC6, 0xE6, 0x56, 0x05, 0x87,
      0x03, 0x25, 0xEB, 0xFE, 0x9C, 0x6D, 0xF8, 0x6D, 0x2E, 0xAB, 0xDE,
      0x20, 0xBA, 0x89, 0x3C, 0x92, 0xF8, 0xD3, 0x53, 0xBC };

   if(m_name == "R3411_94_TestParam")
      m_sboxes = GOST_R_3411_TEST_PARAMS;
   else if(m_name == "R3411_CryptoPro")
      m_sboxes = GOST_R_3411_CRYPTOPRO_PARAMS;
   else
      throw Invalid_Argument("GOST_28147_89_Params: Unknown " + m_name);
   }

/*
* GOST Constructor
*/
GOST_28147_89::GOST_28147_89(const GOST_28147_89_Params& param) : m_SBOX(1024)
   {
   // Convert the parallel 4x4 sboxes into larger word-based sboxes

   for(size_t i = 0; i != 256; ++i)
      {
      m_SBOX[i    ] = rotl<11, uint32_t>(param.sbox_pair(0, i));
      m_SBOX[i+256] = rotl<19, uint32_t>(param.sbox_pair(1, i));
      m_SBOX[i+512] = rotl<27, uint32_t>(param.sbox_pair(2, i));
      m_SBOX[i+768] = rotl< 3, uint32_t>(param.sbox_pair(3, i));
      }
   }

std::string GOST_28147_89::name() const
   {
   /*
   'Guess' the right name for the sbox on the basis of the values.
   This would need to be updated if support for other sbox parameters
   is added. Preferably, we would just store the string value in the
   constructor, but can't break binary compat.
   */
   std::string sbox_name = "";
   if(m_SBOX[0] == 0x00072000)
      sbox_name = "R3411_94_TestParam";
   else if(m_SBOX[0] == 0x0002D000)
      sbox_name = "R3411_CryptoPro";
   else
      throw Internal_Error("GOST-28147 unrecognized sbox value");

   return "GOST-28147-89(" + sbox_name + ")";
   }

/*
* Two rounds of GOST
*/
#define GOST_2ROUND(N1, N2, R1, R2)   \
   do {                               \
   uint32_t T0 = N1 + m_EK[R1];           \
   N2 ^= m_SBOX[get_byte(3, T0)] |      \
         m_SBOX[get_byte(2, T0)+256] |  \
         m_SBOX[get_byte(1, T0)+512] |  \
         m_SBOX[get_byte(0, T0)+768];   \
                                      \
   uint32_t T1 = N2 + m_EK[R2];           \
   N1 ^= m_SBOX[get_byte(3, T1)] |      \
         m_SBOX[get_byte(2, T1)+256] |  \
         m_SBOX[get_byte(1, T1)+512] |  \
         m_SBOX[get_byte(0, T1)+768];   \
   } while(0)

/*
* GOST Encryption
*/
void GOST_28147_89::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

   for(size_t i = 0; i != blocks; ++i)
      {
      uint32_t N1 = load_le<uint32_t>(in, 0);
      uint32_t N2 = load_le<uint32_t>(in, 1);

      for(size_t j = 0; j != 3; ++j)
         {
         GOST_2ROUND(N1, N2, 0, 1);
         GOST_2ROUND(N1, N2, 2, 3);
         GOST_2ROUND(N1, N2, 4, 5);
         GOST_2ROUND(N1, N2, 6, 7);
         }

      GOST_2ROUND(N1, N2, 7, 6);
      GOST_2ROUND(N1, N2, 5, 4);
      GOST_2ROUND(N1, N2, 3, 2);
      GOST_2ROUND(N1, N2, 1, 0);

      store_le(out, N2, N1);

      in += BLOCK_SIZE;
      out += BLOCK_SIZE;
      }
   }

/*
* GOST Decryption
*/
void GOST_28147_89::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_EK.empty() == false);

   for(size_t i = 0; i != blocks; ++i)
      {
      uint32_t N1 = load_le<uint32_t>(in, 0);
      uint32_t N2 = load_le<uint32_t>(in, 1);

      GOST_2ROUND(N1, N2, 0, 1);
      GOST_2ROUND(N1, N2, 2, 3);
      GOST_2ROUND(N1, N2, 4, 5);
      GOST_2ROUND(N1, N2, 6, 7);

      for(size_t j = 0; j != 3; ++j)
         {
         GOST_2ROUND(N1, N2, 7, 6);
         GOST_2ROUND(N1, N2, 5, 4);
         GOST_2ROUND(N1, N2, 3, 2);
         GOST_2ROUND(N1, N2, 1, 0);
         }

      store_le(out, N2, N1);
      in += BLOCK_SIZE;
      out += BLOCK_SIZE;
      }
   }

/*
* GOST Key Schedule
*/
void GOST_28147_89::key_schedule(const uint8_t key[], size_t)
   {
   m_EK.resize(8);
   for(size_t i = 0; i != 8; ++i)
      m_EK[i] = load_le<uint32_t>(key, i);
   }

void GOST_28147_89::clear()
   {
   zap(m_EK);
   }

}
/*
* GOST 34.11
* (C) 2009 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/**
* GOST 34.11 Constructor
*/
GOST_34_11::GOST_34_11() :
   m_cipher(GOST_28147_89_Params("R3411_CryptoPro")),
   m_buffer(32),
   m_sum(32),
   m_hash(32)
   {
   m_count = 0;
   m_position = 0;
   }

void GOST_34_11::clear()
   {
   m_cipher.clear();
   zeroise(m_sum);
   zeroise(m_hash);
   m_count = 0;
   m_position = 0;
   }

std::unique_ptr<HashFunction> GOST_34_11::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new GOST_34_11(*this));
   }

/**
* Hash additional inputs
*/
void GOST_34_11::add_data(const uint8_t input[], size_t length)
   {
   m_count += length;

   if(m_position)
      {
      buffer_insert(m_buffer, m_position, input, length);

      if(m_position + length >= hash_block_size())
         {
         compress_n(m_buffer.data(), 1);
         input += (hash_block_size() - m_position);
         length -= (hash_block_size() - m_position);
         m_position = 0;
         }
      }

   const size_t full_blocks = length / hash_block_size();
   const size_t remaining   = length % hash_block_size();

   if(full_blocks)
      compress_n(input, full_blocks);

   buffer_insert(m_buffer, m_position, input + full_blocks * hash_block_size(), remaining);
   m_position += remaining;
   }

/**
* The GOST 34.11 compression function
*/
void GOST_34_11::compress_n(const uint8_t input[], size_t blocks)
   {
   for(size_t i = 0; i != blocks; ++i)
      {
      for(uint16_t j = 0, carry = 0; j != 32; ++j)
         {
         uint16_t s = m_sum[j] + input[32*i+j] + carry;
         carry = get_byte(0, s);
         m_sum[j] = get_byte(1, s);
         }

      uint8_t S[32] = { 0 };

      uint64_t U[4], V[4];
      load_be(U, m_hash.data(), 4);
      load_be(V, input + 32*i, 4);

      for(size_t j = 0; j != 4; ++j)
         {
         uint8_t key[32] = { 0 };

         // P transformation
         for(size_t k = 0; k != 4; ++k)
            {
            const uint64_t UVk = U[k] ^ V[k];
            for(size_t l = 0; l != 8; ++l)
               key[4*l+k] = get_byte(l, UVk);
            }

         m_cipher.set_key(key, 32);
         m_cipher.encrypt(&m_hash[8*j], S + 8*j);

         if(j == 3)
            break;

         // A(x)
         uint64_t A_U = U[0];
         U[0] = U[1];
         U[1] = U[2];
         U[2] = U[3];
         U[3] = U[0] ^ A_U;

         if(j == 1) // C_3
            {
            U[0] ^= 0x00FF00FF00FF00FF;
            U[1] ^= 0xFF00FF00FF00FF00;
            U[2] ^= 0x00FFFF00FF0000FF;
            U[3] ^= 0xFF000000FFFF00FF;
            }

         // A(A(x))
         uint64_t AA_V_1 = V[0] ^ V[1];
         uint64_t AA_V_2 = V[1] ^ V[2];
         V[0] = V[2];
         V[1] = V[3];
         V[2] = AA_V_1;
         V[3] = AA_V_2;
         }

      uint8_t S2[32] = { 0 };

      // 12 rounds of psi
      S2[ 0] = S[24];
      S2[ 1] = S[25];
      S2[ 2] = S[26];
      S2[ 3] = S[27];
      S2[ 4] = S[28];
      S2[ 5] = S[29];
      S2[ 6] = S[30];
      S2[ 7] = S[31];
      S2[ 8] = S[ 0] ^ S[ 2] ^ S[ 4] ^ S[ 6] ^ S[24] ^ S[30];
      S2[ 9] = S[ 1] ^ S[ 3] ^ S[ 5] ^ S[ 7] ^ S[25] ^ S[31];
      S2[10] = S[ 0] ^ S[ 8] ^ S[24] ^ S[26] ^ S[30];
      S2[11] = S[ 1] ^ S[ 9] ^ S[25] ^ S[27] ^ S[31];
      S2[12] = S[ 0] ^ S[ 4] ^ S[ 6] ^ S[10] ^ S[24] ^ S[26] ^ S[28] ^ S[30];
      S2[13] = S[ 1] ^ S[ 5] ^ S[ 7] ^ S[11] ^ S[25] ^ S[27] ^ S[29] ^ S[31];
      S2[14] = S[ 0] ^ S[ 4] ^ S[ 8] ^ S[12] ^ S[24] ^ S[26] ^ S[28];
      S2[15] = S[ 1] ^ S[ 5] ^ S[ 9] ^ S[13] ^ S[25] ^ S[27] ^ S[29];
      S2[16] = S[ 2] ^ S[ 6] ^ S[10] ^ S[14] ^ S[26] ^ S[28] ^ S[30];
      S2[17] = S[ 3] ^ S[ 7] ^ S[11] ^ S[15] ^ S[27] ^ S[29] ^ S[31];
      S2[18] = S[ 0] ^ S[ 2] ^ S[ 6] ^ S[ 8] ^ S[12] ^ S[16] ^ S[24] ^ S[28];
      S2[19] = S[ 1] ^ S[ 3] ^ S[ 7] ^ S[ 9] ^ S[13] ^ S[17] ^ S[25] ^ S[29];
      S2[20] = S[ 2] ^ S[ 4] ^ S[ 8] ^ S[10] ^ S[14] ^ S[18] ^ S[26] ^ S[30];
      S2[21] = S[ 3] ^ S[ 5] ^ S[ 9] ^ S[11] ^ S[15] ^ S[19] ^ S[27] ^ S[31];
      S2[22] = S[ 0] ^ S[ 2] ^ S[10] ^ S[12] ^ S[16] ^ S[20] ^ S[24] ^ S[28] ^ S[30];
      S2[23] = S[ 1] ^ S[ 3] ^ S[11] ^ S[13] ^ S[17] ^ S[21] ^ S[25] ^ S[29] ^ S[31];
      S2[24] = S[ 0] ^ S[ 6] ^ S[12] ^ S[14] ^ S[18] ^ S[22] ^ S[24] ^ S[26];
      S2[25] = S[ 1] ^ S[ 7] ^ S[13] ^ S[15] ^ S[19] ^ S[23] ^ S[25] ^ S[27];
      S2[26] = S[ 2] ^ S[ 8] ^ S[14] ^ S[16] ^ S[20] ^ S[24] ^ S[26] ^ S[28];
      S2[27] = S[ 3] ^ S[ 9] ^ S[15] ^ S[17] ^ S[21] ^ S[25] ^ S[27] ^ S[29];
      S2[28] = S[ 4] ^ S[10] ^ S[16] ^ S[18] ^ S[22] ^ S[26] ^ S[28] ^ S[30];
      S2[29] = S[ 5] ^ S[11] ^ S[17] ^ S[19] ^ S[23] ^ S[27] ^ S[29] ^ S[31];
      S2[30] = S[ 0] ^ S[ 2] ^ S[ 4] ^ S[12] ^ S[18] ^ S[20] ^ S[28];
      S2[31] = S[ 1] ^ S[ 3] ^ S[ 5] ^ S[13] ^ S[19] ^ S[21] ^ S[29];

      xor_buf(S, S2, input + 32*i, 32);

      S2[0] = S[0] ^ S[2] ^ S[4] ^ S[6] ^ S[24] ^ S[30];
      S2[1] = S[1] ^ S[3] ^ S[5] ^ S[7] ^ S[25] ^ S[31];

      copy_mem(S, S+2, 30);
      S[30] = S2[0];
      S[31] = S2[1];

      xor_buf(S, m_hash.data(), 32);

      // 61 rounds of psi
      S2[ 0] = S[ 2] ^ S[ 6] ^ S[14] ^ S[20] ^ S[22] ^ S[26] ^ S[28] ^ S[30];
      S2[ 1] = S[ 3] ^ S[ 7] ^ S[15] ^ S[21] ^ S[23] ^ S[27] ^ S[29] ^ S[31];
      S2[ 2] = S[ 0] ^ S[ 2] ^ S[ 6] ^ S[ 8] ^ S[16] ^ S[22] ^ S[28];
      S2[ 3] = S[ 1] ^ S[ 3] ^ S[ 7] ^ S[ 9] ^ S[17] ^ S[23] ^ S[29];
      S2[ 4] = S[ 2] ^ S[ 4] ^ S[ 8] ^ S[10] ^ S[18] ^ S[24] ^ S[30];
      S2[ 5] = S[ 3] ^ S[ 5] ^ S[ 9] ^ S[11] ^ S[19] ^ S[25] ^ S[31];
      S2[ 6] = S[ 0] ^ S[ 2] ^ S[10] ^ S[12] ^ S[20] ^ S[24] ^ S[26] ^ S[30];
      S2[ 7] = S[ 1] ^ S[ 3] ^ S[11] ^ S[13] ^ S[21] ^ S[25] ^ S[27] ^ S[31];
      S2[ 8] = S[ 0] ^ S[ 6] ^ S[12] ^ S[14] ^ S[22] ^ S[24] ^ S[26] ^ S[28] ^ S[30];
      S2[ 9] = S[ 1] ^ S[ 7] ^ S[13] ^ S[15] ^ S[23] ^ S[25] ^ S[27] ^ S[29] ^ S[31];
      S2[10] = S[ 0] ^ S[ 4] ^ S[ 6] ^ S[ 8] ^ S[14] ^ S[16] ^ S[26] ^ S[28];
      S2[11] = S[ 1] ^ S[ 5] ^ S[ 7] ^ S[ 9] ^ S[15] ^ S[17] ^ S[27] ^ S[29];
      S2[12] = S[ 2] ^ S[ 6] ^ S[ 8] ^ S[10] ^ S[16] ^ S[18] ^ S[28] ^ S[30];
      S2[13] = S[ 3] ^ S[ 7] ^ S[ 9] ^ S[11] ^ S[17] ^ S[19] ^ S[29] ^ S[31];
      S2[14] = S[ 0] ^ S[ 2] ^ S[ 6] ^ S[ 8] ^ S[10] ^ S[12] ^ S[18] ^ S[20] ^ S[24];
      S2[15] = S[ 1] ^ S[ 3] ^ S[ 7] ^ S[ 9] ^ S[11] ^ S[13] ^ S[19] ^ S[21] ^ S[25];
      S2[16] = S[ 2] ^ S[ 4] ^ S[ 8] ^ S[10] ^ S[12] ^ S[14] ^ S[20] ^ S[22] ^ S[26];
      S2[17] = S[ 3] ^ S[ 5] ^ S[ 9] ^ S[11] ^ S[13] ^ S[15] ^ S[21] ^ S[23] ^ S[27];
      S2[18] = S[ 4] ^ S[ 6] ^ S[10] ^ S[12] ^ S[14] ^ S[16] ^ S[22] ^ S[24] ^ S[28];
      S2[19] = S[ 5] ^ S[ 7] ^ S[11] ^ S[13] ^ S[15] ^ S[17] ^ S[23] ^ S[25] ^ S[29];
      S2[20] = S[ 6] ^ S[ 8] ^ S[12] ^ S[14] ^ S[16] ^ S[18] ^ S[24] ^ S[26] ^ S[30];
      S2[21] = S[ 7] ^ S[ 9] ^ S[13] ^ S[15] ^ S[17] ^ S[19] ^ S[25] ^ S[27] ^ S[31];
      S2[22] = S[ 0] ^ S[ 2] ^ S[ 4] ^ S[ 6] ^ S[ 8] ^ S[10] ^ S[14] ^ S[16] ^
               S[18] ^ S[20] ^ S[24] ^ S[26] ^ S[28] ^ S[30];
      S2[23] = S[ 1] ^ S[ 3] ^ S[ 5] ^ S[ 7] ^ S[ 9] ^ S[11] ^ S[15] ^ S[17] ^
               S[19] ^ S[21] ^ S[25] ^ S[27] ^ S[29] ^ S[31];
      S2[24] = S[ 0] ^ S[ 8] ^ S[10] ^ S[12] ^ S[16] ^ S[18] ^ S[20] ^ S[22] ^
               S[24] ^ S[26] ^ S[28];
      S2[25] = S[ 1] ^ S[ 9] ^ S[11] ^ S[13] ^ S[17] ^ S[19] ^ S[21] ^ S[23] ^
               S[25] ^ S[27] ^ S[29];
      S2[26] = S[ 2] ^ S[10] ^ S[12] ^ S[14] ^ S[18] ^ S[20] ^ S[22] ^ S[24] ^
               S[26] ^ S[28] ^ S[30];
      S2[27] = S[ 3] ^ S[11] ^ S[13] ^ S[15] ^ S[19] ^ S[21] ^ S[23] ^ S[25] ^
               S[27] ^ S[29] ^ S[31];
      S2[28] = S[ 0] ^ S[ 2] ^ S[ 6] ^ S[12] ^ S[14] ^ S[16] ^ S[20] ^ S[22] ^ S[26] ^ S[28];
      S2[29] = S[ 1] ^ S[ 3] ^ S[ 7] ^ S[13] ^ S[15] ^ S[17] ^ S[21] ^ S[23] ^ S[27] ^ S[29];
      S2[30] = S[ 2] ^ S[ 4] ^ S[ 8] ^ S[14] ^ S[16] ^ S[18] ^ S[22] ^ S[24] ^ S[28] ^ S[30];
      S2[31] = S[ 3] ^ S[ 5] ^ S[ 9] ^ S[15] ^ S[17] ^ S[19] ^ S[23] ^ S[25] ^ S[29] ^ S[31];

      copy_mem(m_hash.data(), S2, 32);
      }
   }

/**
* Produce the final GOST 34.11 output
*/
void GOST_34_11::final_result(uint8_t out[])
   {
   if(m_position)
      {
      clear_mem(m_buffer.data() + m_position, m_buffer.size() - m_position);
      compress_n(m_buffer.data(), 1);
      }

   secure_vector<uint8_t> length_buf(32);
   const uint64_t bit_count = m_count * 8;
   store_le(bit_count, length_buf.data());

   secure_vector<uint8_t> sum_buf = m_sum;

   compress_n(length_buf.data(), 1);
   compress_n(sum_buf.data(), 1);

   copy_mem(out, m_hash.data(), 32);

   clear();
   }

}
/*
* Hash Functions
* (C) 2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_ADLER32)
#endif

#if defined(BOTAN_HAS_CRC24)
#endif

#if defined(BOTAN_HAS_CRC32)
#endif

#if defined(BOTAN_HAS_GOST_34_11)
#endif

#if defined(BOTAN_HAS_KECCAK)
#endif

#if defined(BOTAN_HAS_MD4)
#endif

#if defined(BOTAN_HAS_MD5)
#endif

#if defined(BOTAN_HAS_RIPEMD_160)
#endif

#if defined(BOTAN_HAS_SHA1)
#endif

#if defined(BOTAN_HAS_SHA2_32)
#endif

#if defined(BOTAN_HAS_SHA2_64)
#endif

#if defined(BOTAN_HAS_SHA3)
#endif

#if defined(BOTAN_HAS_SHAKE)
#endif

#if defined(BOTAN_HAS_SKEIN_512)
#endif

#if defined(BOTAN_HAS_STREEBOG)
#endif

#if defined(BOTAN_HAS_SM3)
#endif

#if defined(BOTAN_HAS_TIGER)
#endif

#if defined(BOTAN_HAS_WHIRLPOOL)
#endif

#if defined(BOTAN_HAS_PARALLEL_HASH)
#endif

#if defined(BOTAN_HAS_COMB4P)
#endif

#if defined(BOTAN_HAS_BLAKE2B)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

#if defined(BOTAN_HAS_COMMONCRYPTO)
#endif

namespace Botan {

std::unique_ptr<HashFunction> HashFunction::create(const std::string& algo_spec,
                                                   const std::string& provider)
   {

#if defined(BOTAN_HAS_COMMONCRYPTO)
   if(provider.empty() || provider == "commoncrypto")
      {
      if(auto hash = make_commoncrypto_hash(algo_spec))
         return hash;

      if(!provider.empty())
         return nullptr;
      }
#endif

#if defined(BOTAN_HAS_OPENSSL)
   if(provider.empty() || provider == "openssl")
      {
      if(auto hash = make_openssl_hash(algo_spec))
         return hash;

      if(!provider.empty())
         return nullptr;
      }
#endif

   if(provider.empty() == false && provider != "base")
      return nullptr; // unknown provider

#if defined(BOTAN_HAS_SHA1)
   if(algo_spec == "SHA-160" ||
      algo_spec == "SHA-1" ||
      algo_spec == "SHA1")
      {
      return std::unique_ptr<HashFunction>(new SHA_160);
      }
#endif

#if defined(BOTAN_HAS_SHA2_32)
   if(algo_spec == "SHA-224")
      {
      return std::unique_ptr<HashFunction>(new SHA_224);
      }

   if(algo_spec == "SHA-256")
      {
      return std::unique_ptr<HashFunction>(new SHA_256);
      }
#endif

#if defined(BOTAN_HAS_SHA2_64)
   if(algo_spec == "SHA-384")
      {
      return std::unique_ptr<HashFunction>(new SHA_384);
      }

   if(algo_spec == "SHA-512")
      {
      return std::unique_ptr<HashFunction>(new SHA_512);
      }

   if(algo_spec == "SHA-512-256")
      {
      return std::unique_ptr<HashFunction>(new SHA_512_256);
      }
#endif

#if defined(BOTAN_HAS_RIPEMD_160)
   if(algo_spec == "RIPEMD-160")
      {
      return std::unique_ptr<HashFunction>(new RIPEMD_160);
      }
#endif

#if defined(BOTAN_HAS_WHIRLPOOL)
   if(algo_spec == "Whirlpool")
      {
      return std::unique_ptr<HashFunction>(new Whirlpool);
      }
#endif

#if defined(BOTAN_HAS_MD5)
   if(algo_spec == "MD5")
      {
      return std::unique_ptr<HashFunction>(new MD5);
      }
#endif

#if defined(BOTAN_HAS_MD4)
   if(algo_spec == "MD4")
      {
      return std::unique_ptr<HashFunction>(new MD4);
      }
#endif

#if defined(BOTAN_HAS_GOST_34_11)
   if(algo_spec == "GOST-R-34.11-94" || algo_spec == "GOST-34.11")
      {
      return std::unique_ptr<HashFunction>(new GOST_34_11);
      }
#endif

#if defined(BOTAN_HAS_ADLER32)
   if(algo_spec == "Adler32")
      {
      return std::unique_ptr<HashFunction>(new Adler32);
      }
#endif

#if defined(BOTAN_HAS_CRC24)
   if(algo_spec == "CRC24")
      {
      return std::unique_ptr<HashFunction>(new CRC24);
      }
#endif

#if defined(BOTAN_HAS_CRC32)
   if(algo_spec == "CRC32")
      {
      return std::unique_ptr<HashFunction>(new CRC32);
      }
#endif

   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_TIGER)
   if(req.algo_name() == "Tiger")
      {
      return std::unique_ptr<HashFunction>(
         new Tiger(req.arg_as_integer(0, 24),
                   req.arg_as_integer(1, 3)));
      }
#endif

#if defined(BOTAN_HAS_SKEIN_512)
   if(req.algo_name() == "Skein-512")
      {
      return std::unique_ptr<HashFunction>(
         new Skein_512(req.arg_as_integer(0, 512), req.arg(1, "")));
      }
#endif

#if defined(BOTAN_HAS_BLAKE2B)
   if(req.algo_name() == "Blake2b" || req.algo_name() == "BLAKE2b")
      {
      return std::unique_ptr<HashFunction>(
         new Blake2b(req.arg_as_integer(0, 512)));
   }
#endif

#if defined(BOTAN_HAS_KECCAK)
   if(req.algo_name() == "Keccak-1600")
      {
      return std::unique_ptr<HashFunction>(
         new Keccak_1600(req.arg_as_integer(0, 512)));
      }
#endif

#if defined(BOTAN_HAS_SHA3)
   if(req.algo_name() == "SHA-3")
      {
      return std::unique_ptr<HashFunction>(
         new SHA_3(req.arg_as_integer(0, 512)));
      }
#endif

#if defined(BOTAN_HAS_SHAKE)
   if(req.algo_name() == "SHAKE-128")
      {
      return std::unique_ptr<HashFunction>(new SHAKE_128(req.arg_as_integer(0, 128)));
      }
   if(req.algo_name() == "SHAKE-256")
      {
      return std::unique_ptr<HashFunction>(new SHAKE_256(req.arg_as_integer(0, 256)));
      }
#endif

#if defined(BOTAN_HAS_STREEBOG)
   if(algo_spec == "Streebog-256")
      {
      return std::unique_ptr<HashFunction>(new Streebog_256);
      }
   if(algo_spec == "Streebog-512")
      {
      return std::unique_ptr<HashFunction>(new Streebog_512);
      }
#endif

#if defined(BOTAN_HAS_SM3)
   if(algo_spec == "SM3")
      {
      return std::unique_ptr<HashFunction>(new SM3);
      }
#endif

#if defined(BOTAN_HAS_WHIRLPOOL)
   if(req.algo_name() == "Whirlpool")
      {
      return std::unique_ptr<HashFunction>(new Whirlpool);
      }
#endif

#if defined(BOTAN_HAS_PARALLEL_HASH)
   if(req.algo_name() == "Parallel")
      {
      std::vector<std::unique_ptr<HashFunction>> hashes;

      for(size_t i = 0; i != req.arg_count(); ++i)
         {
         auto h = HashFunction::create(req.arg(i));
         if(!h)
            {
            return nullptr;
            }
         hashes.push_back(std::move(h));
         }

      return std::unique_ptr<HashFunction>(new Parallel(hashes));
      }
#endif

#if defined(BOTAN_HAS_COMB4P)
   if(req.algo_name() == "Comb4P" && req.arg_count() == 2)
      {
      std::unique_ptr<HashFunction> h1(HashFunction::create(req.arg(0)));
      std::unique_ptr<HashFunction> h2(HashFunction::create(req.arg(1)));

      if(h1 && h2)
         return std::unique_ptr<HashFunction>(new Comb4P(h1.release(), h2.release()));
      }
#endif


   return nullptr;
   }

//static
std::unique_ptr<HashFunction>
HashFunction::create_or_throw(const std::string& algo,
                              const std::string& provider)
   {
   if(auto hash = HashFunction::create(algo, provider))
      {
      return hash;
      }
   throw Lookup_Error("Hash", algo, provider);
   }

std::vector<std::string> HashFunction::providers(const std::string& algo_spec)
   {
   return probe_providers_of<HashFunction>(algo_spec, {"base", "openssl", "commoncrypto"});
   }

}

/*
* Hex Encoding and Decoding
* (C) 2010,2020 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

char hex_encode_nibble(uint8_t n, bool uppercase)
   {
   BOTAN_DEBUG_ASSERT(n <= 15);

   const auto in_09 = CT::Mask<uint8_t>::is_lt(n, 10);

   const char c_09 = n + '0';
   const char c_af = n + (uppercase ? 'A' : 'a') - 10;

   return in_09.select(c_09, c_af);
   }

}

void hex_encode(char output[],
                const uint8_t input[],
                size_t input_length,
                bool uppercase)
   {
   for(size_t i = 0; i != input_length; ++i)
      {
      const uint8_t n0 = (input[i] >> 4) & 0xF;
      const uint8_t n1 = (input[i]     ) & 0xF;

      output[2*i  ] = hex_encode_nibble(n0, uppercase);
      output[2*i+1] = hex_encode_nibble(n1, uppercase);
      }
   }

std::string hex_encode(const uint8_t input[],
                       size_t input_length,
                       bool uppercase)
   {
   std::string output(2 * input_length, 0);

   if(input_length)
      hex_encode(&output.front(), input, input_length, uppercase);

   return output;
   }

namespace {

uint8_t hex_char_to_bin(char input)
   {
   const uint8_t c = static_cast<uint8_t>(input);

   const auto is_alpha_upper = CT::Mask<uint8_t>::is_within_range(c, uint8_t('A'), uint8_t('F'));
   const auto is_alpha_lower = CT::Mask<uint8_t>::is_within_range(c, uint8_t('a'), uint8_t('f'));
   const auto is_decimal     = CT::Mask<uint8_t>::is_within_range(c, uint8_t('0'), uint8_t('9'));

   const auto is_whitespace  = CT::Mask<uint8_t>::is_any_of(c, {
         uint8_t(' '), uint8_t('\t'), uint8_t('\n'), uint8_t('\r')
      });

   const uint8_t c_upper = c - uint8_t('A') + 10;
   const uint8_t c_lower = c - uint8_t('a') + 10;
   const uint8_t c_decim = c - uint8_t('0');

   uint8_t ret = 0xFF; // default value

   ret = is_alpha_upper.select(c_upper, ret);
   ret = is_alpha_lower.select(c_lower, ret);
   ret = is_decimal.select(c_decim, ret);
   ret = is_whitespace.select(0x80, ret);

   return ret;
   }

}


size_t hex_decode(uint8_t output[],
                  const char input[],
                  size_t input_length,
                  size_t& input_consumed,
                  bool ignore_ws)
   {
   uint8_t* out_ptr = output;
   bool top_nibble = true;

   clear_mem(output, input_length / 2);

   for(size_t i = 0; i != input_length; ++i)
      {
      const uint8_t bin = hex_char_to_bin(input[i]);

      if(bin >= 0x10)
         {
         if(bin == 0x80 && ignore_ws)
            continue;

         std::string bad_char(1, input[i]);
         if(bad_char == "\t")
           bad_char = "\\t";
         else if(bad_char == "\n")
           bad_char = "\\n";

         throw Invalid_Argument(
           std::string("hex_decode: invalid hex character '") +
           bad_char + "'");
         }

      if(top_nibble)
         *out_ptr |= bin << 4;
      else
         *out_ptr |= bin;

      top_nibble = !top_nibble;
      if(top_nibble)
         ++out_ptr;
      }

   input_consumed = input_length;
   size_t written = (out_ptr - output);

   /*
   * We only got half of a uint8_t at the end; zap the half-written
   * output and mark it as unread
   */
   if(!top_nibble)
      {
      *out_ptr = 0;
      input_consumed -= 1;
      }

   return written;
   }

size_t hex_decode(uint8_t output[],
                  const char input[],
                  size_t input_length,
                  bool ignore_ws)
   {
   size_t consumed = 0;
   size_t written = hex_decode(output, input, input_length,
                               consumed, ignore_ws);

   if(consumed != input_length)
      throw Invalid_Argument("hex_decode: input did not have full bytes");

   return written;
   }

size_t hex_decode(uint8_t output[],
                  const std::string& input,
                  bool ignore_ws)
   {
   return hex_decode(output, input.data(), input.length(), ignore_ws);
   }

secure_vector<uint8_t> hex_decode_locked(const char input[],
                                      size_t input_length,
                                      bool ignore_ws)
   {
   secure_vector<uint8_t> bin(1 + input_length / 2);

   size_t written = hex_decode(bin.data(),
                               input,
                               input_length,
                               ignore_ws);

   bin.resize(written);
   return bin;
   }

secure_vector<uint8_t> hex_decode_locked(const std::string& input,
                                      bool ignore_ws)
   {
   return hex_decode_locked(input.data(), input.size(), ignore_ws);
   }

std::vector<uint8_t> hex_decode(const char input[],
                             size_t input_length,
                             bool ignore_ws)
   {
   std::vector<uint8_t> bin(1 + input_length / 2);

   size_t written = hex_decode(bin.data(),
                               input,
                               input_length,
                               ignore_ws);

   bin.resize(written);
   return bin;
   }

std::vector<uint8_t> hex_decode(const std::string& input,
                             bool ignore_ws)
   {
   return hex_decode(input.data(), input.size(), ignore_ws);
   }

}
/*
* HMAC
* (C) 1999-2007,2014,2020 Jack Lloyd
*     2007 Yves Jerschow
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* Update a HMAC Calculation
*/
void HMAC::add_data(const uint8_t input[], size_t length)
   {
   verify_key_set(m_ikey.empty() == false);
   m_hash->update(input, length);
   }

/*
* Finalize a HMAC Calculation
*/
void HMAC::final_result(uint8_t mac[])
   {
   verify_key_set(m_okey.empty() == false);
   m_hash->final(mac);
   m_hash->update(m_okey);
   m_hash->update(mac, m_hash_output_length);
   m_hash->final(mac);
   m_hash->update(m_ikey);
   }

Key_Length_Specification HMAC::key_spec() const
   {
   // Support very long lengths for things like PBKDF2 and the TLS PRF
   return Key_Length_Specification(0, 4096);
   }

size_t HMAC::output_length() const
   {
   return m_hash_output_length;
   }

/*
* HMAC Key Schedule
*/
void HMAC::key_schedule(const uint8_t key[], size_t length)
   {
   const uint8_t ipad = 0x36;
   const uint8_t opad = 0x5C;

   m_hash->clear();

   m_ikey.resize(m_hash_block_size);
   m_okey.resize(m_hash_block_size);

   clear_mem(m_ikey.data(), m_ikey.size());
   clear_mem(m_okey.data(), m_okey.size());

   /*
   * Sometimes the HMAC key length itself is sensitive, as with PBKDF2 where it
   * reveals the length of the passphrase. Make some attempt to hide this to
   * side channels. Clearly if the secret is longer than the block size then the
   * branch to hash first reveals that. In addition, counting the number of
   * compression functions executed reveals the size at the granularity of the
   * hash function's block size.
   *
   * The greater concern is for smaller keys; being able to detect when a
   * passphrase is say 4 bytes may assist choosing weaker targets. Even though
   * the loop bounds are constant, we can only actually read key[0..length] so
   * it doesn't seem possible to make this computation truly constant time.
   *
   * We don't mind leaking if the length is exactly zero since that's
   * trivial to simply check.
   */

   if(length > m_hash_block_size)
      {
      m_hash->update(key, length);
      m_hash->final(m_ikey.data());
      }
   else if(length > 0)
      {
      for(size_t i = 0, i_mod_length = 0; i != m_hash_block_size; ++i)
         {
         /*
         access key[i % length] but avoiding division due to variable
         time computation on some processors.
         */
         auto needs_reduction = CT::Mask<size_t>::is_lte(length, i_mod_length);
         i_mod_length = needs_reduction.select(0, i_mod_length);
         const uint8_t kb = key[i_mod_length];

         auto in_range = CT::Mask<size_t>::is_lt(i, length);
         m_ikey[i] = static_cast<uint8_t>(in_range.if_set_return(kb));
         i_mod_length += 1;
         }
      }

   for(size_t i = 0; i != m_hash_block_size; ++i)
      {
      m_ikey[i] ^= ipad;
      m_okey[i] = m_ikey[i] ^ ipad ^ opad;
      }

   m_hash->update(m_ikey);
   }

/*
* Clear memory of sensitive data
*/
void HMAC::clear()
   {
   m_hash->clear();
   zap(m_ikey);
   zap(m_okey);
   }

/*
* Return the name of this type
*/
std::string HMAC::name() const
   {
   return "HMAC(" + m_hash->name() + ")";
   }

/*
* Return a clone of this object
*/
MessageAuthenticationCode* HMAC::clone() const
   {
   return new HMAC(m_hash->clone());
   }

/*
* HMAC Constructor
*/
HMAC::HMAC(HashFunction* hash) :
   m_hash(hash),
   m_hash_output_length(m_hash->output_length()),
   m_hash_block_size(m_hash->hash_block_size())
   {
   BOTAN_ARG_CHECK(m_hash_block_size >= m_hash_output_length,
                   "HMAC is not compatible with this hash function");
   }

}
/*
* HMAC_DRBG
* (C) 2014,2015,2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

size_t hmac_drbg_security_level(size_t mac_output_length)
   {
   // security strength of the hash function
   // for pre-image resistance (see NIST SP 800-57)
   // SHA-160: 128 bits
   // SHA-224, SHA-512/224: 192 bits,
   // SHA-256, SHA-512/256, SHA-384, SHA-512: >= 256 bits
   // NIST SP 800-90A only supports up to 256 bits though

   if(mac_output_length < 32)
      {
      return (mac_output_length - 4) * 8;
      }
   else
      {
      return 32 * 8;
      }
   }

void check_limits(size_t reseed_interval,
                  size_t max_number_of_bytes_per_request)
   {
   // SP800-90A permits up to 2^48, but it is not usable on 32 bit
   // platforms, so we only allow up to 2^24, which is still reasonably high
   if(reseed_interval == 0 || reseed_interval > static_cast<size_t>(1) << 24)
      {
      throw Invalid_Argument("Invalid value for reseed_interval");
      }

   if(max_number_of_bytes_per_request == 0 || max_number_of_bytes_per_request > 64 * 1024)
      {
      throw Invalid_Argument("Invalid value for max_number_of_bytes_per_request");
      }
   }

}

HMAC_DRBG::HMAC_DRBG(std::unique_ptr<MessageAuthenticationCode> prf,
                     RandomNumberGenerator& underlying_rng,
                     size_t reseed_interval,
                     size_t max_number_of_bytes_per_request) :
   Stateful_RNG(underlying_rng, reseed_interval),
   m_mac(std::move(prf)),
   m_max_number_of_bytes_per_request(max_number_of_bytes_per_request),
   m_security_level(hmac_drbg_security_level(m_mac->output_length()))
   {
   BOTAN_ASSERT_NONNULL(m_mac);

   check_limits(reseed_interval, max_number_of_bytes_per_request);

   clear();
   }

HMAC_DRBG::HMAC_DRBG(std::unique_ptr<MessageAuthenticationCode> prf,
                     RandomNumberGenerator& underlying_rng,
                     Entropy_Sources& entropy_sources,
                     size_t reseed_interval,
                     size_t max_number_of_bytes_per_request) :
   Stateful_RNG(underlying_rng, entropy_sources, reseed_interval),
   m_mac(std::move(prf)),
   m_max_number_of_bytes_per_request(max_number_of_bytes_per_request),
   m_security_level(hmac_drbg_security_level(m_mac->output_length()))
   {
   BOTAN_ASSERT_NONNULL(m_mac);

   check_limits(reseed_interval, max_number_of_bytes_per_request);

   clear();
   }

HMAC_DRBG::HMAC_DRBG(std::unique_ptr<MessageAuthenticationCode> prf,
                     Entropy_Sources& entropy_sources,
                     size_t reseed_interval,
                     size_t max_number_of_bytes_per_request) :
   Stateful_RNG(entropy_sources, reseed_interval),
   m_mac(std::move(prf)),
   m_max_number_of_bytes_per_request(max_number_of_bytes_per_request),
   m_security_level(hmac_drbg_security_level(m_mac->output_length()))
   {
   BOTAN_ASSERT_NONNULL(m_mac);

   check_limits(reseed_interval, max_number_of_bytes_per_request);

   clear();
   }

HMAC_DRBG::HMAC_DRBG(std::unique_ptr<MessageAuthenticationCode> prf) :
   Stateful_RNG(),
   m_mac(std::move(prf)),
   m_max_number_of_bytes_per_request(64*1024),
   m_security_level(hmac_drbg_security_level(m_mac->output_length()))
   {
   BOTAN_ASSERT_NONNULL(m_mac);
   clear();
   }

HMAC_DRBG::HMAC_DRBG(const std::string& hmac_hash) :
   Stateful_RNG(),
   m_mac(MessageAuthenticationCode::create_or_throw("HMAC(" + hmac_hash + ")")),
   m_max_number_of_bytes_per_request(64 * 1024),
   m_security_level(hmac_drbg_security_level(m_mac->output_length()))
   {
   clear();
   }

void HMAC_DRBG::clear_state()
   {
   if(m_V.size() == 0)
      {
      const size_t output_length = m_mac->output_length();
      m_V.resize(output_length);
      }

   for(size_t i = 0; i != m_V.size(); ++i)
      m_V[i] = 0x01;
   m_mac->set_key(std::vector<uint8_t>(m_V.size(), 0x00));
   }

std::string HMAC_DRBG::name() const
   {
   return "HMAC_DRBG(" + m_mac->name() + ")";
   }

/*
* HMAC_DRBG generation
* See NIST SP800-90A section 10.1.2.5
*/
void HMAC_DRBG::generate_output(uint8_t output[], size_t output_len,
                                const uint8_t input[], size_t input_len)
   {
   if(input_len > 0)
      {
      update(input, input_len);
      }

   while(output_len > 0)
      {
      const size_t to_copy = std::min(output_len, m_V.size());
      m_mac->update(m_V.data(), m_V.size());
      m_mac->final(m_V.data());
      copy_mem(output, m_V.data(), to_copy);

      output += to_copy;
      output_len -= to_copy;
      }

   update(input, input_len);
   }

/*
* Reset V and the mac key with new values
* See NIST SP800-90A section 10.1.2.2
*/
void HMAC_DRBG::update(const uint8_t input[], size_t input_len)
   {
   secure_vector<uint8_t> T(m_V.size());
   m_mac->update(m_V);
   m_mac->update(0x00);
   m_mac->update(input, input_len);
   m_mac->final(T.data());
   m_mac->set_key(T);

   m_mac->update(m_V.data(), m_V.size());
   m_mac->final(m_V.data());

   if(input_len > 0)
      {
      m_mac->update(m_V);
      m_mac->update(0x01);
      m_mac->update(input, input_len);
      m_mac->final(T.data());
      m_mac->set_key(T);

      m_mac->update(m_V.data(), m_V.size());
      m_mac->final(m_V.data());
      }
   }

size_t HMAC_DRBG::security_level() const
   {
   return m_security_level;
   }
}
/*
* Keccak
* (C) 2010,2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> Keccak_1600::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new Keccak_1600(*this));
   }

Keccak_1600::Keccak_1600(size_t output_bits) :
   m_output_bits(output_bits),
   m_bitrate(1600 - 2*output_bits),
   m_S(25),
   m_S_pos(0)
   {
   // We only support the parameters for the SHA-3 proposal

   if(output_bits != 224 && output_bits != 256 &&
      output_bits != 384 && output_bits != 512)
      throw Invalid_Argument("Keccak_1600: Invalid output length " +
                             std::to_string(output_bits));
   }

std::string Keccak_1600::name() const
   {
   return "Keccak-1600(" + std::to_string(m_output_bits) + ")";
   }

HashFunction* Keccak_1600::clone() const
   {
   return new Keccak_1600(m_output_bits);
   }

void Keccak_1600::clear()
   {
   zeroise(m_S);
   m_S_pos = 0;
   }

void Keccak_1600::add_data(const uint8_t input[], size_t length)
   {
   m_S_pos = SHA_3::absorb(m_bitrate, m_S, m_S_pos, input, length);
   }

void Keccak_1600::final_result(uint8_t output[])
   {
   SHA_3::finish(m_bitrate, m_S, m_S_pos, 0x01, 0x80);

   /*
   * We never have to run the permutation again because we only support
   * limited output lengths
   */
   copy_out_vec_le(output, m_output_bits/8, m_S);

   clear();
   }

}
/*
* Message Authentication Code base class
* (C) 1999-2008 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_CBC_MAC)
#endif

#if defined(BOTAN_HAS_CMAC)
#endif

#if defined(BOTAN_HAS_GMAC)
#endif

#if defined(BOTAN_HAS_HMAC)
#endif

#if defined(BOTAN_HAS_POLY1305)
#endif

#if defined(BOTAN_HAS_SIPHASH)
#endif

#if defined(BOTAN_HAS_ANSI_X919_MAC)
#endif

namespace Botan {

std::unique_ptr<MessageAuthenticationCode>
MessageAuthenticationCode::create(const std::string& algo_spec,
                                  const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_GMAC)
   if(req.algo_name() == "GMAC" && req.arg_count() == 1)
      {
      if(provider.empty() || provider == "base")
         {
         if(auto bc = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new GMAC(bc.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_HMAC)
   if(req.algo_name() == "HMAC" && req.arg_count() == 1)
      {
      // TODO OpenSSL
      if(provider.empty() || provider == "base")
         {
         if(auto h = HashFunction::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new HMAC(h.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_POLY1305)
   if(req.algo_name() == "Poly1305" && req.arg_count() == 0)
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<MessageAuthenticationCode>(new Poly1305);
      }
#endif

#if defined(BOTAN_HAS_SIPHASH)
   if(req.algo_name() == "SipHash")
      {
      if(provider.empty() || provider == "base")
         {
         return std::unique_ptr<MessageAuthenticationCode>(
            new SipHash(req.arg_as_integer(0, 2), req.arg_as_integer(1, 4)));
         }
      }
#endif

#if defined(BOTAN_HAS_CMAC)
   if((req.algo_name() == "CMAC" || req.algo_name() == "OMAC") && req.arg_count() == 1)
      {
      // TODO: OpenSSL CMAC
      if(provider.empty() || provider == "base")
         {
         if(auto bc = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new CMAC(bc.release()));
         }
      }
#endif


#if defined(BOTAN_HAS_CBC_MAC)
   if(req.algo_name() == "CBC-MAC" && req.arg_count() == 1)
      {
      if(provider.empty() || provider == "base")
         {
         if(auto bc = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<MessageAuthenticationCode>(new CBC_MAC(bc.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_ANSI_X919_MAC)
   if(req.algo_name() == "X9.19-MAC")
      {
      if(provider.empty() || provider == "base")
         {
         return std::unique_ptr<MessageAuthenticationCode>(new ANSI_X919_MAC);
         }
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

std::vector<std::string>
MessageAuthenticationCode::providers(const std::string& algo_spec)
   {
   return probe_providers_of<MessageAuthenticationCode>(algo_spec, {"base", "openssl"});
   }

//static
std::unique_ptr<MessageAuthenticationCode>
MessageAuthenticationCode::create_or_throw(const std::string& algo,
                                           const std::string& provider)
   {
   if(auto mac = MessageAuthenticationCode::create(algo, provider))
      {
      return mac;
      }
   throw Lookup_Error("MAC", algo, provider);
   }

void MessageAuthenticationCode::start_msg(const uint8_t nonce[], size_t nonce_len)
   {
   BOTAN_UNUSED(nonce);
   if(nonce_len > 0)
      throw Invalid_IV_Length(name(), nonce_len);
   }

/*
* Default (deterministic) MAC verification operation
*/
bool MessageAuthenticationCode::verify_mac(const uint8_t mac[], size_t length)
   {
   secure_vector<uint8_t> our_mac = final();

   if(our_mac.size() != length)
      return false;

   return constant_time_compare(our_mac.data(), mac, length);
   }

}
/*
* MD4
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> MD4::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new MD4(*this));
   }

namespace {

inline void FF4(uint32_t& A, uint32_t& B, uint32_t& C, uint32_t& D,
                uint32_t M0, uint32_t M1, uint32_t M2, uint32_t M3)

   {
   A += (D ^ (B & (C ^ D))) + M0;
   A = rotl<3>(A);

   D += (C ^ (A & (B ^ C))) + M1;
   D = rotl<7>(D);

   C += (B ^ (D & (A ^ B))) + M2;
   C = rotl<11>(C);

   B += (A ^ (C & (D ^ A))) + M3;
   B = rotl<19>(B);
   }

inline void GG4(uint32_t& A, uint32_t& B, uint32_t& C, uint32_t& D,
                uint32_t M0, uint32_t M1, uint32_t M2, uint32_t M3)

   {
   A += ((B & C) | (D & (B | C))) + M0 + 0x5A827999;
   A = rotl<3>(A);

   D += ((A & B) | (C & (A | B))) + M1 + 0x5A827999;
   D = rotl<5>(D);

   C += ((D & A) | (B & (D | A))) + M2 + 0x5A827999;
   C = rotl<9>(C);

   B += ((C & D) | (A & (C | D))) + M3 + 0x5A827999;
   B = rotl<13>(B);
   }

inline void HH4(uint32_t& A, uint32_t& B, uint32_t& C, uint32_t& D,
                uint32_t M0, uint32_t M1, uint32_t M2, uint32_t M3)

   {
   A += (B ^ C ^ D) + M0 + 0x6ED9EBA1;
   A = rotl<3>(A);

   D += (A ^ B ^ C) + M1 + 0x6ED9EBA1;
   D = rotl<9>(D);

   C += (A ^ B ^ D) + M2 + 0x6ED9EBA1;
   C = rotl<11>(C);

   B += (A ^ C ^ D) + M3 + 0x6ED9EBA1;
   B = rotl<15>(B);
   }

}

/*
* MD4 Compression Function
*/
void MD4::compress_n(const uint8_t input[], size_t blocks)
   {
   uint32_t A = m_digest[0], B = m_digest[1], C = m_digest[2], D = m_digest[3];

   for(size_t i = 0; i != blocks; ++i)
      {
      uint32_t M00 = load_le<uint32_t>(input, 0);
      uint32_t M01 = load_le<uint32_t>(input, 1);
      uint32_t M02 = load_le<uint32_t>(input, 2);
      uint32_t M03 = load_le<uint32_t>(input, 3);
      uint32_t M04 = load_le<uint32_t>(input, 4);
      uint32_t M05 = load_le<uint32_t>(input, 5);
      uint32_t M06 = load_le<uint32_t>(input, 6);
      uint32_t M07 = load_le<uint32_t>(input, 7);
      uint32_t M08 = load_le<uint32_t>(input, 8);
      uint32_t M09 = load_le<uint32_t>(input, 9);
      uint32_t M10 = load_le<uint32_t>(input, 10);
      uint32_t M11 = load_le<uint32_t>(input, 11);
      uint32_t M12 = load_le<uint32_t>(input, 12);
      uint32_t M13 = load_le<uint32_t>(input, 13);
      uint32_t M14 = load_le<uint32_t>(input, 14);
      uint32_t M15 = load_le<uint32_t>(input, 15);

      FF4(A, B, C, D, M00, M01, M02, M03);
      FF4(A, B, C, D, M04, M05, M06, M07);
      FF4(A, B, C, D, M08, M09, M10, M11);
      FF4(A, B, C, D, M12, M13, M14, M15);

      GG4(A, B, C, D, M00, M04, M08, M12);
      GG4(A, B, C, D, M01, M05, M09, M13);
      GG4(A, B, C, D, M02, M06, M10, M14);
      GG4(A, B, C, D, M03, M07, M11, M15);

      HH4(A, B, C, D, M00, M08, M04, M12);
      HH4(A, B, C, D, M02, M10, M06, M14);
      HH4(A, B, C, D, M01, M09, M05, M13);
      HH4(A, B, C, D, M03, M11, M07, M15);

      A = (m_digest[0] += A);
      B = (m_digest[1] += B);
      C = (m_digest[2] += C);
      D = (m_digest[3] += D);

      input += hash_block_size();
      }
   }

/*
* Copy out the digest
*/
void MD4::copy_out(uint8_t output[])
   {
   copy_out_vec_le(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void MD4::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0x67452301;
   m_digest[1] = 0xEFCDAB89;
   m_digest[2] = 0x98BADCFE;
   m_digest[3] = 0x10325476;
   }

}
/*
* MD5
* (C) 1999-2008 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> MD5::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new MD5(*this));
   }

namespace {

/*
* MD5 FF Function
*/
template<size_t S>
inline void FF(uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t M)
   {
   A += (D ^ (B & (C ^ D))) + M;
   A  = rotl<S>(A) + B;
   }

/*
* MD5 GG Function
*/
template<size_t S>
inline void GG(uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t M)
   {
   A += (C ^ (D & (B ^ C))) + M;
   A  = rotl<S>(A) + B;
   }

/*
* MD5 HH Function
*/
template<size_t S>
inline void HH(uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t M)
   {
   A += (B ^ C ^ D) + M;
   A  = rotl<S>(A) + B;
   }

/*
* MD5 II Function
*/
template<size_t S>
inline void II(uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t M)
   {
   A += (C ^ (B | ~D)) + M;
   A  = rotl<S>(A) + B;
   }

}

/*
* MD5 Compression Function
*/
void MD5::compress_n(const uint8_t input[], size_t blocks)
   {
   uint32_t A = m_digest[0], B = m_digest[1], C = m_digest[2], D = m_digest[3];

   for(size_t i = 0; i != blocks; ++i)
      {
      load_le(m_M.data(), input, m_M.size());

      FF< 7>(A,B,C,D,m_M[ 0]+0xD76AA478);   FF<12>(D,A,B,C,m_M[ 1]+0xE8C7B756);
      FF<17>(C,D,A,B,m_M[ 2]+0x242070DB);   FF<22>(B,C,D,A,m_M[ 3]+0xC1BDCEEE);
      FF< 7>(A,B,C,D,m_M[ 4]+0xF57C0FAF);   FF<12>(D,A,B,C,m_M[ 5]+0x4787C62A);
      FF<17>(C,D,A,B,m_M[ 6]+0xA8304613);   FF<22>(B,C,D,A,m_M[ 7]+0xFD469501);
      FF< 7>(A,B,C,D,m_M[ 8]+0x698098D8);   FF<12>(D,A,B,C,m_M[ 9]+0x8B44F7AF);
      FF<17>(C,D,A,B,m_M[10]+0xFFFF5BB1);   FF<22>(B,C,D,A,m_M[11]+0x895CD7BE);
      FF< 7>(A,B,C,D,m_M[12]+0x6B901122);   FF<12>(D,A,B,C,m_M[13]+0xFD987193);
      FF<17>(C,D,A,B,m_M[14]+0xA679438E);   FF<22>(B,C,D,A,m_M[15]+0x49B40821);

      GG< 5>(A,B,C,D,m_M[ 1]+0xF61E2562);   GG< 9>(D,A,B,C,m_M[ 6]+0xC040B340);
      GG<14>(C,D,A,B,m_M[11]+0x265E5A51);   GG<20>(B,C,D,A,m_M[ 0]+0xE9B6C7AA);
      GG< 5>(A,B,C,D,m_M[ 5]+0xD62F105D);   GG< 9>(D,A,B,C,m_M[10]+0x02441453);
      GG<14>(C,D,A,B,m_M[15]+0xD8A1E681);   GG<20>(B,C,D,A,m_M[ 4]+0xE7D3FBC8);
      GG< 5>(A,B,C,D,m_M[ 9]+0x21E1CDE6);   GG< 9>(D,A,B,C,m_M[14]+0xC33707D6);
      GG<14>(C,D,A,B,m_M[ 3]+0xF4D50D87);   GG<20>(B,C,D,A,m_M[ 8]+0x455A14ED);
      GG< 5>(A,B,C,D,m_M[13]+0xA9E3E905);   GG< 9>(D,A,B,C,m_M[ 2]+0xFCEFA3F8);
      GG<14>(C,D,A,B,m_M[ 7]+0x676F02D9);   GG<20>(B,C,D,A,m_M[12]+0x8D2A4C8A);

      HH< 4>(A,B,C,D,m_M[ 5]+0xFFFA3942);   HH<11>(D,A,B,C,m_M[ 8]+0x8771F681);
      HH<16>(C,D,A,B,m_M[11]+0x6D9D6122);   HH<23>(B,C,D,A,m_M[14]+0xFDE5380C);
      HH< 4>(A,B,C,D,m_M[ 1]+0xA4BEEA44);   HH<11>(D,A,B,C,m_M[ 4]+0x4BDECFA9);
      HH<16>(C,D,A,B,m_M[ 7]+0xF6BB4B60);   HH<23>(B,C,D,A,m_M[10]+0xBEBFBC70);
      HH< 4>(A,B,C,D,m_M[13]+0x289B7EC6);   HH<11>(D,A,B,C,m_M[ 0]+0xEAA127FA);
      HH<16>(C,D,A,B,m_M[ 3]+0xD4EF3085);   HH<23>(B,C,D,A,m_M[ 6]+0x04881D05);
      HH< 4>(A,B,C,D,m_M[ 9]+0xD9D4D039);   HH<11>(D,A,B,C,m_M[12]+0xE6DB99E5);
      HH<16>(C,D,A,B,m_M[15]+0x1FA27CF8);   HH<23>(B,C,D,A,m_M[ 2]+0xC4AC5665);

      II< 6>(A,B,C,D,m_M[ 0]+0xF4292244);   II<10>(D,A,B,C,m_M[ 7]+0x432AFF97);
      II<15>(C,D,A,B,m_M[14]+0xAB9423A7);   II<21>(B,C,D,A,m_M[ 5]+0xFC93A039);
      II< 6>(A,B,C,D,m_M[12]+0x655B59C3);   II<10>(D,A,B,C,m_M[ 3]+0x8F0CCC92);
      II<15>(C,D,A,B,m_M[10]+0xFFEFF47D);   II<21>(B,C,D,A,m_M[ 1]+0x85845DD1);
      II< 6>(A,B,C,D,m_M[ 8]+0x6FA87E4F);   II<10>(D,A,B,C,m_M[15]+0xFE2CE6E0);
      II<15>(C,D,A,B,m_M[ 6]+0xA3014314);   II<21>(B,C,D,A,m_M[13]+0x4E0811A1);
      II< 6>(A,B,C,D,m_M[ 4]+0xF7537E82);   II<10>(D,A,B,C,m_M[11]+0xBD3AF235);
      II<15>(C,D,A,B,m_M[ 2]+0x2AD7D2BB);   II<21>(B,C,D,A,m_M[ 9]+0xEB86D391);

      A = (m_digest[0] += A);
      B = (m_digest[1] += B);
      C = (m_digest[2] += C);
      D = (m_digest[3] += D);

      input += hash_block_size();
      }
   }

/*
* Copy out the digest
*/
void MD5::copy_out(uint8_t output[])
   {
   copy_out_vec_le(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void MD5::clear()
   {
   MDx_HashFunction::clear();
   zeroise(m_M);
   m_digest[0] = 0x67452301;
   m_digest[1] = 0xEFCDAB89;
   m_digest[2] = 0x98BADCFE;
   m_digest[3] = 0x10325476;
   }

}
/*
* Merkle-Damgard Hash Function
* (C) 1999-2008,2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
* MDx_HashFunction Constructor
*/
MDx_HashFunction::MDx_HashFunction(size_t block_len,
                                   bool byte_big_endian,
                                   bool bit_big_endian,
                                   uint8_t cnt_size) :
   m_pad_char(bit_big_endian == true ? 0x80 : 0x01),
   m_counter_size(cnt_size),
   m_block_bits(ceil_log2(block_len)),
   m_count_big_endian(byte_big_endian),
   m_count(0),
   m_buffer(block_len),
   m_position(0)
   {
   if(!is_power_of_2(block_len))
      throw Invalid_Argument("MDx_HashFunction block length must be a power of 2");
   if(m_block_bits < 3 || m_block_bits > 16)
      throw Invalid_Argument("MDx_HashFunction block size too large or too small");
   if(m_counter_size < 8 || m_counter_size > block_len)
      throw Invalid_State("MDx_HashFunction invalid counter length");
   }

/*
* Clear memory of sensitive data
*/
void MDx_HashFunction::clear()
   {
   zeroise(m_buffer);
   m_count = m_position = 0;
   }

/*
* Update the hash
*/
void MDx_HashFunction::add_data(const uint8_t input[], size_t length)
   {
   const size_t block_len = static_cast<size_t>(1) << m_block_bits;

   m_count += length;

   if(m_position)
      {
      buffer_insert(m_buffer, m_position, input, length);

      if(m_position + length >= block_len)
         {
         compress_n(m_buffer.data(), 1);
         input += (block_len - m_position);
         length -= (block_len - m_position);
         m_position = 0;
         }
      }

   // Just in case the compiler can't figure out block_len is a power of 2
   const size_t full_blocks = length >> m_block_bits;
   const size_t remaining   = length & (block_len - 1);

   if(full_blocks > 0)
      {
      compress_n(input, full_blocks);
      }

   buffer_insert(m_buffer, m_position, input + full_blocks * block_len, remaining);
   m_position += remaining;
   }

/*
* Finalize a hash
*/
void MDx_HashFunction::final_result(uint8_t output[])
   {
   const size_t block_len = static_cast<size_t>(1) << m_block_bits;

   clear_mem(&m_buffer[m_position], block_len - m_position);
   m_buffer[m_position] = m_pad_char;

   if(m_position >= block_len - m_counter_size)
      {
      compress_n(m_buffer.data(), 1);
      zeroise(m_buffer);
      }

   write_count(&m_buffer[block_len - m_counter_size]);

   compress_n(m_buffer.data(), 1);
   copy_out(output);
   clear();
   }

/*
* Write the count bits to the buffer
*/
void MDx_HashFunction::write_count(uint8_t out[])
   {
   BOTAN_ASSERT_NOMSG(m_counter_size <= output_length());
   BOTAN_ASSERT_NOMSG(m_counter_size >= 8);

   const uint64_t bit_count = m_count * 8;

   if(m_count_big_endian)
      store_be(bit_count, out + m_counter_size - 8);
   else
      store_le(bit_count, out + m_counter_size - 8);
   }

}
/*
* Cipher Modes
* (C) 2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_BLOCK_CIPHER)
#endif

#if defined(BOTAN_HAS_AEAD_MODES)
#endif

#if defined(BOTAN_HAS_MODE_CBC)
#endif

#if defined(BOTAN_HAS_MODE_CFB)
#endif

#if defined(BOTAN_HAS_MODE_XTS)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

#if defined(BOTAN_HAS_COMMONCRYPTO)
#endif

namespace Botan {

std::unique_ptr<Cipher_Mode> Cipher_Mode::create_or_throw(const std::string& algo,
                                                          Cipher_Dir direction,
                                                          const std::string& provider)
   {
   if(auto mode = Cipher_Mode::create(algo, direction, provider))
      return mode;

   throw Lookup_Error("Cipher mode", algo, provider);
   }

std::unique_ptr<Cipher_Mode> Cipher_Mode::create(const std::string& algo,
                                                 Cipher_Dir direction,
                                                 const std::string& provider)
   {
#if defined(BOTAN_HAS_COMMONCRYPTO)
   if(provider.empty() || provider == "commoncrypto")
      {
      std::unique_ptr<Cipher_Mode> commoncrypto_cipher(make_commoncrypto_cipher_mode(algo, direction));

      if(commoncrypto_cipher)
         return commoncrypto_cipher;

      if(!provider.empty())
         return std::unique_ptr<Cipher_Mode>();
      }
#endif

#if defined(BOTAN_HAS_OPENSSL)
   if(provider.empty() || provider == "openssl")
      {
      std::unique_ptr<Cipher_Mode> openssl_cipher(make_openssl_cipher_mode(algo, direction));

      if(openssl_cipher)
         return openssl_cipher;

      if(!provider.empty())
         return std::unique_ptr<Cipher_Mode>();
      }
#endif

#if defined(BOTAN_HAS_STREAM_CIPHER)
   if(auto sc = StreamCipher::create(algo))
      {
      return std::unique_ptr<Cipher_Mode>(new Stream_Cipher_Mode(sc.release()));
      }
#endif

#if defined(BOTAN_HAS_AEAD_MODES)
   if(auto aead = AEAD_Mode::create(algo, direction))
      {
      return std::unique_ptr<Cipher_Mode>(aead.release());
      }
#endif

   if(algo.find('/') != std::string::npos)
      {
      const std::vector<std::string> algo_parts = split_on(algo, '/');
      const std::string cipher_name = algo_parts[0];
      const std::vector<std::string> mode_info = parse_algorithm_name(algo_parts[1]);

      if(mode_info.empty())
         return std::unique_ptr<Cipher_Mode>();

      std::ostringstream alg_args;

      alg_args << '(' << cipher_name;
      for(size_t i = 1; i < mode_info.size(); ++i)
         alg_args << ',' << mode_info[i];
      for(size_t i = 2; i < algo_parts.size(); ++i)
         alg_args << ',' << algo_parts[i];
      alg_args << ')';

      const std::string mode_name = mode_info[0] + alg_args.str();
      return Cipher_Mode::create(mode_name, direction, provider);
      }

#if defined(BOTAN_HAS_BLOCK_CIPHER)

   SCAN_Name spec(algo);

   if(spec.arg_count() == 0)
      {
      return std::unique_ptr<Cipher_Mode>();
      }

   std::unique_ptr<BlockCipher> bc(BlockCipher::create(spec.arg(0), provider));

   if(!bc)
      {
      return std::unique_ptr<Cipher_Mode>();
      }

#if defined(BOTAN_HAS_MODE_CBC)
   if(spec.algo_name() == "CBC")
      {
      const std::string padding = spec.arg(1, "PKCS7");

      if(padding == "CTS")
         {
         if(direction == ENCRYPTION)
            return std::unique_ptr<Cipher_Mode>(new CTS_Encryption(bc.release()));
         else
            return std::unique_ptr<Cipher_Mode>(new CTS_Decryption(bc.release()));
         }
      else
         {
         std::unique_ptr<BlockCipherModePaddingMethod> pad(get_bc_pad(padding));

         if(pad)
            {
            if(direction == ENCRYPTION)
               return std::unique_ptr<Cipher_Mode>(new CBC_Encryption(bc.release(), pad.release()));
            else
               return std::unique_ptr<Cipher_Mode>(new CBC_Decryption(bc.release(), pad.release()));
            }
         }
      }
#endif

#if defined(BOTAN_HAS_MODE_XTS)
   if(spec.algo_name() == "XTS")
      {
      if(direction == ENCRYPTION)
         return std::unique_ptr<Cipher_Mode>(new XTS_Encryption(bc.release()));
      else
         return std::unique_ptr<Cipher_Mode>(new XTS_Decryption(bc.release()));
      }
#endif

#if defined(BOTAN_HAS_MODE_CFB)
   if(spec.algo_name() == "CFB")
      {
      const size_t feedback_bits = spec.arg_as_integer(1, 8*bc->block_size());
      if(direction == ENCRYPTION)
         return std::unique_ptr<Cipher_Mode>(new CFB_Encryption(bc.release(), feedback_bits));
      else
         return std::unique_ptr<Cipher_Mode>(new CFB_Decryption(bc.release(), feedback_bits));
      }
#endif

#endif

   return std::unique_ptr<Cipher_Mode>();
   }

//static
std::vector<std::string> Cipher_Mode::providers(const std::string& algo_spec)
   {
   const std::vector<std::string>& possible = { "base", "openssl", "commoncrypto" };
   std::vector<std::string> providers;
   for(auto&& prov : possible)
      {
      std::unique_ptr<Cipher_Mode> mode = Cipher_Mode::create(algo_spec, ENCRYPTION, prov);
      if(mode)
         {
         providers.push_back(prov); // available
         }
      }
   return providers;
   }

}
/*
* PBKDF
* (C) 2012 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_PBKDF1)
#endif

#if defined(BOTAN_HAS_PBKDF2)
#endif

#if defined(BOTAN_HAS_PGP_S2K)
#endif

namespace Botan {

std::unique_ptr<PBKDF> PBKDF::create(const std::string& algo_spec,
                                     const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_PBKDF2)
   if(req.algo_name() == "PBKDF2")
      {
      // TODO OpenSSL

      if(provider.empty() || provider == "base")
         {
         if(auto mac = MessageAuthenticationCode::create(req.arg(0)))
            return std::unique_ptr<PBKDF>(new PKCS5_PBKDF2(mac.release()));

         if(auto mac = MessageAuthenticationCode::create("HMAC(" + req.arg(0) + ")"))
            return std::unique_ptr<PBKDF>(new PKCS5_PBKDF2(mac.release()));
         }

      return nullptr;
      }
#endif

#if defined(BOTAN_HAS_PBKDF1)
   if(req.algo_name() == "PBKDF1" && req.arg_count() == 1)
      {
      if(auto hash = HashFunction::create(req.arg(0)))
         return std::unique_ptr<PBKDF>(new PKCS5_PBKDF1(hash.release()));

      }
#endif

#if defined(BOTAN_HAS_PGP_S2K)
   if(req.algo_name() == "OpenPGP-S2K" && req.arg_count() == 1)
      {
      if(auto hash = HashFunction::create(req.arg(0)))
         return std::unique_ptr<PBKDF>(new OpenPGP_S2K(hash.release()));
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<PBKDF>
PBKDF::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto pbkdf = PBKDF::create(algo, provider))
      {
      return pbkdf;
      }
   throw Lookup_Error("PBKDF", algo, provider);
   }

std::vector<std::string> PBKDF::providers(const std::string& algo_spec)
   {
   return probe_providers_of<PBKDF>(algo_spec, { "base", "openssl" });
   }

void PBKDF::pbkdf_timed(uint8_t out[], size_t out_len,
                        const std::string& passphrase,
                        const uint8_t salt[], size_t salt_len,
                        std::chrono::milliseconds msec,
                        size_t& iterations) const
   {
   iterations = pbkdf(out, out_len, passphrase, salt, salt_len, 0, msec);
   }

void PBKDF::pbkdf_iterations(uint8_t out[], size_t out_len,
                             const std::string& passphrase,
                             const uint8_t salt[], size_t salt_len,
                             size_t iterations) const
   {
   if(iterations == 0)
      throw Invalid_Argument(name() + ": Invalid iteration count");

   const size_t iterations_run = pbkdf(out, out_len, passphrase,
                                       salt, salt_len, iterations,
                                       std::chrono::milliseconds(0));
   BOTAN_ASSERT_EQUAL(iterations, iterations_run, "Expected PBKDF iterations");
   }

secure_vector<uint8_t> PBKDF::pbkdf_iterations(size_t out_len,
                                            const std::string& passphrase,
                                            const uint8_t salt[], size_t salt_len,
                                            size_t iterations) const
   {
   secure_vector<uint8_t> out(out_len);
   pbkdf_iterations(out.data(), out_len, passphrase, salt, salt_len, iterations);
   return out;
   }

secure_vector<uint8_t> PBKDF::pbkdf_timed(size_t out_len,
                                       const std::string& passphrase,
                                       const uint8_t salt[], size_t salt_len,
                                       std::chrono::milliseconds msec,
                                       size_t& iterations) const
   {
   secure_vector<uint8_t> out(out_len);
   pbkdf_timed(out.data(), out_len, passphrase, salt, salt_len, msec, iterations);
   return out;
   }

}
/*
* (C) 2018 Ribose Inc
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_PBKDF2)
#endif

#if defined(BOTAN_HAS_PGP_S2K)
#endif

#if defined(BOTAN_HAS_SCRYPT)
#endif

#if defined(BOTAN_HAS_ARGON2)
#endif

#if defined(BOTAN_HAS_PBKDF_BCRYPT)
#endif

namespace Botan {

std::unique_ptr<PasswordHashFamily> PasswordHashFamily::create(const std::string& algo_spec,
                                     const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_PBKDF2)
   if(req.algo_name() == "PBKDF2")
      {
      // TODO OpenSSL

      if(provider.empty() || provider == "base")
         {
         if(auto mac = MessageAuthenticationCode::create(req.arg(0)))
            return std::unique_ptr<PasswordHashFamily>(new PBKDF2_Family(mac.release()));

         if(auto mac = MessageAuthenticationCode::create("HMAC(" + req.arg(0) + ")"))
            return std::unique_ptr<PasswordHashFamily>(new PBKDF2_Family(mac.release()));
         }

      return nullptr;
      }
#endif

#if defined(BOTAN_HAS_SCRYPT)
   if(req.algo_name() == "Scrypt")
      {
      return std::unique_ptr<PasswordHashFamily>(new Scrypt_Family);
      }
#endif

#if defined(BOTAN_HAS_ARGON2)
   if(req.algo_name() == "Argon2d")
      {
      return std::unique_ptr<PasswordHashFamily>(new Argon2_Family(0));
      }
   else if(req.algo_name() == "Argon2i")
      {
      return std::unique_ptr<PasswordHashFamily>(new Argon2_Family(1));
      }
   else if(req.algo_name() == "Argon2id")
      {
      return std::unique_ptr<PasswordHashFamily>(new Argon2_Family(2));
      }
#endif

#if defined(BOTAN_HAS_PBKDF_BCRYPT)
   if(req.algo_name() == "Bcrypt-PBKDF")
      {
      return std::unique_ptr<PasswordHashFamily>(new Bcrypt_PBKDF_Family);
      }
#endif

#if defined(BOTAN_HAS_PGP_S2K)
   if(req.algo_name() == "OpenPGP-S2K" && req.arg_count() == 1)
      {
      if(auto hash = HashFunction::create(req.arg(0)))
         {
         return std::unique_ptr<PasswordHashFamily>(new RFC4880_S2K_Family(hash.release()));
         }
      }
#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<PasswordHashFamily>
PasswordHashFamily::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto pbkdf = PasswordHashFamily::create(algo, provider))
      {
      return pbkdf;
      }
   throw Lookup_Error("PasswordHashFamily", algo, provider);
   }

std::vector<std::string> PasswordHashFamily::providers(const std::string& algo_spec)
   {
   return probe_providers_of<PasswordHashFamily>(algo_spec, { "base", "openssl" });
   }

}
/*
* Derived from poly1305-donna-64.h by Andrew Moon <liquidsun@gmail.com>
* in https://github.com/floodyberry/poly1305-donna
*
* (C) 2014 Andrew Moon
* (C) 2014 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

void poly1305_init(secure_vector<uint64_t>& X, const uint8_t key[32])
   {
   /* r &= 0xffffffc0ffffffc0ffffffc0fffffff */
   const uint64_t t0 = load_le<uint64_t>(key, 0);
   const uint64_t t1 = load_le<uint64_t>(key, 1);

   X[0] = ( t0                    ) & 0xffc0fffffff;
   X[1] = ((t0 >> 44) | (t1 << 20)) & 0xfffffc0ffff;
   X[2] = ((t1 >> 24)             ) & 0x00ffffffc0f;

   /* h = 0 */
   X[3] = 0;
   X[4] = 0;
   X[5] = 0;

   /* save pad for later */
   X[6] = load_le<uint64_t>(key, 2);
   X[7] = load_le<uint64_t>(key, 3);
   }

void poly1305_blocks(secure_vector<uint64_t>& X, const uint8_t *m, size_t blocks, bool is_final = false)
   {
#if !defined(BOTAN_TARGET_HAS_NATIVE_UINT128)
   typedef donna128 uint128_t;
#endif

   const uint64_t hibit = is_final ? 0 : (static_cast<uint64_t>(1) << 40); /* 1 << 128 */

   const uint64_t r0 = X[0];
   const uint64_t r1 = X[1];
   const uint64_t r2 = X[2];

   const uint64_t M44 = 0xFFFFFFFFFFF;
   const uint64_t M42 = 0x3FFFFFFFFFF;

   uint64_t h0 = X[3+0];
   uint64_t h1 = X[3+1];
   uint64_t h2 = X[3+2];

   const uint64_t s1 = r1 * 20;
   const uint64_t s2 = r2 * 20;

   for(size_t i = 0; i != blocks; ++i)
      {
      const uint64_t t0 = load_le<uint64_t>(m, 0);
      const uint64_t t1 = load_le<uint64_t>(m, 1);

      h0 += (( t0                    ) & M44);
      h1 += (((t0 >> 44) | (t1 << 20)) & M44);
      h2 += (((t1 >> 24)             ) & M42) | hibit;

      const uint128_t d0 = uint128_t(h0) * r0 + uint128_t(h1) * s2 + uint128_t(h2) * s1;
      const uint64_t c0 = carry_shift(d0, 44);

      const uint128_t d1 = uint128_t(h0) * r1 + uint128_t(h1) * r0 + uint128_t(h2) * s2 + c0;
      const uint64_t c1 = carry_shift(d1, 44);

      const uint128_t d2 = uint128_t(h0) * r2 + uint128_t(h1) * r1 + uint128_t(h2) * r0 + c1;
      const uint64_t c2 = carry_shift(d2, 42);

      h0 = d0 & M44;
      h1 = d1 & M44;
      h2 = d2 & M42;

      h0 += c2 * 5;
      h1 += carry_shift(h0, 44);
      h0 = h0 & M44;

      m += 16;
      }

   X[3+0] = h0;
   X[3+1] = h1;
   X[3+2] = h2;
   }

void poly1305_finish(secure_vector<uint64_t>& X, uint8_t mac[16])
   {
   const uint64_t M44 = 0xFFFFFFFFFFF;
   const uint64_t M42 = 0x3FFFFFFFFFF;

   /* fully carry h */
   uint64_t h0 = X[3+0];
   uint64_t h1 = X[3+1];
   uint64_t h2 = X[3+2];

   uint64_t c;
                c = (h1 >> 44); h1 &= M44;
   h2 += c;     c = (h2 >> 42); h2 &= M42;
   h0 += c * 5; c = (h0 >> 44); h0 &= M44;
   h1 += c;     c = (h1 >> 44); h1 &= M44;
   h2 += c;     c = (h2 >> 42); h2 &= M42;
   h0 += c * 5; c = (h0 >> 44); h0 &= M44;
   h1 += c;

   /* compute h + -p */
   uint64_t g0 = h0 + 5; c = (g0 >> 44); g0 &= M44;
   uint64_t g1 = h1 + c; c = (g1 >> 44); g1 &= M44;
   uint64_t g2 = h2 + c - (static_cast<uint64_t>(1) << 42);

   /* select h if h < p, or h + -p if h >= p */
   const auto c_mask = CT::Mask<uint64_t>::expand(c);
   h0 = c_mask.select(g0, h0);
   h1 = c_mask.select(g1, h1);
   h2 = c_mask.select(g2, h2);

   /* h = (h + pad) */
   const uint64_t t0 = X[6];
   const uint64_t t1 = X[7];

   h0 += (( t0                    ) & M44)    ; c = (h0 >> 44); h0 &= M44;
   h1 += (((t0 >> 44) | (t1 << 20)) & M44) + c; c = (h1 >> 44); h1 &= M44;
   h2 += (((t1 >> 24)             ) & M42) + c;                 h2 &= M42;

   /* mac = h % (2^128) */
   h0 = ((h0      ) | (h1 << 44));
   h1 = ((h1 >> 20) | (h2 << 24));

   store_le(mac, h0, h1);

   /* zero out the state */
   clear_mem(X.data(), X.size());
   }

}

void Poly1305::clear()
   {
   zap(m_poly);
   zap(m_buf);
   m_buf_pos = 0;
   }

void Poly1305::key_schedule(const uint8_t key[], size_t)
   {
   m_buf_pos = 0;
   m_buf.resize(16);
   m_poly.resize(8);

   poly1305_init(m_poly, key);
   }

void Poly1305::add_data(const uint8_t input[], size_t length)
   {
   verify_key_set(m_poly.size() == 8);

   if(m_buf_pos)
      {
      buffer_insert(m_buf, m_buf_pos, input, length);

      if(m_buf_pos + length >= m_buf.size())
         {
         poly1305_blocks(m_poly, m_buf.data(), 1);
         input += (m_buf.size() - m_buf_pos);
         length -= (m_buf.size() - m_buf_pos);
         m_buf_pos = 0;
         }
      }

   const size_t full_blocks = length / m_buf.size();
   const size_t remaining   = length % m_buf.size();

   if(full_blocks)
      poly1305_blocks(m_poly, input, full_blocks);

   buffer_insert(m_buf, m_buf_pos, input + full_blocks * m_buf.size(), remaining);
   m_buf_pos += remaining;
   }

void Poly1305::final_result(uint8_t out[])
   {
   verify_key_set(m_poly.size() == 8);

   if(m_buf_pos != 0)
      {
      m_buf[m_buf_pos] = 1;
      const size_t len = m_buf.size() - m_buf_pos - 1;
      if (len > 0)
         {
         clear_mem(&m_buf[m_buf_pos+1], len);
         }
      poly1305_blocks(m_poly, m_buf.data(), 1, true);
      }

   poly1305_finish(m_poly, out);

   m_poly.clear();
   m_buf_pos = 0;
   }

}
/*
* (C) 2017,2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

/*
* The minimum weight irreducible binary polynomial of size n
*
* See http://www.hpl.hp.com/techreports/98/HPL-98-135.pdf
*/
enum class MinWeightPolynomial : uint64_t {
   P64   = 0x1B,
   P128  = 0x87,
   P192  = 0x87,
   P256  = 0x425,
   P512  = 0x125,
   P1024 = 0x80043,
};

template<size_t LIMBS, MinWeightPolynomial P>
void poly_double(uint8_t out[], const uint8_t in[])
   {
   uint64_t W[LIMBS];
   load_be(W, in, LIMBS);

   const uint64_t POLY = static_cast<uint64_t>(P);

   const uint64_t carry = POLY * (W[0] >> 63);

   BOTAN_IF_CONSTEXPR(LIMBS > 0)
      {
      for(size_t i = 0; i != LIMBS - 1; ++i)
         W[i] = (W[i] << 1) ^ (W[i+1] >> 63);
      }

   W[LIMBS-1] = (W[LIMBS-1] << 1) ^ carry;

   copy_out_be(out, LIMBS*8, W);
   }

template<size_t LIMBS, MinWeightPolynomial P>
void poly_double_le(uint8_t out[], const uint8_t in[])
   {
   uint64_t W[LIMBS];
   load_le(W, in, LIMBS);

   const uint64_t POLY = static_cast<uint64_t>(P);

   const uint64_t carry = POLY * (W[LIMBS-1] >> 63);

   BOTAN_IF_CONSTEXPR(LIMBS > 0)
      {
      for(size_t i = 0; i != LIMBS - 1; ++i)
         W[LIMBS-1-i] = (W[LIMBS-1-i] << 1) ^ (W[LIMBS-2-i] >> 63);
      }

   W[0] = (W[0] << 1) ^ carry;

   copy_out_le(out, LIMBS*8, W);
   }

}

void poly_double_n(uint8_t out[], const uint8_t in[], size_t n)
   {
   switch(n)
      {
      case 8:
         return poly_double<1, MinWeightPolynomial::P64>(out, in);
      case 16:
         return poly_double<2, MinWeightPolynomial::P128>(out, in);
      case 24:
         return poly_double<3, MinWeightPolynomial::P192>(out, in);
      case 32:
         return poly_double<4, MinWeightPolynomial::P256>(out, in);
      case 64:
         return poly_double<8, MinWeightPolynomial::P512>(out, in);
      case 128:
         return poly_double<16, MinWeightPolynomial::P1024>(out, in);
      default:
         throw Invalid_Argument("Unsupported size for poly_double_n");
      }
   }

void poly_double_n_le(uint8_t out[], const uint8_t in[], size_t n)
   {
   switch(n)
      {
      case 8:
         return poly_double_le<1, MinWeightPolynomial::P64>(out, in);
      case 16:
         return poly_double_le<2, MinWeightPolynomial::P128>(out, in);
      case 24:
         return poly_double_le<3, MinWeightPolynomial::P192>(out, in);
      case 32:
         return poly_double_le<4, MinWeightPolynomial::P256>(out, in);
      case 64:
         return poly_double_le<8, MinWeightPolynomial::P512>(out, in);
      case 128:
         return poly_double_le<16, MinWeightPolynomial::P1024>(out, in);
      default:
         throw Invalid_Argument("Unsupported size for poly_double_n_le");
      }
   }

}
/*
* RIPEMD-160
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> RIPEMD_160::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new RIPEMD_160(*this));
   }

namespace {

/*
* RIPEMD-160 F1 Function
*/
template<size_t S>
inline void F1(uint32_t& A, uint32_t B, uint32_t& C, uint32_t D, uint32_t E,
               uint32_t M)
   {
   A += (B ^ C ^ D) + M;
   A  = rotl<S>(A) + E;
   C  = rotl<10>(C);
   }

/*
* RIPEMD-160 F2 Function
*/
template<size_t S>
inline void F2(uint32_t& A, uint32_t B, uint32_t& C, uint32_t D, uint32_t E,
               uint32_t M)
   {
   A += (D ^ (B & (C ^ D))) + M;
   A  = rotl<S>(A) + E;
   C  = rotl<10>(C);
   }

/*
* RIPEMD-160 F3 Function
*/
template<size_t S>
inline void F3(uint32_t& A, uint32_t B, uint32_t& C, uint32_t D, uint32_t E,
               uint32_t M)
   {
   A += (D ^ (B | ~C)) + M;
   A  = rotl<S>(A) + E;
   C  = rotl<10>(C);
   }

/*
* RIPEMD-160 F4 Function
*/
template<size_t S>
inline void F4(uint32_t& A, uint32_t B, uint32_t& C, uint32_t D, uint32_t E,
               uint32_t M)
   {
   A += (C ^ (D & (B ^ C))) + M;
   A  = rotl<S>(A) + E;
   C  = rotl<10>(C);
   }

/*
* RIPEMD-160 F5 Function
*/
template<size_t S>
inline void F5(uint32_t& A, uint32_t B, uint32_t& C, uint32_t D, uint32_t E,
               uint32_t M)
   {
   A += (B ^ (C | ~D)) + M;
   A  = rotl<S>(A) + E;
   C  = rotl<10>(C);
   }

}

/*
* RIPEMD-160 Compression Function
*/
void RIPEMD_160::compress_n(const uint8_t input[], size_t blocks)
   {
   const uint32_t MAGIC2 = 0x5A827999, MAGIC3 = 0x6ED9EBA1,
                  MAGIC4 = 0x8F1BBCDC, MAGIC5 = 0xA953FD4E,
                  MAGIC6 = 0x50A28BE6, MAGIC7 = 0x5C4DD124,
                  MAGIC8 = 0x6D703EF3, MAGIC9 = 0x7A6D76E9;

   for(size_t i = 0; i != blocks; ++i)
      {
      load_le(m_M.data(), input, m_M.size());

      uint32_t A1 = m_digest[0], A2 = A1,
               B1 = m_digest[1], B2 = B1,
               C1 = m_digest[2], C2 = C1,
               D1 = m_digest[3], D2 = D1,
               E1 = m_digest[4], E2 = E1;

      F1<11>(A1,B1,C1,D1,E1,m_M[ 0]       );  F5< 8>(A2,B2,C2,D2,E2,m_M[ 5]+MAGIC6);
      F1<14>(E1,A1,B1,C1,D1,m_M[ 1]       );  F5< 9>(E2,A2,B2,C2,D2,m_M[14]+MAGIC6);
      F1<15>(D1,E1,A1,B1,C1,m_M[ 2]       );  F5< 9>(D2,E2,A2,B2,C2,m_M[ 7]+MAGIC6);
      F1<12>(C1,D1,E1,A1,B1,m_M[ 3]       );  F5<11>(C2,D2,E2,A2,B2,m_M[ 0]+MAGIC6);
      F1< 5>(B1,C1,D1,E1,A1,m_M[ 4]       );  F5<13>(B2,C2,D2,E2,A2,m_M[ 9]+MAGIC6);
      F1< 8>(A1,B1,C1,D1,E1,m_M[ 5]       );  F5<15>(A2,B2,C2,D2,E2,m_M[ 2]+MAGIC6);
      F1< 7>(E1,A1,B1,C1,D1,m_M[ 6]       );  F5<15>(E2,A2,B2,C2,D2,m_M[11]+MAGIC6);
      F1< 9>(D1,E1,A1,B1,C1,m_M[ 7]       );  F5< 5>(D2,E2,A2,B2,C2,m_M[ 4]+MAGIC6);
      F1<11>(C1,D1,E1,A1,B1,m_M[ 8]       );  F5< 7>(C2,D2,E2,A2,B2,m_M[13]+MAGIC6);
      F1<13>(B1,C1,D1,E1,A1,m_M[ 9]       );  F5< 7>(B2,C2,D2,E2,A2,m_M[ 6]+MAGIC6);
      F1<14>(A1,B1,C1,D1,E1,m_M[10]       );  F5< 8>(A2,B2,C2,D2,E2,m_M[15]+MAGIC6);
      F1<15>(E1,A1,B1,C1,D1,m_M[11]       );  F5<11>(E2,A2,B2,C2,D2,m_M[ 8]+MAGIC6);
      F1< 6>(D1,E1,A1,B1,C1,m_M[12]       );  F5<14>(D2,E2,A2,B2,C2,m_M[ 1]+MAGIC6);
      F1< 7>(C1,D1,E1,A1,B1,m_M[13]       );  F5<14>(C2,D2,E2,A2,B2,m_M[10]+MAGIC6);
      F1< 9>(B1,C1,D1,E1,A1,m_M[14]       );  F5<12>(B2,C2,D2,E2,A2,m_M[ 3]+MAGIC6);
      F1< 8>(A1,B1,C1,D1,E1,m_M[15]       );  F5< 6>(A2,B2,C2,D2,E2,m_M[12]+MAGIC6);

      F2< 7>(E1,A1,B1,C1,D1,m_M[ 7]+MAGIC2);  F4< 9>(E2,A2,B2,C2,D2,m_M[ 6]+MAGIC7);
      F2< 6>(D1,E1,A1,B1,C1,m_M[ 4]+MAGIC2);  F4<13>(D2,E2,A2,B2,C2,m_M[11]+MAGIC7);
      F2< 8>(C1,D1,E1,A1,B1,m_M[13]+MAGIC2);  F4<15>(C2,D2,E2,A2,B2,m_M[ 3]+MAGIC7);
      F2<13>(B1,C1,D1,E1,A1,m_M[ 1]+MAGIC2);  F4< 7>(B2,C2,D2,E2,A2,m_M[ 7]+MAGIC7);
      F2<11>(A1,B1,C1,D1,E1,m_M[10]+MAGIC2);  F4<12>(A2,B2,C2,D2,E2,m_M[ 0]+MAGIC7);
      F2< 9>(E1,A1,B1,C1,D1,m_M[ 6]+MAGIC2);  F4< 8>(E2,A2,B2,C2,D2,m_M[13]+MAGIC7);
      F2< 7>(D1,E1,A1,B1,C1,m_M[15]+MAGIC2);  F4< 9>(D2,E2,A2,B2,C2,m_M[ 5]+MAGIC7);
      F2<15>(C1,D1,E1,A1,B1,m_M[ 3]+MAGIC2);  F4<11>(C2,D2,E2,A2,B2,m_M[10]+MAGIC7);
      F2< 7>(B1,C1,D1,E1,A1,m_M[12]+MAGIC2);  F4< 7>(B2,C2,D2,E2,A2,m_M[14]+MAGIC7);
      F2<12>(A1,B1,C1,D1,E1,m_M[ 0]+MAGIC2);  F4< 7>(A2,B2,C2,D2,E2,m_M[15]+MAGIC7);
      F2<15>(E1,A1,B1,C1,D1,m_M[ 9]+MAGIC2);  F4<12>(E2,A2,B2,C2,D2,m_M[ 8]+MAGIC7);
      F2< 9>(D1,E1,A1,B1,C1,m_M[ 5]+MAGIC2);  F4< 7>(D2,E2,A2,B2,C2,m_M[12]+MAGIC7);
      F2<11>(C1,D1,E1,A1,B1,m_M[ 2]+MAGIC2);  F4< 6>(C2,D2,E2,A2,B2,m_M[ 4]+MAGIC7);
      F2< 7>(B1,C1,D1,E1,A1,m_M[14]+MAGIC2);  F4<15>(B2,C2,D2,E2,A2,m_M[ 9]+MAGIC7);
      F2<13>(A1,B1,C1,D1,E1,m_M[11]+MAGIC2);  F4<13>(A2,B2,C2,D2,E2,m_M[ 1]+MAGIC7);
      F2<12>(E1,A1,B1,C1,D1,m_M[ 8]+MAGIC2);  F4<11>(E2,A2,B2,C2,D2,m_M[ 2]+MAGIC7);

      F3<11>(D1,E1,A1,B1,C1,m_M[ 3]+MAGIC3);  F3< 9>(D2,E2,A2,B2,C2,m_M[15]+MAGIC8);
      F3<13>(C1,D1,E1,A1,B1,m_M[10]+MAGIC3);  F3< 7>(C2,D2,E2,A2,B2,m_M[ 5]+MAGIC8);
      F3< 6>(B1,C1,D1,E1,A1,m_M[14]+MAGIC3);  F3<15>(B2,C2,D2,E2,A2,m_M[ 1]+MAGIC8);
      F3< 7>(A1,B1,C1,D1,E1,m_M[ 4]+MAGIC3);  F3<11>(A2,B2,C2,D2,E2,m_M[ 3]+MAGIC8);
      F3<14>(E1,A1,B1,C1,D1,m_M[ 9]+MAGIC3);  F3< 8>(E2,A2,B2,C2,D2,m_M[ 7]+MAGIC8);
      F3< 9>(D1,E1,A1,B1,C1,m_M[15]+MAGIC3);  F3< 6>(D2,E2,A2,B2,C2,m_M[14]+MAGIC8);
      F3<13>(C1,D1,E1,A1,B1,m_M[ 8]+MAGIC3);  F3< 6>(C2,D2,E2,A2,B2,m_M[ 6]+MAGIC8);
      F3<15>(B1,C1,D1,E1,A1,m_M[ 1]+MAGIC3);  F3<14>(B2,C2,D2,E2,A2,m_M[ 9]+MAGIC8);
      F3<14>(A1,B1,C1,D1,E1,m_M[ 2]+MAGIC3);  F3<12>(A2,B2,C2,D2,E2,m_M[11]+MAGIC8);
      F3< 8>(E1,A1,B1,C1,D1,m_M[ 7]+MAGIC3);  F3<13>(E2,A2,B2,C2,D2,m_M[ 8]+MAGIC8);
      F3<13>(D1,E1,A1,B1,C1,m_M[ 0]+MAGIC3);  F3< 5>(D2,E2,A2,B2,C2,m_M[12]+MAGIC8);
      F3< 6>(C1,D1,E1,A1,B1,m_M[ 6]+MAGIC3);  F3<14>(C2,D2,E2,A2,B2,m_M[ 2]+MAGIC8);
      F3< 5>(B1,C1,D1,E1,A1,m_M[13]+MAGIC3);  F3<13>(B2,C2,D2,E2,A2,m_M[10]+MAGIC8);
      F3<12>(A1,B1,C1,D1,E1,m_M[11]+MAGIC3);  F3<13>(A2,B2,C2,D2,E2,m_M[ 0]+MAGIC8);
      F3< 7>(E1,A1,B1,C1,D1,m_M[ 5]+MAGIC3);  F3< 7>(E2,A2,B2,C2,D2,m_M[ 4]+MAGIC8);
      F3< 5>(D1,E1,A1,B1,C1,m_M[12]+MAGIC3);  F3< 5>(D2,E2,A2,B2,C2,m_M[13]+MAGIC8);

      F4<11>(C1,D1,E1,A1,B1,m_M[ 1]+MAGIC4);  F2<15>(C2,D2,E2,A2,B2,m_M[ 8]+MAGIC9);
      F4<12>(B1,C1,D1,E1,A1,m_M[ 9]+MAGIC4);  F2< 5>(B2,C2,D2,E2,A2,m_M[ 6]+MAGIC9);
      F4<14>(A1,B1,C1,D1,E1,m_M[11]+MAGIC4);  F2< 8>(A2,B2,C2,D2,E2,m_M[ 4]+MAGIC9);
      F4<15>(E1,A1,B1,C1,D1,m_M[10]+MAGIC4);  F2<11>(E2,A2,B2,C2,D2,m_M[ 1]+MAGIC9);
      F4<14>(D1,E1,A1,B1,C1,m_M[ 0]+MAGIC4);  F2<14>(D2,E2,A2,B2,C2,m_M[ 3]+MAGIC9);
      F4<15>(C1,D1,E1,A1,B1,m_M[ 8]+MAGIC4);  F2<14>(C2,D2,E2,A2,B2,m_M[11]+MAGIC9);
      F4< 9>(B1,C1,D1,E1,A1,m_M[12]+MAGIC4);  F2< 6>(B2,C2,D2,E2,A2,m_M[15]+MAGIC9);
      F4< 8>(A1,B1,C1,D1,E1,m_M[ 4]+MAGIC4);  F2<14>(A2,B2,C2,D2,E2,m_M[ 0]+MAGIC9);
      F4< 9>(E1,A1,B1,C1,D1,m_M[13]+MAGIC4);  F2< 6>(E2,A2,B2,C2,D2,m_M[ 5]+MAGIC9);
      F4<14>(D1,E1,A1,B1,C1,m_M[ 3]+MAGIC4);  F2< 9>(D2,E2,A2,B2,C2,m_M[12]+MAGIC9);
      F4< 5>(C1,D1,E1,A1,B1,m_M[ 7]+MAGIC4);  F2<12>(C2,D2,E2,A2,B2,m_M[ 2]+MAGIC9);
      F4< 6>(B1,C1,D1,E1,A1,m_M[15]+MAGIC4);  F2< 9>(B2,C2,D2,E2,A2,m_M[13]+MAGIC9);
      F4< 8>(A1,B1,C1,D1,E1,m_M[14]+MAGIC4);  F2<12>(A2,B2,C2,D2,E2,m_M[ 9]+MAGIC9);
      F4< 6>(E1,A1,B1,C1,D1,m_M[ 5]+MAGIC4);  F2< 5>(E2,A2,B2,C2,D2,m_M[ 7]+MAGIC9);
      F4< 5>(D1,E1,A1,B1,C1,m_M[ 6]+MAGIC4);  F2<15>(D2,E2,A2,B2,C2,m_M[10]+MAGIC9);
      F4<12>(C1,D1,E1,A1,B1,m_M[ 2]+MAGIC4);  F2< 8>(C2,D2,E2,A2,B2,m_M[14]+MAGIC9);

      F5< 9>(B1,C1,D1,E1,A1,m_M[ 4]+MAGIC5);  F1< 8>(B2,C2,D2,E2,A2,m_M[12]       );
      F5<15>(A1,B1,C1,D1,E1,m_M[ 0]+MAGIC5);  F1< 5>(A2,B2,C2,D2,E2,m_M[15]       );
      F5< 5>(E1,A1,B1,C1,D1,m_M[ 5]+MAGIC5);  F1<12>(E2,A2,B2,C2,D2,m_M[10]       );
      F5<11>(D1,E1,A1,B1,C1,m_M[ 9]+MAGIC5);  F1< 9>(D2,E2,A2,B2,C2,m_M[ 4]       );
      F5< 6>(C1,D1,E1,A1,B1,m_M[ 7]+MAGIC5);  F1<12>(C2,D2,E2,A2,B2,m_M[ 1]       );
      F5< 8>(B1,C1,D1,E1,A1,m_M[12]+MAGIC5);  F1< 5>(B2,C2,D2,E2,A2,m_M[ 5]       );
      F5<13>(A1,B1,C1,D1,E1,m_M[ 2]+MAGIC5);  F1<14>(A2,B2,C2,D2,E2,m_M[ 8]       );
      F5<12>(E1,A1,B1,C1,D1,m_M[10]+MAGIC5);  F1< 6>(E2,A2,B2,C2,D2,m_M[ 7]       );
      F5< 5>(D1,E1,A1,B1,C1,m_M[14]+MAGIC5);  F1< 8>(D2,E2,A2,B2,C2,m_M[ 6]       );
      F5<12>(C1,D1,E1,A1,B1,m_M[ 1]+MAGIC5);  F1<13>(C2,D2,E2,A2,B2,m_M[ 2]       );
      F5<13>(B1,C1,D1,E1,A1,m_M[ 3]+MAGIC5);  F1< 6>(B2,C2,D2,E2,A2,m_M[13]       );
      F5<14>(A1,B1,C1,D1,E1,m_M[ 8]+MAGIC5);  F1< 5>(A2,B2,C2,D2,E2,m_M[14]       );
      F5<11>(E1,A1,B1,C1,D1,m_M[11]+MAGIC5);  F1<15>(E2,A2,B2,C2,D2,m_M[ 0]       );
      F5< 8>(D1,E1,A1,B1,C1,m_M[ 6]+MAGIC5);  F1<13>(D2,E2,A2,B2,C2,m_M[ 3]       );
      F5< 5>(C1,D1,E1,A1,B1,m_M[15]+MAGIC5);  F1<11>(C2,D2,E2,A2,B2,m_M[ 9]       );
      F5< 6>(B1,C1,D1,E1,A1,m_M[13]+MAGIC5);  F1<11>(B2,C2,D2,E2,A2,m_M[11]       );

      C1          = m_digest[1] + C1 + D2;
      m_digest[1] = m_digest[2] + D1 + E2;
      m_digest[2] = m_digest[3] + E1 + A2;
      m_digest[3] = m_digest[4] + A1 + B2;
      m_digest[4] = m_digest[0] + B1 + C2;
      m_digest[0] = C1;

      input += hash_block_size();
      }
   }

/*
* Copy out the digest
*/
void RIPEMD_160::copy_out(uint8_t output[])
   {
   copy_out_vec_le(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void RIPEMD_160::clear()
   {
   MDx_HashFunction::clear();
   zeroise(m_M);
   m_digest[0] = 0x67452301;
   m_digest[1] = 0xEFCDAB89;
   m_digest[2] = 0x98BADCFE;
   m_digest[3] = 0x10325476;
   m_digest[4] = 0xC3D2E1F0;
   }

}
/*
* (C) 2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_AUTO_SEEDING_RNG)
#endif

namespace Botan {

void RandomNumberGenerator::randomize_with_ts_input(uint8_t output[], size_t output_len)
   {
   if(this->accepts_input())
      {
      /*
      Form additional input which is provided to the PRNG implementation
      to paramaterize the KDF output.
      */
      uint8_t additional_input[16] = { 0 };
      store_le(OS::get_system_timestamp_ns(), additional_input);
      store_le(OS::get_high_resolution_clock(), additional_input + 8);

      this->randomize_with_input(output, output_len, additional_input, sizeof(additional_input));
      }
   else
      {
      this->randomize(output, output_len);
      }
   }

void RandomNumberGenerator::randomize_with_input(uint8_t output[], size_t output_len,
                                                 const uint8_t input[], size_t input_len)
   {
   this->add_entropy(input, input_len);
   this->randomize(output, output_len);
   }

size_t RandomNumberGenerator::reseed(Entropy_Sources& srcs,
                                     size_t poll_bits,
                                     std::chrono::milliseconds poll_timeout)
   {
   if(this->accepts_input())
      {
      return srcs.poll(*this, poll_bits, poll_timeout);
      }
   else
      {
      return 0;
      }
   }

void RandomNumberGenerator::reseed_from_rng(RandomNumberGenerator& rng, size_t poll_bits)
   {
   if(this->accepts_input())
      {
      secure_vector<uint8_t> buf(poll_bits / 8);
      rng.randomize(buf.data(), buf.size());
      this->add_entropy(buf.data(), buf.size());
      }
   }

RandomNumberGenerator* RandomNumberGenerator::make_rng()
   {
#if defined(BOTAN_HAS_AUTO_SEEDING_RNG)
   return new AutoSeeded_RNG;
#else
   throw Not_Implemented("make_rng failed, no AutoSeeded_RNG in this build");
#endif
   }

#if defined(BOTAN_TARGET_OS_HAS_THREADS)

#if defined(BOTAN_HAS_AUTO_SEEDING_RNG)
Serialized_RNG::Serialized_RNG() : m_rng(new AutoSeeded_RNG) {}
#else
Serialized_RNG::Serialized_RNG()
   {
   throw Not_Implemented("Serialized_RNG default constructor failed: AutoSeeded_RNG disabled in build");
   }
#endif

#endif

}
/*
* Serpent
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_SERPENT_SIMD) || defined(BOTAN_HAS_SERPENT_AVX2)
#endif

namespace Botan {

namespace {

/*
* Serpent's Linear Transform
*/
inline void transform(uint32_t& B0, uint32_t& B1, uint32_t& B2, uint32_t& B3)
   {
   B0  = rotl<13>(B0);   B2  = rotl<3>(B2);
   B1 ^= B0 ^ B2;        B3 ^= B2 ^ (B0 << 3);
   B1  = rotl<1>(B1);    B3  = rotl<7>(B3);
   B0 ^= B1 ^ B3;        B2 ^= B3 ^ (B1 << 7);
   B0  = rotl<5>(B0);    B2  = rotl<22>(B2);
   }

/*
* Serpent's Inverse Linear Transform
*/
inline void i_transform(uint32_t& B0, uint32_t& B1, uint32_t& B2, uint32_t& B3)
   {
   B2  = rotr<22>(B2);   B0  = rotr<5>(B0);
   B2 ^= B3 ^ (B1 << 7); B0 ^= B1 ^ B3;
   B3  = rotr<7>(B3);    B1  = rotr<1>(B1);
   B3 ^= B2 ^ (B0 << 3); B1 ^= B0 ^ B2;
   B2  = rotr<3>(B2);    B0  = rotr<13>(B0);
   }

}

/*
* XOR a key block with a data block
*/
#define key_xor(round, B0, B1, B2, B3) \
   B0 ^= m_round_key[4*round  ]; \
   B1 ^= m_round_key[4*round+1]; \
   B2 ^= m_round_key[4*round+2]; \
   B3 ^= m_round_key[4*round+3];

/*
* Serpent Encryption
*/
void Serpent::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_round_key.empty() == false);

#if defined(BOTAN_HAS_SERPENT_AVX2)
   if(CPUID::has_avx2())
      {
      while(blocks >= 8)
         {
         avx2_encrypt_8(in, out);
         in += 8 * BLOCK_SIZE;
         out += 8 * BLOCK_SIZE;
         blocks -= 8;
         }
      }
#endif

#if defined(BOTAN_HAS_SERPENT_SIMD)
   if(CPUID::has_simd_32())
      {
      while(blocks >= 4)
         {
         simd_encrypt_4(in, out);
         in += 4 * BLOCK_SIZE;
         out += 4 * BLOCK_SIZE;
         blocks -= 4;
         }
      }
#endif

   BOTAN_PARALLEL_SIMD_FOR(size_t i = 0; i < blocks; ++i)
      {
      uint32_t B0, B1, B2, B3;
      load_le(in + 16*i, B0, B1, B2, B3);

      key_xor( 0,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 1,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 2,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 3,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 4,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 5,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 6,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 7,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 8,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor( 9,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(10,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(11,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(12,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(13,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(14,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(15,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(16,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(17,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(18,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(19,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(20,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(21,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(22,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(23,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(24,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(25,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(26,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(27,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(28,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(29,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(30,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
      key_xor(31,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); key_xor(32,B0,B1,B2,B3);

      store_le(out + 16*i, B0, B1, B2, B3);
      }
   }

/*
* Serpent Decryption
*/
void Serpent::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_round_key.empty() == false);

#if defined(BOTAN_HAS_SERPENT_AVX2)
   if(CPUID::has_avx2())
      {
      while(blocks >= 8)
         {
         avx2_decrypt_8(in, out);
         in += 8 * BLOCK_SIZE;
         out += 8 * BLOCK_SIZE;
         blocks -= 8;
         }
      }
#endif

#if defined(BOTAN_HAS_SERPENT_SIMD)
   if(CPUID::has_simd_32())
      {
      while(blocks >= 4)
         {
         simd_decrypt_4(in, out);
         in += 4 * BLOCK_SIZE;
         out += 4 * BLOCK_SIZE;
         blocks -= 4;
         }
      }
#endif

   BOTAN_PARALLEL_SIMD_FOR(size_t i = 0; i < blocks; ++i)
      {
      uint32_t B0, B1, B2, B3;
      load_le(in + 16*i, B0, B1, B2, B3);

      key_xor(32,B0,B1,B2,B3);  SBoxD7(B0,B1,B2,B3); key_xor(31,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(30,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(29,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(28,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(27,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(26,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor(25,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor(24,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor(23,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(22,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(21,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(20,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(19,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(18,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor(17,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor(16,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor(15,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(14,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(13,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(12,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(11,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(10,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor( 9,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor( 8,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor( 7,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor( 6,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor( 5,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor( 4,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor( 3,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor( 2,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor( 1,B0,B1,B2,B3);
      i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor( 0,B0,B1,B2,B3);

      store_le(out + 16*i, B0, B1, B2, B3);
      }
   }

#undef key_xor
#undef transform
#undef i_transform

/*
* Serpent Key Schedule
*/
void Serpent::key_schedule(const uint8_t key[], size_t length)
   {
   const uint32_t PHI = 0x9E3779B9;

   secure_vector<uint32_t> W(140);
   for(size_t i = 0; i != length / 4; ++i)
      W[i] = load_le<uint32_t>(key, i);

   W[length / 4] |= uint32_t(1) << ((length%4)*8);

   for(size_t i = 8; i != 140; ++i)
      {
      uint32_t wi = W[i-8] ^ W[i-5] ^ W[i-3] ^ W[i-1] ^ PHI ^ uint32_t(i-8);
      W[i] = rotl<11>(wi);
      }

   SBoxE0(W[ 20],W[ 21],W[ 22],W[ 23]);
   SBoxE0(W[ 52],W[ 53],W[ 54],W[ 55]);
   SBoxE0(W[ 84],W[ 85],W[ 86],W[ 87]);
   SBoxE0(W[116],W[117],W[118],W[119]);

   SBoxE1(W[ 16],W[ 17],W[ 18],W[ 19]);
   SBoxE1(W[ 48],W[ 49],W[ 50],W[ 51]);
   SBoxE1(W[ 80],W[ 81],W[ 82],W[ 83]);
   SBoxE1(W[112],W[113],W[114],W[115]);

   SBoxE2(W[ 12],W[ 13],W[ 14],W[ 15]);
   SBoxE2(W[ 44],W[ 45],W[ 46],W[ 47]);
   SBoxE2(W[ 76],W[ 77],W[ 78],W[ 79]);
   SBoxE2(W[108],W[109],W[110],W[111]);

   SBoxE3(W[  8],W[  9],W[ 10],W[ 11]);
   SBoxE3(W[ 40],W[ 41],W[ 42],W[ 43]);
   SBoxE3(W[ 72],W[ 73],W[ 74],W[ 75]);
   SBoxE3(W[104],W[105],W[106],W[107]);
   SBoxE3(W[136],W[137],W[138],W[139]);

   SBoxE4(W[ 36],W[ 37],W[ 38],W[ 39]);
   SBoxE4(W[ 68],W[ 69],W[ 70],W[ 71]);
   SBoxE4(W[100],W[101],W[102],W[103]);
   SBoxE4(W[132],W[133],W[134],W[135]);

   SBoxE5(W[ 32],W[ 33],W[ 34],W[ 35]);
   SBoxE5(W[ 64],W[ 65],W[ 66],W[ 67]);
   SBoxE5(W[ 96],W[ 97],W[ 98],W[ 99]);
   SBoxE5(W[128],W[129],W[130],W[131]);

   SBoxE6(W[ 28],W[ 29],W[ 30],W[ 31]);
   SBoxE6(W[ 60],W[ 61],W[ 62],W[ 63]);
   SBoxE6(W[ 92],W[ 93],W[ 94],W[ 95]);
   SBoxE6(W[124],W[125],W[126],W[127]);

   SBoxE7(W[ 24],W[ 25],W[ 26],W[ 27]);
   SBoxE7(W[ 56],W[ 57],W[ 58],W[ 59]);
   SBoxE7(W[ 88],W[ 89],W[ 90],W[ 91]);
   SBoxE7(W[120],W[121],W[122],W[123]);

   m_round_key.assign(W.begin() + 8, W.end());
   }

void Serpent::clear()
   {
   zap(m_round_key);
   }

std::string Serpent::provider() const
   {
#if defined(BOTAN_HAS_SERPENT_AVX2)
   if(CPUID::has_avx2())
      {
      return "avx2";
      }
#endif

#if defined(BOTAN_HAS_SERPENT_SIMD)
   if(CPUID::has_simd_32())
      {
      return "simd";
      }
#endif

   return "base";
   }

#undef key_xor

}
/*
* Serpent (SIMD)
* (C) 2009,2013 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

#define key_xor(round, B0, B1, B2, B3)                             \
   do {                                                            \
      B0 ^= SIMD_4x32::splat(m_round_key[4*round  ]);              \
      B1 ^= SIMD_4x32::splat(m_round_key[4*round+1]);              \
      B2 ^= SIMD_4x32::splat(m_round_key[4*round+2]);              \
      B3 ^= SIMD_4x32::splat(m_round_key[4*round+3]);              \
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

/*
* SIMD Serpent Encryption of 4 blocks in parallel
*/
void Serpent::simd_encrypt_4(const uint8_t in[64], uint8_t out[64]) const
   {
   SIMD_4x32 B0 = SIMD_4x32::load_le(in);
   SIMD_4x32 B1 = SIMD_4x32::load_le(in + 16);
   SIMD_4x32 B2 = SIMD_4x32::load_le(in + 32);
   SIMD_4x32 B3 = SIMD_4x32::load_le(in + 48);

   SIMD_4x32::transpose(B0, B1, B2, B3);

   key_xor( 0,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 1,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 2,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 3,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 4,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 5,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 6,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 7,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);

   key_xor( 8,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor( 9,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(10,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(11,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(12,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(13,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(14,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(15,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);

   key_xor(16,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(17,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(18,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(19,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(20,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(21,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(22,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(23,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); transform(B0,B1,B2,B3);

   key_xor(24,B0,B1,B2,B3); SBoxE0(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(25,B0,B1,B2,B3); SBoxE1(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(26,B0,B1,B2,B3); SBoxE2(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(27,B0,B1,B2,B3); SBoxE3(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(28,B0,B1,B2,B3); SBoxE4(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(29,B0,B1,B2,B3); SBoxE5(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(30,B0,B1,B2,B3); SBoxE6(B0,B1,B2,B3); transform(B0,B1,B2,B3);
   key_xor(31,B0,B1,B2,B3); SBoxE7(B0,B1,B2,B3); key_xor(32,B0,B1,B2,B3);

   SIMD_4x32::transpose(B0, B1, B2, B3);

   B0.store_le(out);
   B1.store_le(out + 16);
   B2.store_le(out + 32);
   B3.store_le(out + 48);
   }

/*
* SIMD Serpent Decryption of 4 blocks in parallel
*/
void Serpent::simd_decrypt_4(const uint8_t in[64], uint8_t out[64]) const
   {
   SIMD_4x32 B0 = SIMD_4x32::load_le(in);
   SIMD_4x32 B1 = SIMD_4x32::load_le(in + 16);
   SIMD_4x32 B2 = SIMD_4x32::load_le(in + 32);
   SIMD_4x32 B3 = SIMD_4x32::load_le(in + 48);

   SIMD_4x32::transpose(B0, B1, B2, B3);

   key_xor(32,B0,B1,B2,B3);  SBoxD7(B0,B1,B2,B3); key_xor(31,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(30,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(29,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(28,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(27,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(26,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor(25,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor(24,B0,B1,B2,B3);

   i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor(23,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(22,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(21,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(20,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(19,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(18,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor(17,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor(16,B0,B1,B2,B3);

   i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor(15,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor(14,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor(13,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor(12,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor(11,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor(10,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor( 9,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor( 8,B0,B1,B2,B3);

   i_transform(B0,B1,B2,B3); SBoxD7(B0,B1,B2,B3); key_xor( 7,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD6(B0,B1,B2,B3); key_xor( 6,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD5(B0,B1,B2,B3); key_xor( 5,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD4(B0,B1,B2,B3); key_xor( 4,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD3(B0,B1,B2,B3); key_xor( 3,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD2(B0,B1,B2,B3); key_xor( 2,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD1(B0,B1,B2,B3); key_xor( 1,B0,B1,B2,B3);
   i_transform(B0,B1,B2,B3); SBoxD0(B0,B1,B2,B3); key_xor( 0,B0,B1,B2,B3);

   SIMD_4x32::transpose(B0, B1, B2, B3);

   B0.store_le(out);
   B1.store_le(out + 16);
   B2.store_le(out + 32);
   B3.store_le(out + 48);
   }

#undef key_xor
#undef transform
#undef i_transform

}
/*
* SHA-160
* (C) 1999-2008,2011 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> SHA_160::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_160(*this));
   }

namespace SHA1_F {

namespace {

/*
* SHA-160 F1 Function
*/
inline void F1(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += (D ^ (B & (C ^ D))) + msg + 0x5A827999 + rotl<5>(A);
   B  = rotl<30>(B);
   }

/*
* SHA-160 F2 Function
*/
inline void F2(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += (B ^ C ^ D) + msg + 0x6ED9EBA1 + rotl<5>(A);
   B  = rotl<30>(B);
   }

/*
* SHA-160 F3 Function
*/
inline void F3(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += ((B & C) | ((B | C) & D)) + msg + 0x8F1BBCDC + rotl<5>(A);
   B  = rotl<30>(B);
   }

/*
* SHA-160 F4 Function
*/
inline void F4(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += (B ^ C ^ D) + msg + 0xCA62C1D6 + rotl<5>(A);
   B  = rotl<30>(B);
   }

}

}

/*
* SHA-160 Compression Function
*/
void SHA_160::compress_n(const uint8_t input[], size_t blocks)
   {
   using namespace SHA1_F;

#if defined(BOTAN_HAS_SHA1_X86_SHA_NI)
   if(CPUID::has_intel_sha())
      {
      return sha1_compress_x86(m_digest, input, blocks);
      }
#endif

#if defined(BOTAN_HAS_SHA1_ARMV8)
   if(CPUID::has_arm_sha1())
      {
      return sha1_armv8_compress_n(m_digest, input, blocks);
      }
#endif

#if defined(BOTAN_HAS_SHA1_SSE2)
   if(CPUID::has_sse2())
      {
      return sse2_compress_n(m_digest, input, blocks);
      }

#endif

   uint32_t A = m_digest[0], B = m_digest[1], C = m_digest[2],
          D = m_digest[3], E = m_digest[4];

   m_W.resize(80);

   for(size_t i = 0; i != blocks; ++i)
      {
      load_be(m_W.data(), input, 16);

      for(size_t j = 16; j != 80; j += 8)
         {
         m_W[j  ] = rotl<1>(m_W[j-3] ^ m_W[j-8] ^ m_W[j-14] ^ m_W[j-16]);
         m_W[j+1] = rotl<1>(m_W[j-2] ^ m_W[j-7] ^ m_W[j-13] ^ m_W[j-15]);
         m_W[j+2] = rotl<1>(m_W[j-1] ^ m_W[j-6] ^ m_W[j-12] ^ m_W[j-14]);
         m_W[j+3] = rotl<1>(m_W[j  ] ^ m_W[j-5] ^ m_W[j-11] ^ m_W[j-13]);
         m_W[j+4] = rotl<1>(m_W[j+1] ^ m_W[j-4] ^ m_W[j-10] ^ m_W[j-12]);
         m_W[j+5] = rotl<1>(m_W[j+2] ^ m_W[j-3] ^ m_W[j- 9] ^ m_W[j-11]);
         m_W[j+6] = rotl<1>(m_W[j+3] ^ m_W[j-2] ^ m_W[j- 8] ^ m_W[j-10]);
         m_W[j+7] = rotl<1>(m_W[j+4] ^ m_W[j-1] ^ m_W[j- 7] ^ m_W[j- 9]);
         }

      F1(A, B, C, D, E, m_W[ 0]);   F1(E, A, B, C, D, m_W[ 1]);
      F1(D, E, A, B, C, m_W[ 2]);   F1(C, D, E, A, B, m_W[ 3]);
      F1(B, C, D, E, A, m_W[ 4]);   F1(A, B, C, D, E, m_W[ 5]);
      F1(E, A, B, C, D, m_W[ 6]);   F1(D, E, A, B, C, m_W[ 7]);
      F1(C, D, E, A, B, m_W[ 8]);   F1(B, C, D, E, A, m_W[ 9]);
      F1(A, B, C, D, E, m_W[10]);   F1(E, A, B, C, D, m_W[11]);
      F1(D, E, A, B, C, m_W[12]);   F1(C, D, E, A, B, m_W[13]);
      F1(B, C, D, E, A, m_W[14]);   F1(A, B, C, D, E, m_W[15]);
      F1(E, A, B, C, D, m_W[16]);   F1(D, E, A, B, C, m_W[17]);
      F1(C, D, E, A, B, m_W[18]);   F1(B, C, D, E, A, m_W[19]);

      F2(A, B, C, D, E, m_W[20]);   F2(E, A, B, C, D, m_W[21]);
      F2(D, E, A, B, C, m_W[22]);   F2(C, D, E, A, B, m_W[23]);
      F2(B, C, D, E, A, m_W[24]);   F2(A, B, C, D, E, m_W[25]);
      F2(E, A, B, C, D, m_W[26]);   F2(D, E, A, B, C, m_W[27]);
      F2(C, D, E, A, B, m_W[28]);   F2(B, C, D, E, A, m_W[29]);
      F2(A, B, C, D, E, m_W[30]);   F2(E, A, B, C, D, m_W[31]);
      F2(D, E, A, B, C, m_W[32]);   F2(C, D, E, A, B, m_W[33]);
      F2(B, C, D, E, A, m_W[34]);   F2(A, B, C, D, E, m_W[35]);
      F2(E, A, B, C, D, m_W[36]);   F2(D, E, A, B, C, m_W[37]);
      F2(C, D, E, A, B, m_W[38]);   F2(B, C, D, E, A, m_W[39]);

      F3(A, B, C, D, E, m_W[40]);   F3(E, A, B, C, D, m_W[41]);
      F3(D, E, A, B, C, m_W[42]);   F3(C, D, E, A, B, m_W[43]);
      F3(B, C, D, E, A, m_W[44]);   F3(A, B, C, D, E, m_W[45]);
      F3(E, A, B, C, D, m_W[46]);   F3(D, E, A, B, C, m_W[47]);
      F3(C, D, E, A, B, m_W[48]);   F3(B, C, D, E, A, m_W[49]);
      F3(A, B, C, D, E, m_W[50]);   F3(E, A, B, C, D, m_W[51]);
      F3(D, E, A, B, C, m_W[52]);   F3(C, D, E, A, B, m_W[53]);
      F3(B, C, D, E, A, m_W[54]);   F3(A, B, C, D, E, m_W[55]);
      F3(E, A, B, C, D, m_W[56]);   F3(D, E, A, B, C, m_W[57]);
      F3(C, D, E, A, B, m_W[58]);   F3(B, C, D, E, A, m_W[59]);

      F4(A, B, C, D, E, m_W[60]);   F4(E, A, B, C, D, m_W[61]);
      F4(D, E, A, B, C, m_W[62]);   F4(C, D, E, A, B, m_W[63]);
      F4(B, C, D, E, A, m_W[64]);   F4(A, B, C, D, E, m_W[65]);
      F4(E, A, B, C, D, m_W[66]);   F4(D, E, A, B, C, m_W[67]);
      F4(C, D, E, A, B, m_W[68]);   F4(B, C, D, E, A, m_W[69]);
      F4(A, B, C, D, E, m_W[70]);   F4(E, A, B, C, D, m_W[71]);
      F4(D, E, A, B, C, m_W[72]);   F4(C, D, E, A, B, m_W[73]);
      F4(B, C, D, E, A, m_W[74]);   F4(A, B, C, D, E, m_W[75]);
      F4(E, A, B, C, D, m_W[76]);   F4(D, E, A, B, C, m_W[77]);
      F4(C, D, E, A, B, m_W[78]);   F4(B, C, D, E, A, m_W[79]);

      A = (m_digest[0] += A);
      B = (m_digest[1] += B);
      C = (m_digest[2] += C);
      D = (m_digest[3] += D);
      E = (m_digest[4] += E);

      input += hash_block_size();
      }
   }

/*
* Copy out the digest
*/
void SHA_160::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void SHA_160::clear()
   {
   MDx_HashFunction::clear();
   zeroise(m_W);
   m_digest[0] = 0x67452301;
   m_digest[1] = 0xEFCDAB89;
   m_digest[2] = 0x98BADCFE;
   m_digest[3] = 0x10325476;
   m_digest[4] = 0xC3D2E1F0;
   }

}
/*
* SHA-1 using SSE2
* Based on public domain code by Dean Gaudet
*    (http://arctic.org/~dean/crypto/sha1.html)
* (C) 2009-2011 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <emmintrin.h>

namespace Botan {

namespace SHA1_SSE2_F {

namespace {

/*
* First 16 bytes just need byte swapping. Preparing just means
* adding in the round constants.
*/

#define prep00_15(P, W)                                      \
   do {                                                      \
      W = _mm_shufflehi_epi16(W, _MM_SHUFFLE(2, 3, 0, 1));   \
      W = _mm_shufflelo_epi16(W, _MM_SHUFFLE(2, 3, 0, 1));   \
      W = _mm_or_si128(_mm_slli_epi16(W, 8),                 \
                       _mm_srli_epi16(W, 8));                \
      P.u128 = _mm_add_epi32(W, K00_19);                     \
   } while(0)

/*
For each multiple of 4, t, we want to calculate this:

W[t+0] = rol(W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16], 1);
W[t+1] = rol(W[t-2] ^ W[t-7] ^ W[t-13] ^ W[t-15], 1);
W[t+2] = rol(W[t-1] ^ W[t-6] ^ W[t-12] ^ W[t-14], 1);
W[t+3] = rol(W[t]   ^ W[t-5] ^ W[t-11] ^ W[t-13], 1);

we'll actually calculate this:

W[t+0] = rol(W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16], 1);
W[t+1] = rol(W[t-2] ^ W[t-7] ^ W[t-13] ^ W[t-15], 1);
W[t+2] = rol(W[t-1] ^ W[t-6] ^ W[t-12] ^ W[t-14], 1);
W[t+3] = rol(  0    ^ W[t-5] ^ W[t-11] ^ W[t-13], 1);
W[t+3] ^= rol(W[t+0], 1);

the parameters are:

W0 = &W[t-16];
W1 = &W[t-12];
W2 = &W[t- 8];
W3 = &W[t- 4];

and on output:
prepared = W0 + K
W0 = W[t]..W[t+3]
*/

/* note that there is a step here where i want to do a rol by 1, which
* normally would look like this:
*
* r1 = psrld r0,$31
* r0 = pslld r0,$1
* r0 = por r0,r1
*
* but instead i do this:
*
* r1 = pcmpltd r0,zero
* r0 = paddd r0,r0
* r0 = psub r0,r1
*
* because pcmpltd and paddd are available in both MMX units on
* efficeon, pentium-m, and opteron but shifts are available in
* only one unit.
*/
#define prep(prep, XW0, XW1, XW2, XW3, K)                               \
   do {                                                                 \
      __m128i r0, r1, r2, r3;                                           \
                                                                        \
      /* load W[t-4] 16-byte aligned, and shift */                      \
      r3 = _mm_srli_si128((XW3), 4);                                    \
      r0 = (XW0);                                                       \
      /* get high 64-bits of XW0 into low 64-bits */                    \
      r1 = _mm_shuffle_epi32((XW0), _MM_SHUFFLE(1,0,3,2));              \
      /* load high 64-bits of r1 */                                     \
      r1 = _mm_unpacklo_epi64(r1, (XW1));                               \
      r2 = (XW2);                                                       \
                                                                        \
      r0 = _mm_xor_si128(r1, r0);                                       \
      r2 = _mm_xor_si128(r3, r2);                                       \
      r0 = _mm_xor_si128(r2, r0);                                       \
      /* unrotated W[t]..W[t+2] in r0 ... still need W[t+3] */          \
                                                                        \
      r2 = _mm_slli_si128(r0, 12);                                      \
      r1 = _mm_cmplt_epi32(r0, _mm_setzero_si128());                    \
      r0 = _mm_add_epi32(r0, r0);   /* shift left by 1 */               \
      r0 = _mm_sub_epi32(r0, r1);   /* r0 has W[t]..W[t+2] */           \
                                                                        \
      r3 = _mm_srli_epi32(r2, 30);                                      \
      r2 = _mm_slli_epi32(r2, 2);                                       \
                                                                        \
      r0 = _mm_xor_si128(r0, r3);                                       \
      r0 = _mm_xor_si128(r0, r2);   /* r0 now has W[t+3] */             \
                                                                        \
      (XW0) = r0;                                                       \
      (prep).u128 = _mm_add_epi32(r0, K);                               \
   } while(0)

/*
* SHA-160 F1 Function
*/
inline void F1(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += (D ^ (B & (C ^ D))) + msg + rotl<5>(A);
   B  = rotl<30>(B);
   }

/*
* SHA-160 F2 Function
*/
inline void F2(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += (B ^ C ^ D) + msg + rotl<5>(A);
   B  = rotl<30>(B);
   }

/*
* SHA-160 F3 Function
*/
inline void F3(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += ((B & C) | ((B | C) & D)) + msg + rotl<5>(A);
   B  = rotl<30>(B);
   }

/*
* SHA-160 F4 Function
*/
inline void F4(uint32_t A, uint32_t& B, uint32_t C, uint32_t D, uint32_t& E, uint32_t msg)
   {
   E += (B ^ C ^ D) + msg + rotl<5>(A);
   B  = rotl<30>(B);
   }

}

}

/*
* SHA-160 Compression Function using SSE for message expansion
*/
//static
BOTAN_FUNC_ISA("sse2")
void SHA_160::sse2_compress_n(secure_vector<uint32_t>& digest, const uint8_t input[], size_t blocks)
   {
   using namespace SHA1_SSE2_F;

   const __m128i K00_19 = _mm_set1_epi32(0x5A827999);
   const __m128i K20_39 = _mm_set1_epi32(0x6ED9EBA1);
   const __m128i K40_59 = _mm_set1_epi32(0x8F1BBCDC);
   const __m128i K60_79 = _mm_set1_epi32(0xCA62C1D6);

   uint32_t A = digest[0],
          B = digest[1],
          C = digest[2],
          D = digest[3],
          E = digest[4];

   const __m128i* input_mm = reinterpret_cast<const __m128i*>(input);

   for(size_t i = 0; i != blocks; ++i)
      {
      union v4si {
         uint32_t u32[4];
         __m128i u128;
         };

      v4si P0, P1, P2, P3;

      __m128i W0 = _mm_loadu_si128(&input_mm[0]);
      prep00_15(P0, W0);

      __m128i W1 = _mm_loadu_si128(&input_mm[1]);
      prep00_15(P1, W1);

      __m128i W2 = _mm_loadu_si128(&input_mm[2]);
      prep00_15(P2, W2);

      __m128i W3 = _mm_loadu_si128(&input_mm[3]);
      prep00_15(P3, W3);

      /*
      Using SSE4; slower on Core2 and Nehalem
      #define GET_P_32(P, i) _mm_extract_epi32(P.u128, i)

      Much slower on all tested platforms
      #define GET_P_32(P,i) _mm_cvtsi128_si32(_mm_srli_si128(P.u128, i*4))
      */

#define GET_P_32(P, i) P.u32[i]

      F1(A, B, C, D, E, GET_P_32(P0, 0));
      F1(E, A, B, C, D, GET_P_32(P0, 1));
      F1(D, E, A, B, C, GET_P_32(P0, 2));
      F1(C, D, E, A, B, GET_P_32(P0, 3));
      prep(P0, W0, W1, W2, W3, K00_19);

      F1(B, C, D, E, A, GET_P_32(P1, 0));
      F1(A, B, C, D, E, GET_P_32(P1, 1));
      F1(E, A, B, C, D, GET_P_32(P1, 2));
      F1(D, E, A, B, C, GET_P_32(P1, 3));
      prep(P1, W1, W2, W3, W0, K20_39);

      F1(C, D, E, A, B, GET_P_32(P2, 0));
      F1(B, C, D, E, A, GET_P_32(P2, 1));
      F1(A, B, C, D, E, GET_P_32(P2, 2));
      F1(E, A, B, C, D, GET_P_32(P2, 3));
      prep(P2, W2, W3, W0, W1, K20_39);

      F1(D, E, A, B, C, GET_P_32(P3, 0));
      F1(C, D, E, A, B, GET_P_32(P3, 1));
      F1(B, C, D, E, A, GET_P_32(P3, 2));
      F1(A, B, C, D, E, GET_P_32(P3, 3));
      prep(P3, W3, W0, W1, W2, K20_39);

      F1(E, A, B, C, D, GET_P_32(P0, 0));
      F1(D, E, A, B, C, GET_P_32(P0, 1));
      F1(C, D, E, A, B, GET_P_32(P0, 2));
      F1(B, C, D, E, A, GET_P_32(P0, 3));
      prep(P0, W0, W1, W2, W3, K20_39);

      F2(A, B, C, D, E, GET_P_32(P1, 0));
      F2(E, A, B, C, D, GET_P_32(P1, 1));
      F2(D, E, A, B, C, GET_P_32(P1, 2));
      F2(C, D, E, A, B, GET_P_32(P1, 3));
      prep(P1, W1, W2, W3, W0, K20_39);

      F2(B, C, D, E, A, GET_P_32(P2, 0));
      F2(A, B, C, D, E, GET_P_32(P2, 1));
      F2(E, A, B, C, D, GET_P_32(P2, 2));
      F2(D, E, A, B, C, GET_P_32(P2, 3));
      prep(P2, W2, W3, W0, W1, K40_59);

      F2(C, D, E, A, B, GET_P_32(P3, 0));
      F2(B, C, D, E, A, GET_P_32(P3, 1));
      F2(A, B, C, D, E, GET_P_32(P3, 2));
      F2(E, A, B, C, D, GET_P_32(P3, 3));
      prep(P3, W3, W0, W1, W2, K40_59);

      F2(D, E, A, B, C, GET_P_32(P0, 0));
      F2(C, D, E, A, B, GET_P_32(P0, 1));
      F2(B, C, D, E, A, GET_P_32(P0, 2));
      F2(A, B, C, D, E, GET_P_32(P0, 3));
      prep(P0, W0, W1, W2, W3, K40_59);

      F2(E, A, B, C, D, GET_P_32(P1, 0));
      F2(D, E, A, B, C, GET_P_32(P1, 1));
      F2(C, D, E, A, B, GET_P_32(P1, 2));
      F2(B, C, D, E, A, GET_P_32(P1, 3));
      prep(P1, W1, W2, W3, W0, K40_59);

      F3(A, B, C, D, E, GET_P_32(P2, 0));
      F3(E, A, B, C, D, GET_P_32(P2, 1));
      F3(D, E, A, B, C, GET_P_32(P2, 2));
      F3(C, D, E, A, B, GET_P_32(P2, 3));
      prep(P2, W2, W3, W0, W1, K40_59);

      F3(B, C, D, E, A, GET_P_32(P3, 0));
      F3(A, B, C, D, E, GET_P_32(P3, 1));
      F3(E, A, B, C, D, GET_P_32(P3, 2));
      F3(D, E, A, B, C, GET_P_32(P3, 3));
      prep(P3, W3, W0, W1, W2, K60_79);

      F3(C, D, E, A, B, GET_P_32(P0, 0));
      F3(B, C, D, E, A, GET_P_32(P0, 1));
      F3(A, B, C, D, E, GET_P_32(P0, 2));
      F3(E, A, B, C, D, GET_P_32(P0, 3));
      prep(P0, W0, W1, W2, W3, K60_79);

      F3(D, E, A, B, C, GET_P_32(P1, 0));
      F3(C, D, E, A, B, GET_P_32(P1, 1));
      F3(B, C, D, E, A, GET_P_32(P1, 2));
      F3(A, B, C, D, E, GET_P_32(P1, 3));
      prep(P1, W1, W2, W3, W0, K60_79);

      F3(E, A, B, C, D, GET_P_32(P2, 0));
      F3(D, E, A, B, C, GET_P_32(P2, 1));
      F3(C, D, E, A, B, GET_P_32(P2, 2));
      F3(B, C, D, E, A, GET_P_32(P2, 3));
      prep(P2, W2, W3, W0, W1, K60_79);

      F4(A, B, C, D, E, GET_P_32(P3, 0));
      F4(E, A, B, C, D, GET_P_32(P3, 1));
      F4(D, E, A, B, C, GET_P_32(P3, 2));
      F4(C, D, E, A, B, GET_P_32(P3, 3));
      prep(P3, W3, W0, W1, W2, K60_79);

      F4(B, C, D, E, A, GET_P_32(P0, 0));
      F4(A, B, C, D, E, GET_P_32(P0, 1));
      F4(E, A, B, C, D, GET_P_32(P0, 2));
      F4(D, E, A, B, C, GET_P_32(P0, 3));

      F4(C, D, E, A, B, GET_P_32(P1, 0));
      F4(B, C, D, E, A, GET_P_32(P1, 1));
      F4(A, B, C, D, E, GET_P_32(P1, 2));
      F4(E, A, B, C, D, GET_P_32(P1, 3));

      F4(D, E, A, B, C, GET_P_32(P2, 0));
      F4(C, D, E, A, B, GET_P_32(P2, 1));
      F4(B, C, D, E, A, GET_P_32(P2, 2));
      F4(A, B, C, D, E, GET_P_32(P2, 3));

      F4(E, A, B, C, D, GET_P_32(P3, 0));
      F4(D, E, A, B, C, GET_P_32(P3, 1));
      F4(C, D, E, A, B, GET_P_32(P3, 2));
      F4(B, C, D, E, A, GET_P_32(P3, 3));

      A = (digest[0] += A);
      B = (digest[1] += B);
      C = (digest[2] += C);
      D = (digest[3] += D);
      E = (digest[4] += E);

      input_mm += (64 / 16);
      }

#undef GET_P_32
   }

#undef prep00_15
#undef prep

}
/*
* SHA-{224,256}
* (C) 1999-2010,2017 Jack Lloyd
*     2007 FlexSecure GmbH
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

std::string sha256_provider()
   {
#if defined(BOTAN_HAS_SHA2_32_X86)
   if(CPUID::has_intel_sha())
      {
      return "shani";
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_X86_BMI2)
   if(CPUID::has_bmi2())
      {
      return "bmi2";
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_ARMV8)
   if(CPUID::has_arm_sha2())
      {
      return "armv8";
      }
#endif

   return "base";
   }

}

std::unique_ptr<HashFunction> SHA_224::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_224(*this));
   }

std::unique_ptr<HashFunction> SHA_256::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_256(*this));
   }

/*
* SHA-256 F1 Function
*
* Use a macro as many compilers won't inline a function this big,
* even though it is much faster if inlined.
*/
#define SHA2_32_F(A, B, C, D, E, F, G, H, M1, M2, M3, M4, magic) do {               \
   uint32_t A_rho = rotr<2>(A) ^ rotr<13>(A) ^ rotr<22>(A); \
   uint32_t E_rho = rotr<6>(E) ^ rotr<11>(E) ^ rotr<25>(E); \
   uint32_t M2_sigma = rotr<17>(M2) ^ rotr<19>(M2) ^ (M2 >> 10);    \
   uint32_t M4_sigma = rotr<7>(M4) ^ rotr<18>(M4) ^ (M4 >> 3);      \
   H += magic + E_rho + ((E & F) ^ (~E & G)) + M1;                                  \
   D += H;                                                                          \
   H += A_rho + ((A & B) | ((A | B) & C));                                          \
   M1 += M2_sigma + M3 + M4_sigma;                                                  \
   } while(0);

/*
* SHA-224 / SHA-256 compression function
*/
void SHA_256::compress_digest(secure_vector<uint32_t>& digest,
                              const uint8_t input[], size_t blocks)
   {
#if defined(BOTAN_HAS_SHA2_32_X86)
   if(CPUID::has_intel_sha())
      {
      return SHA_256::compress_digest_x86(digest, input, blocks);
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_X86_BMI2)
   if(CPUID::has_bmi2())
      {
      return SHA_256::compress_digest_x86_bmi2(digest, input, blocks);
      }
#endif

#if defined(BOTAN_HAS_SHA2_32_ARMV8)
   if(CPUID::has_arm_sha2())
      {
      return SHA_256::compress_digest_armv8(digest, input, blocks);
      }
#endif

   uint32_t A = digest[0], B = digest[1], C = digest[2],
            D = digest[3], E = digest[4], F = digest[5],
            G = digest[6], H = digest[7];

   for(size_t i = 0; i != blocks; ++i)
      {
      uint32_t W00 = load_be<uint32_t>(input,  0);
      uint32_t W01 = load_be<uint32_t>(input,  1);
      uint32_t W02 = load_be<uint32_t>(input,  2);
      uint32_t W03 = load_be<uint32_t>(input,  3);
      uint32_t W04 = load_be<uint32_t>(input,  4);
      uint32_t W05 = load_be<uint32_t>(input,  5);
      uint32_t W06 = load_be<uint32_t>(input,  6);
      uint32_t W07 = load_be<uint32_t>(input,  7);
      uint32_t W08 = load_be<uint32_t>(input,  8);
      uint32_t W09 = load_be<uint32_t>(input,  9);
      uint32_t W10 = load_be<uint32_t>(input, 10);
      uint32_t W11 = load_be<uint32_t>(input, 11);
      uint32_t W12 = load_be<uint32_t>(input, 12);
      uint32_t W13 = load_be<uint32_t>(input, 13);
      uint32_t W14 = load_be<uint32_t>(input, 14);
      uint32_t W15 = load_be<uint32_t>(input, 15);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x428A2F98);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x71374491);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0xB5C0FBCF);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0xE9B5DBA5);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x3956C25B);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x59F111F1);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x923F82A4);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0xAB1C5ED5);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0xD807AA98);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0x12835B01);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0x243185BE);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0x550C7DC3);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0x72BE5D74);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0x80DEB1FE);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0x9BDC06A7);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0xC19BF174);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0xE49B69C1);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0xEFBE4786);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x0FC19DC6);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x240CA1CC);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x2DE92C6F);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x4A7484AA);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x5CB0A9DC);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x76F988DA);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0x983E5152);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0xA831C66D);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0xB00327C8);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0xBF597FC7);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0xC6E00BF3);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xD5A79147);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0x06CA6351);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0x14292967);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x27B70A85);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x2E1B2138);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x4D2C6DFC);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x53380D13);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x650A7354);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x766A0ABB);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x81C2C92E);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x92722C85);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0xA2BFE8A1);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0xA81A664B);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0xC24B8B70);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0xC76C51A3);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0xD192E819);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xD6990624);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0xF40E3585);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0x106AA070);

      SHA2_32_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x19A4C116);
      SHA2_32_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x1E376C08);
      SHA2_32_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x2748774C);
      SHA2_32_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x34B0BCB5);
      SHA2_32_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x391C0CB3);
      SHA2_32_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x4ED8AA4A);
      SHA2_32_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x5B9CCA4F);
      SHA2_32_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x682E6FF3);
      SHA2_32_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0x748F82EE);
      SHA2_32_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0x78A5636F);
      SHA2_32_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0x84C87814);
      SHA2_32_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0x8CC70208);
      SHA2_32_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0x90BEFFFA);
      SHA2_32_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xA4506CEB);
      SHA2_32_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0xBEF9A3F7);
      SHA2_32_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0xC67178F2);

      A = (digest[0] += A);
      B = (digest[1] += B);
      C = (digest[2] += C);
      D = (digest[3] += D);
      E = (digest[4] += E);
      F = (digest[5] += F);
      G = (digest[6] += G);
      H = (digest[7] += H);

      input += 64;
      }
   }

std::string SHA_224::provider() const
   {
   return sha256_provider();
   }

std::string SHA_256::provider() const
   {
   return sha256_provider();
   }

/*
* SHA-224 compression function
*/
void SHA_224::compress_n(const uint8_t input[], size_t blocks)
   {
   SHA_256::compress_digest(m_digest, input, blocks);
   }

/*
* Copy out the digest
*/
void SHA_224::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void SHA_224::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0xC1059ED8;
   m_digest[1] = 0x367CD507;
   m_digest[2] = 0x3070DD17;
   m_digest[3] = 0xF70E5939;
   m_digest[4] = 0xFFC00B31;
   m_digest[5] = 0x68581511;
   m_digest[6] = 0x64F98FA7;
   m_digest[7] = 0xBEFA4FA4;
   }

/*
* SHA-256 compression function
*/
void SHA_256::compress_n(const uint8_t input[], size_t blocks)
   {
   SHA_256::compress_digest(m_digest, input, blocks);
   }

/*
* Copy out the digest
*/
void SHA_256::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void SHA_256::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0x6A09E667;
   m_digest[1] = 0xBB67AE85;
   m_digest[2] = 0x3C6EF372;
   m_digest[3] = 0xA54FF53A;
   m_digest[4] = 0x510E527F;
   m_digest[5] = 0x9B05688C;
   m_digest[6] = 0x1F83D9AB;
   m_digest[7] = 0x5BE0CD19;
   }

}
/*
* SHA-{384,512}
* (C) 1999-2011,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace {

std::string sha512_provider()
   {
#if defined(BOTAN_HAS_SHA2_64_BMI2)
   if(CPUID::has_bmi2())
      {
      return "bmi2";
      }
#endif

   return "base";
   }

}

std::unique_ptr<HashFunction> SHA_384::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_384(*this));
   }

std::unique_ptr<HashFunction> SHA_512::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_512(*this));
   }

std::unique_ptr<HashFunction> SHA_512_256::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_512_256(*this));
   }

/*
* SHA-512 F1 Function
*
* Use a macro as many compilers won't inline a function this big,
* even though it is much faster if inlined.
*/
#define SHA2_64_F(A, B, C, D, E, F, G, H, M1, M2, M3, M4, magic)         \
   do {                                                                  \
      const uint64_t E_rho = rotr<14>(E) ^ rotr<18>(E) ^ rotr<41>(E);    \
      const uint64_t A_rho = rotr<28>(A) ^ rotr<34>(A) ^ rotr<39>(A);    \
      const uint64_t M2_sigma = rotr<19>(M2) ^ rotr<61>(M2) ^ (M2 >> 6); \
      const uint64_t M4_sigma = rotr<1>(M4) ^ rotr<8>(M4) ^ (M4 >> 7);   \
      H += magic + E_rho + ((E & F) ^ (~E & G)) + M1;                    \
      D += H;                                                            \
      H += A_rho + ((A & B) | ((A | B) & C));                            \
      M1 += M2_sigma + M3 + M4_sigma;                                    \
   } while(0);

/*
* SHA-{384,512} Compression Function
*/
//static
void SHA_512::compress_digest(secure_vector<uint64_t>& digest,
                              const uint8_t input[], size_t blocks)
   {
#if defined(BOTAN_HAS_SHA2_64_BMI2)
   if(CPUID::has_bmi2())
      {
      return compress_digest_bmi2(digest, input, blocks);
      }
#endif

   uint64_t A = digest[0], B = digest[1], C = digest[2],
            D = digest[3], E = digest[4], F = digest[5],
            G = digest[6], H = digest[7];

   for(size_t i = 0; i != blocks; ++i)
      {
      uint64_t W00 = load_be<uint64_t>(input,  0);
      uint64_t W01 = load_be<uint64_t>(input,  1);
      uint64_t W02 = load_be<uint64_t>(input,  2);
      uint64_t W03 = load_be<uint64_t>(input,  3);
      uint64_t W04 = load_be<uint64_t>(input,  4);
      uint64_t W05 = load_be<uint64_t>(input,  5);
      uint64_t W06 = load_be<uint64_t>(input,  6);
      uint64_t W07 = load_be<uint64_t>(input,  7);
      uint64_t W08 = load_be<uint64_t>(input,  8);
      uint64_t W09 = load_be<uint64_t>(input,  9);
      uint64_t W10 = load_be<uint64_t>(input, 10);
      uint64_t W11 = load_be<uint64_t>(input, 11);
      uint64_t W12 = load_be<uint64_t>(input, 12);
      uint64_t W13 = load_be<uint64_t>(input, 13);
      uint64_t W14 = load_be<uint64_t>(input, 14);
      uint64_t W15 = load_be<uint64_t>(input, 15);

      SHA2_64_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x428A2F98D728AE22);
      SHA2_64_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x7137449123EF65CD);
      SHA2_64_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0xB5C0FBCFEC4D3B2F);
      SHA2_64_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0xE9B5DBA58189DBBC);
      SHA2_64_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x3956C25BF348B538);
      SHA2_64_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x59F111F1B605D019);
      SHA2_64_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x923F82A4AF194F9B);
      SHA2_64_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0xAB1C5ED5DA6D8118);
      SHA2_64_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0xD807AA98A3030242);
      SHA2_64_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0x12835B0145706FBE);
      SHA2_64_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0x243185BE4EE4B28C);
      SHA2_64_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0x550C7DC3D5FFB4E2);
      SHA2_64_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0x72BE5D74F27B896F);
      SHA2_64_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0x80DEB1FE3B1696B1);
      SHA2_64_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0x9BDC06A725C71235);
      SHA2_64_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0xC19BF174CF692694);
      SHA2_64_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0xE49B69C19EF14AD2);
      SHA2_64_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0xEFBE4786384F25E3);
      SHA2_64_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x0FC19DC68B8CD5B5);
      SHA2_64_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x240CA1CC77AC9C65);
      SHA2_64_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x2DE92C6F592B0275);
      SHA2_64_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x4A7484AA6EA6E483);
      SHA2_64_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x5CB0A9DCBD41FBD4);
      SHA2_64_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x76F988DA831153B5);
      SHA2_64_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0x983E5152EE66DFAB);
      SHA2_64_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0xA831C66D2DB43210);
      SHA2_64_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0xB00327C898FB213F);
      SHA2_64_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0xBF597FC7BEEF0EE4);
      SHA2_64_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0xC6E00BF33DA88FC2);
      SHA2_64_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xD5A79147930AA725);
      SHA2_64_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0x06CA6351E003826F);
      SHA2_64_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0x142929670A0E6E70);
      SHA2_64_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x27B70A8546D22FFC);
      SHA2_64_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x2E1B21385C26C926);
      SHA2_64_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x4D2C6DFC5AC42AED);
      SHA2_64_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x53380D139D95B3DF);
      SHA2_64_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x650A73548BAF63DE);
      SHA2_64_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x766A0ABB3C77B2A8);
      SHA2_64_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x81C2C92E47EDAEE6);
      SHA2_64_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x92722C851482353B);
      SHA2_64_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0xA2BFE8A14CF10364);
      SHA2_64_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0xA81A664BBC423001);
      SHA2_64_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0xC24B8B70D0F89791);
      SHA2_64_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0xC76C51A30654BE30);
      SHA2_64_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0xD192E819D6EF5218);
      SHA2_64_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xD69906245565A910);
      SHA2_64_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0xF40E35855771202A);
      SHA2_64_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0x106AA07032BBD1B8);
      SHA2_64_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0x19A4C116B8D2D0C8);
      SHA2_64_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0x1E376C085141AB53);
      SHA2_64_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0x2748774CDF8EEB99);
      SHA2_64_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0x34B0BCB5E19B48A8);
      SHA2_64_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x391C0CB3C5C95A63);
      SHA2_64_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x4ED8AA4AE3418ACB);
      SHA2_64_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x5B9CCA4F7763E373);
      SHA2_64_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x682E6FF3D6B2B8A3);
      SHA2_64_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0x748F82EE5DEFB2FC);
      SHA2_64_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0x78A5636F43172F60);
      SHA2_64_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0x84C87814A1F0AB72);
      SHA2_64_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0x8CC702081A6439EC);
      SHA2_64_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0x90BEFFFA23631E28);
      SHA2_64_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0xA4506CEBDE82BDE9);
      SHA2_64_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0xBEF9A3F7B2C67915);
      SHA2_64_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0xC67178F2E372532B);
      SHA2_64_F(A, B, C, D, E, F, G, H, W00, W14, W09, W01, 0xCA273ECEEA26619C);
      SHA2_64_F(H, A, B, C, D, E, F, G, W01, W15, W10, W02, 0xD186B8C721C0C207);
      SHA2_64_F(G, H, A, B, C, D, E, F, W02, W00, W11, W03, 0xEADA7DD6CDE0EB1E);
      SHA2_64_F(F, G, H, A, B, C, D, E, W03, W01, W12, W04, 0xF57D4F7FEE6ED178);
      SHA2_64_F(E, F, G, H, A, B, C, D, W04, W02, W13, W05, 0x06F067AA72176FBA);
      SHA2_64_F(D, E, F, G, H, A, B, C, W05, W03, W14, W06, 0x0A637DC5A2C898A6);
      SHA2_64_F(C, D, E, F, G, H, A, B, W06, W04, W15, W07, 0x113F9804BEF90DAE);
      SHA2_64_F(B, C, D, E, F, G, H, A, W07, W05, W00, W08, 0x1B710B35131C471B);
      SHA2_64_F(A, B, C, D, E, F, G, H, W08, W06, W01, W09, 0x28DB77F523047D84);
      SHA2_64_F(H, A, B, C, D, E, F, G, W09, W07, W02, W10, 0x32CAAB7B40C72493);
      SHA2_64_F(G, H, A, B, C, D, E, F, W10, W08, W03, W11, 0x3C9EBE0A15C9BEBC);
      SHA2_64_F(F, G, H, A, B, C, D, E, W11, W09, W04, W12, 0x431D67C49C100D4C);
      SHA2_64_F(E, F, G, H, A, B, C, D, W12, W10, W05, W13, 0x4CC5D4BECB3E42B6);
      SHA2_64_F(D, E, F, G, H, A, B, C, W13, W11, W06, W14, 0x597F299CFC657E2A);
      SHA2_64_F(C, D, E, F, G, H, A, B, W14, W12, W07, W15, 0x5FCB6FAB3AD6FAEC);
      SHA2_64_F(B, C, D, E, F, G, H, A, W15, W13, W08, W00, 0x6C44198C4A475817);

      A = (digest[0] += A);
      B = (digest[1] += B);
      C = (digest[2] += C);
      D = (digest[3] += D);
      E = (digest[4] += E);
      F = (digest[5] += F);
      G = (digest[6] += G);
      H = (digest[7] += H);

      input += 128;
      }
   }

#undef SHA2_64_F

std::string SHA_512_256::provider() const
   {
   return sha512_provider();
   }

std::string SHA_384::provider() const
   {
   return sha512_provider();
   }

std::string SHA_512::provider() const
   {
   return sha512_provider();
   }

void SHA_512_256::compress_n(const uint8_t input[], size_t blocks)
   {
   SHA_512::compress_digest(m_digest, input, blocks);
   }

void SHA_384::compress_n(const uint8_t input[], size_t blocks)
   {
   SHA_512::compress_digest(m_digest, input, blocks);
   }

void SHA_512::compress_n(const uint8_t input[], size_t blocks)
   {
   SHA_512::compress_digest(m_digest, input, blocks);
   }

void SHA_512_256::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

void SHA_384::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

void SHA_512::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

void SHA_512_256::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0x22312194FC2BF72C;
   m_digest[1] = 0x9F555FA3C84C64C2;
   m_digest[2] = 0x2393B86B6F53B151;
   m_digest[3] = 0x963877195940EABD;
   m_digest[4] = 0x96283EE2A88EFFE3;
   m_digest[5] = 0xBE5E1E2553863992;
   m_digest[6] = 0x2B0199FC2C85B8AA;
   m_digest[7] = 0x0EB72DDC81C52CA2;
   }

void SHA_384::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0xCBBB9D5DC1059ED8;
   m_digest[1] = 0x629A292A367CD507;
   m_digest[2] = 0x9159015A3070DD17;
   m_digest[3] = 0x152FECD8F70E5939;
   m_digest[4] = 0x67332667FFC00B31;
   m_digest[5] = 0x8EB44A8768581511;
   m_digest[6] = 0xDB0C2E0D64F98FA7;
   m_digest[7] = 0x47B5481DBEFA4FA4;
   }

void SHA_512::clear()
   {
   MDx_HashFunction::clear();
   m_digest[0] = 0x6A09E667F3BCC908;
   m_digest[1] = 0xBB67AE8584CAA73B;
   m_digest[2] = 0x3C6EF372FE94F82B;
   m_digest[3] = 0xA54FF53A5F1D36F1;
   m_digest[4] = 0x510E527FADE682D1;
   m_digest[5] = 0x9B05688C2B3E6C1F;
   m_digest[6] = 0x1F83D9ABFB41BD6B;
   m_digest[7] = 0x5BE0CD19137E2179;
   }

}
/*
* SHA-3
* (C) 2010,2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void SHA_3::permute(uint64_t A[25])
   {
#if defined(BOTAN_HAS_SHA3_BMI2)
   if(CPUID::has_bmi2())
      {
      return permute_bmi2(A);
      }
#endif

   static const uint64_t RC[24] = {
      0x0000000000000001, 0x0000000000008082, 0x800000000000808A,
      0x8000000080008000, 0x000000000000808B, 0x0000000080000001,
      0x8000000080008081, 0x8000000000008009, 0x000000000000008A,
      0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
      0x000000008000808B, 0x800000000000008B, 0x8000000000008089,
      0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
      0x000000000000800A, 0x800000008000000A, 0x8000000080008081,
      0x8000000000008080, 0x0000000080000001, 0x8000000080008008
   };

   uint64_t T[25];

   for(size_t i = 0; i != 24; i += 2)
      {
      SHA3_round(T, A, RC[i+0]);
      SHA3_round(A, T, RC[i+1]);
      }
   }

//static
size_t SHA_3::absorb(size_t bitrate,
                     secure_vector<uint64_t>& S, size_t S_pos,
                     const uint8_t input[], size_t length)
   {
   while(length > 0)
      {
      size_t to_take = std::min(length, bitrate / 8 - S_pos);

      length -= to_take;

      while(to_take && S_pos % 8)
         {
         S[S_pos / 8] ^= static_cast<uint64_t>(input[0]) << (8 * (S_pos % 8));

         ++S_pos;
         ++input;
         --to_take;
         }

      while(to_take && to_take % 8 == 0)
         {
         S[S_pos / 8] ^= load_le<uint64_t>(input, 0);
         S_pos += 8;
         input += 8;
         to_take -= 8;
         }

      while(to_take)
         {
         S[S_pos / 8] ^= static_cast<uint64_t>(input[0]) << (8 * (S_pos % 8));

         ++S_pos;
         ++input;
         --to_take;
         }

      if(S_pos == bitrate / 8)
         {
         SHA_3::permute(S.data());
         S_pos = 0;
         }
      }

   return S_pos;
   }

//static
void SHA_3::finish(size_t bitrate,
                   secure_vector<uint64_t>& S, size_t S_pos,
                   uint8_t init_pad, uint8_t fini_pad)
   {
   BOTAN_ARG_CHECK(bitrate % 64 == 0, "SHA-3 bitrate must be multiple of 64");

   S[S_pos / 8] ^= static_cast<uint64_t>(init_pad) << (8 * (S_pos % 8));
   S[(bitrate / 64) - 1] ^= static_cast<uint64_t>(fini_pad) << 56;
   SHA_3::permute(S.data());
   }

//static
void SHA_3::expand(size_t bitrate,
                   secure_vector<uint64_t>& S,
                   uint8_t output[], size_t output_length)
   {
   BOTAN_ARG_CHECK(bitrate % 64 == 0, "SHA-3 bitrate must be multiple of 64");

   const size_t byterate = bitrate / 8;

   while(output_length > 0)
      {
      const size_t copying = std::min(byterate, output_length);

      copy_out_vec_le(output, copying, S);

      output += copying;
      output_length -= copying;

      if(output_length > 0)
         {
         SHA_3::permute(S.data());
         }
      }
   }

SHA_3::SHA_3(size_t output_bits) :
   m_output_bits(output_bits),
   m_bitrate(1600 - 2*output_bits),
   m_S(25),
   m_S_pos(0)
   {
   // We only support the parameters for SHA-3 in this constructor

   if(output_bits != 224 && output_bits != 256 &&
      output_bits != 384 && output_bits != 512)
      throw Invalid_Argument("SHA_3: Invalid output length " +
                             std::to_string(output_bits));
   }

std::string SHA_3::name() const
   {
   return "SHA-3(" + std::to_string(m_output_bits) + ")";
   }

std::string SHA_3::provider() const
   {
#if defined(BOTAN_HAS_SHA3_BMI2)
   if(CPUID::has_bmi2())
      {
      return "bmi2";
      }
#endif

   return "base";
   }

std::unique_ptr<HashFunction> SHA_3::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHA_3(*this));
   }

HashFunction* SHA_3::clone() const
   {
   return new SHA_3(m_output_bits);
   }

void SHA_3::clear()
   {
   zeroise(m_S);
   m_S_pos = 0;
   }

void SHA_3::add_data(const uint8_t input[], size_t length)
   {
   m_S_pos = SHA_3::absorb(m_bitrate, m_S, m_S_pos, input, length);
   }

void SHA_3::final_result(uint8_t output[])
   {
   SHA_3::finish(m_bitrate, m_S, m_S_pos, 0x06, 0x80);

   /*
   * We never have to run the permutation again because we only support
   * limited output lengths
   */
   copy_out_vec_le(output, m_output_bits/8, m_S);

   clear();
   }

}
/*
* SHAKE-128/256 as a hash
* (C) 2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

SHAKE_128::SHAKE_128(size_t output_bits) :
   m_output_bits(output_bits), m_S(25), m_S_pos(0)
   {
   if(output_bits % 8 != 0)
      throw Invalid_Argument("SHAKE_128: Invalid output length " +
                             std::to_string(output_bits));
   }

std::string SHAKE_128::name() const
   {
   return "SHAKE-128(" + std::to_string(m_output_bits) + ")";
   }

HashFunction* SHAKE_128::clone() const
   {
   return new SHAKE_128(m_output_bits);
   }

std::unique_ptr<HashFunction> SHAKE_128::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHAKE_128(*this));
   }

void SHAKE_128::clear()
   {
   zeroise(m_S);
   m_S_pos = 0;
   }

void SHAKE_128::add_data(const uint8_t input[], size_t length)
   {
   m_S_pos = SHA_3::absorb(SHAKE_128_BITRATE, m_S, m_S_pos, input, length);
   }

void SHAKE_128::final_result(uint8_t output[])
   {
   SHA_3::finish(SHAKE_128_BITRATE, m_S, m_S_pos, 0x1F, 0x80);
   SHA_3::expand(SHAKE_128_BITRATE, m_S, output, output_length());
   clear();
   }

SHAKE_256::SHAKE_256(size_t output_bits) :
   m_output_bits(output_bits), m_S(25), m_S_pos(0)
   {
   if(output_bits % 8 != 0)
      throw Invalid_Argument("SHAKE_256: Invalid output length " +
                             std::to_string(output_bits));
   }

std::string SHAKE_256::name() const
   {
   return "SHAKE-256(" + std::to_string(m_output_bits) + ")";
   }

HashFunction* SHAKE_256::clone() const
   {
   return new SHAKE_256(m_output_bits);
   }

std::unique_ptr<HashFunction> SHAKE_256::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SHAKE_256(*this));
   }

void SHAKE_256::clear()
   {
   zeroise(m_S);
   m_S_pos = 0;
   }

void SHAKE_256::add_data(const uint8_t input[], size_t length)
   {
   m_S_pos = SHA_3::absorb(SHAKE_256_BITRATE, m_S, m_S_pos, input, length);
   }

void SHAKE_256::final_result(uint8_t output[])
   {
   SHA_3::finish(SHAKE_256_BITRATE, m_S, m_S_pos, 0x1F, 0x80);
   SHA_3::expand(SHAKE_256_BITRATE, m_S, output, output_length());

   clear();
   }

}
/*
* SIV Mode Encryption
* (C) 2013,2017 Jack Lloyd
* (C) 2016 Daniel Neus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

SIV_Mode::SIV_Mode(BlockCipher* cipher) :
   m_name(cipher->name() + "/SIV"),
   m_ctr(new CTR_BE(cipher->clone(), 8)),
   m_mac(new CMAC(cipher)),
   m_bs(cipher->block_size())
   {
   // Not really true but only 128 bit allowed at the moment
   if(m_bs != 16)
      throw Invalid_Argument("SIV requires a 128 bit block cipher");
   }

SIV_Mode::~SIV_Mode()
   {
   // for ~unique_ptr
   }

void SIV_Mode::clear()
   {
   m_ctr->clear();
   m_mac->clear();
   reset();
   }

void SIV_Mode::reset()
   {
   m_nonce.clear();
   m_msg_buf.clear();
   m_ad_macs.clear();
   }

std::string SIV_Mode::name() const
   {
   return m_name;
   }

bool SIV_Mode::valid_nonce_length(size_t) const
   {
   return true;
   }

size_t SIV_Mode::update_granularity() const
   {
   /*
   This value does not particularly matter as regardless SIV_Mode::update
   buffers all input, so in theory this could be 1. However as for instance
   Transform_Filter creates update_granularity() uint8_t buffers, use a
   somewhat large size to avoid bouncing on a tiny buffer.
   */
   return 128;
   }

Key_Length_Specification SIV_Mode::key_spec() const
   {
   return m_mac->key_spec().multiple(2);
   }

void SIV_Mode::key_schedule(const uint8_t key[], size_t length)
   {
   const size_t keylen = length / 2;
   m_mac->set_key(key, keylen);
   m_ctr->set_key(key + keylen, keylen);
   m_ad_macs.clear();
   }

size_t SIV_Mode::maximum_associated_data_inputs() const
   {
   return block_size() * 8 - 2;
   }

void SIV_Mode::set_associated_data_n(size_t n, const uint8_t ad[], size_t length)
   {
   const size_t max_ads = maximum_associated_data_inputs();
   if(n > max_ads)
      throw Invalid_Argument(name() + " allows no more than " + std::to_string(max_ads) + " ADs");

   if(n >= m_ad_macs.size())
      m_ad_macs.resize(n+1);

   m_ad_macs[n] = m_mac->process(ad, length);
   }

void SIV_Mode::start_msg(const uint8_t nonce[], size_t nonce_len)
   {
   if(!valid_nonce_length(nonce_len))
      throw Invalid_IV_Length(name(), nonce_len);

   if(nonce_len)
      m_nonce = m_mac->process(nonce, nonce_len);
   else
      m_nonce.clear();

   m_msg_buf.clear();
   }

size_t SIV_Mode::process(uint8_t buf[], size_t sz)
   {
   // all output is saved for processing in finish
   m_msg_buf.insert(m_msg_buf.end(), buf, buf + sz);
   return 0;
   }

secure_vector<uint8_t> SIV_Mode::S2V(const uint8_t* text, size_t text_len)
   {
   const std::vector<uint8_t> zeros(block_size());

   secure_vector<uint8_t> V = m_mac->process(zeros.data(), zeros.size());

   for(size_t i = 0; i != m_ad_macs.size(); ++i)
      {
      poly_double_n(V.data(), V.size());
      V ^= m_ad_macs[i];
      }

   if(m_nonce.size())
      {
      poly_double_n(V.data(), V.size());
      V ^= m_nonce;
      }

   if(text_len < block_size())
      {
      poly_double_n(V.data(), V.size());
      xor_buf(V.data(), text, text_len);
      V[text_len] ^= 0x80;
      return m_mac->process(V);
      }

   m_mac->update(text, text_len - block_size());
   xor_buf(V.data(), &text[text_len - block_size()], block_size());
   m_mac->update(V);

   return m_mac->final();
   }

void SIV_Mode::set_ctr_iv(secure_vector<uint8_t> V)
   {
   V[m_bs-8] &= 0x7F;
   V[m_bs-4] &= 0x7F;

   ctr().set_iv(V.data(), V.size());
   }

void SIV_Encryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ASSERT(buffer.size() >= offset, "Offset is sane");

   buffer.insert(buffer.begin() + offset, msg_buf().begin(), msg_buf().end());
   msg_buf().clear();

   const secure_vector<uint8_t> V = S2V(buffer.data() + offset, buffer.size() - offset);

   buffer.insert(buffer.begin() + offset, V.begin(), V.end());

   if(buffer.size() != offset + V.size())
      {
      set_ctr_iv(V);
      ctr().cipher1(&buffer[offset + V.size()], buffer.size() - offset - V.size());
      }
   }

void SIV_Decryption::finish(secure_vector<uint8_t>& buffer, size_t offset)
   {
   BOTAN_ASSERT(buffer.size() >= offset, "Offset is sane");

   if(msg_buf().size() > 0)
      {
      buffer.insert(buffer.begin() + offset, msg_buf().begin(), msg_buf().end());
      msg_buf().clear();
      }

   const size_t sz = buffer.size() - offset;

   BOTAN_ASSERT(sz >= tag_size(), "We have the tag");

   secure_vector<uint8_t> V(buffer.data() + offset,
                            buffer.data() + offset + block_size());

   if(buffer.size() != offset + V.size())
      {
      set_ctr_iv(V);

      ctr().cipher(buffer.data() + offset + V.size(),
                   buffer.data() + offset,
                   buffer.size() - offset - V.size());
      }

   const secure_vector<uint8_t> T = S2V(buffer.data() + offset, buffer.size() - offset - V.size());

   if(!constant_time_compare(T.data(), V.data(), T.size()))
      throw Invalid_Authentication_Tag("SIV tag check failed");

   buffer.resize(buffer.size() - tag_size());
   }

}
/*
* The Skein-512 hash function
* (C) 2009,2010,2014 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

Skein_512::Skein_512(size_t arg_output_bits,
                     const std::string& arg_personalization) :
   m_personalization(arg_personalization),
   m_output_bits(arg_output_bits),
   m_threefish(new Threefish_512),
   m_T(2), m_buffer(64), m_buf_pos(0)
   {
   if(m_output_bits == 0 || m_output_bits % 8 != 0 || m_output_bits > 512)
      throw Invalid_Argument("Bad output bits size for Skein-512");

   initial_block();
   }

std::string Skein_512::name() const
   {
   if(m_personalization != "")
      return "Skein-512(" + std::to_string(m_output_bits) + "," +
                            m_personalization + ")";
   return "Skein-512(" + std::to_string(m_output_bits) + ")";
   }

HashFunction* Skein_512::clone() const
   {
   return new Skein_512(m_output_bits, m_personalization);
   }

std::unique_ptr<HashFunction> Skein_512::copy_state() const
   {
   std::unique_ptr<Skein_512> copy(new Skein_512(m_output_bits, m_personalization));

   copy->m_threefish->m_K = this->m_threefish->m_K;
   copy->m_T = this->m_T;
   copy->m_buffer = this->m_buffer;
   copy->m_buf_pos = this->m_buf_pos;

   // work around GCC 4.8 bug
   return std::unique_ptr<HashFunction>(copy.release());
   }

void Skein_512::clear()
   {
   zeroise(m_buffer);
   m_buf_pos = 0;

   initial_block();
   }

void Skein_512::reset_tweak(type_code type, bool is_final)
   {
   m_T[0] = 0;

   m_T[1] = (static_cast<uint64_t>(type) << 56) |
          (static_cast<uint64_t>(1) << 62) |
          (static_cast<uint64_t>(is_final) << 63);
   }

void Skein_512::initial_block()
   {
   const uint8_t zeros[64] = { 0 };

   m_threefish->set_key(zeros, sizeof(zeros));

   // ASCII("SHA3") followed by version (0x0001) code
   uint8_t config_str[32] = { 0x53, 0x48, 0x41, 0x33, 0x01, 0x00, 0 };
   store_le(uint32_t(m_output_bits), config_str + 8);

   reset_tweak(SKEIN_CONFIG, true);
   ubi_512(config_str, sizeof(config_str));

   if(m_personalization != "")
      {
      /*
        This is a limitation of this implementation, and not of the
        algorithm specification. Could be fixed relatively easily, but
        doesn't seem worth the trouble.
      */
      if(m_personalization.length() > 64)
         throw Invalid_Argument("Skein personalization must be less than 64 bytes");

      const uint8_t* bits = cast_char_ptr_to_uint8(m_personalization.data());
      reset_tweak(SKEIN_PERSONALIZATION, true);
      ubi_512(bits, m_personalization.length());
      }

   reset_tweak(SKEIN_MSG, false);
   }

void Skein_512::ubi_512(const uint8_t msg[], size_t msg_len)
   {
   secure_vector<uint64_t> M(8);

   do
      {
      const size_t to_proc = std::min<size_t>(msg_len, 64);
      m_T[0] += to_proc;

      load_le(M.data(), msg, to_proc / 8);

      if(to_proc % 8)
         {
         for(size_t j = 0; j != to_proc % 8; ++j)
           M[to_proc/8] |= static_cast<uint64_t>(msg[8*(to_proc/8)+j]) << (8*j);
         }

      m_threefish->skein_feedfwd(M, m_T);

      // clear first flag if set
      m_T[1] &= ~(static_cast<uint64_t>(1) << 62);

      msg_len -= to_proc;
      msg += to_proc;
      } while(msg_len);
   }

void Skein_512::add_data(const uint8_t input[], size_t length)
   {
   if(length == 0)
      return;

   if(m_buf_pos)
      {
      buffer_insert(m_buffer, m_buf_pos, input, length);
      if(m_buf_pos + length > 64)
         {
         ubi_512(m_buffer.data(), m_buffer.size());

         input += (64 - m_buf_pos);
         length -= (64 - m_buf_pos);
         m_buf_pos = 0;
         }
      }

   const size_t full_blocks = (length - 1) / 64;

   if(full_blocks)
      ubi_512(input, 64*full_blocks);

   length -= full_blocks * 64;

   buffer_insert(m_buffer, m_buf_pos, input + full_blocks * 64, length);
   m_buf_pos += length;
   }

void Skein_512::final_result(uint8_t out[])
   {
   m_T[1] |= (static_cast<uint64_t>(1) << 63); // final block flag

   for(size_t i = m_buf_pos; i != m_buffer.size(); ++i)
      m_buffer[i] = 0;

   ubi_512(m_buffer.data(), m_buf_pos);

   const uint8_t counter[8] = { 0 };

   reset_tweak(SKEIN_OUTPUT, true);
   ubi_512(counter, sizeof(counter));

   copy_out_vec_le(out, m_output_bits / 8, m_threefish->m_K);

   m_buf_pos = 0;
   initial_block();
   }

}
/*
* SM3
* (C) 2017 Ribose Inc.
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> SM3::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new SM3(*this));
   }

namespace {

const uint32_t SM3_IV[] = {
   0x7380166fUL, 0x4914b2b9UL, 0x172442d7UL, 0xda8a0600UL,
   0xa96f30bcUL, 0x163138aaUL, 0xe38dee4dUL, 0xb0fb0e4eUL
};

inline uint32_t P0(uint32_t X)
   {
   return X ^ rotl<9>(X) ^ rotl<17>(X);
   }

inline uint32_t FF1(uint32_t X, uint32_t Y, uint32_t Z)
   {
   return (X & Y) | ((X | Y) & Z);
   //return (X & Y) | (X & Z) | (Y & Z);
   }

inline uint32_t GG1(uint32_t X, uint32_t Y, uint32_t Z)
   {
   //return (X & Y) | (~X & Z);
   return ((Z ^ (X & (Y ^ Z))));
   }

inline void R1(uint32_t A, uint32_t& B, uint32_t C, uint32_t& D,
               uint32_t E, uint32_t& F, uint32_t G, uint32_t& H,
               uint32_t TJ, uint32_t Wi, uint32_t Wj)
   {
   const uint32_t A12 = rotl<12>(A);
   const uint32_t SS1 = rotl<7>(A12 + E + TJ);
   const uint32_t TT1 = (A ^ B ^ C) + D + (SS1 ^ A12) + Wj;
   const uint32_t TT2 = (E ^ F ^ G) + H + SS1 + Wi;

   B = rotl<9>(B);
   D = TT1;
   F = rotl<19>(F);
   H = P0(TT2);
   }

inline void R2(uint32_t A, uint32_t& B, uint32_t C, uint32_t& D,
               uint32_t E, uint32_t& F, uint32_t G, uint32_t& H,
               uint32_t TJ, uint32_t Wi, uint32_t Wj)
   {
   const uint32_t A12 = rotl<12>(A);
   const uint32_t SS1 = rotl<7>(A12 + E + TJ);
   const uint32_t TT1 = FF1(A, B, C) + D + (SS1 ^ A12) + Wj;
   const uint32_t TT2 = GG1(E, F, G) + H + SS1 + Wi;

   B = rotl<9>(B);
   D = TT1;
   F = rotl<19>(F);
   H = P0(TT2);
   }

inline uint32_t P1(uint32_t X)
   {
   return X ^ rotl<15>(X) ^ rotl<23>(X);
   }

inline uint32_t SM3_E(uint32_t W0, uint32_t W7, uint32_t W13, uint32_t W3, uint32_t W10)
   {
   return P1(W0 ^ W7 ^ rotl<15>(W13)) ^ rotl<7>(W3) ^ W10;
   }

}

/*
* SM3 Compression Function
*/
void SM3::compress_n(const uint8_t input[], size_t blocks)
   {
   uint32_t A = m_digest[0], B = m_digest[1], C = m_digest[2], D = m_digest[3],
            E = m_digest[4], F = m_digest[5], G = m_digest[6], H = m_digest[7];

   for(size_t i = 0; i != blocks; ++i)
      {
      uint32_t W00 = load_be<uint32_t>(input, 0);
      uint32_t W01 = load_be<uint32_t>(input, 1);
      uint32_t W02 = load_be<uint32_t>(input, 2);
      uint32_t W03 = load_be<uint32_t>(input, 3);
      uint32_t W04 = load_be<uint32_t>(input, 4);
      uint32_t W05 = load_be<uint32_t>(input, 5);
      uint32_t W06 = load_be<uint32_t>(input, 6);
      uint32_t W07 = load_be<uint32_t>(input, 7);
      uint32_t W08 = load_be<uint32_t>(input, 8);
      uint32_t W09 = load_be<uint32_t>(input, 9);
      uint32_t W10 = load_be<uint32_t>(input, 10);
      uint32_t W11 = load_be<uint32_t>(input, 11);
      uint32_t W12 = load_be<uint32_t>(input, 12);
      uint32_t W13 = load_be<uint32_t>(input, 13);
      uint32_t W14 = load_be<uint32_t>(input, 14);
      uint32_t W15 = load_be<uint32_t>(input, 15);

      R1(A, B, C, D, E, F, G, H, 0x79CC4519, W00, W00 ^ W04);
      W00 = SM3_E(W00, W07, W13, W03, W10);
      R1(D, A, B, C, H, E, F, G, 0xF3988A32, W01, W01 ^ W05);
      W01 = SM3_E(W01, W08, W14, W04, W11);
      R1(C, D, A, B, G, H, E, F, 0xE7311465, W02, W02 ^ W06);
      W02 = SM3_E(W02, W09, W15, W05, W12);
      R1(B, C, D, A, F, G, H, E, 0xCE6228CB, W03, W03 ^ W07);
      W03 = SM3_E(W03, W10, W00, W06, W13);
      R1(A, B, C, D, E, F, G, H, 0x9CC45197, W04, W04 ^ W08);
      W04 = SM3_E(W04, W11, W01, W07, W14);
      R1(D, A, B, C, H, E, F, G, 0x3988A32F, W05, W05 ^ W09);
      W05 = SM3_E(W05, W12, W02, W08, W15);
      R1(C, D, A, B, G, H, E, F, 0x7311465E, W06, W06 ^ W10);
      W06 = SM3_E(W06, W13, W03, W09, W00);
      R1(B, C, D, A, F, G, H, E, 0xE6228CBC, W07, W07 ^ W11);
      W07 = SM3_E(W07, W14, W04, W10, W01);
      R1(A, B, C, D, E, F, G, H, 0xCC451979, W08, W08 ^ W12);
      W08 = SM3_E(W08, W15, W05, W11, W02);
      R1(D, A, B, C, H, E, F, G, 0x988A32F3, W09, W09 ^ W13);
      W09 = SM3_E(W09, W00, W06, W12, W03);
      R1(C, D, A, B, G, H, E, F, 0x311465E7, W10, W10 ^ W14);
      W10 = SM3_E(W10, W01, W07, W13, W04);
      R1(B, C, D, A, F, G, H, E, 0x6228CBCE, W11, W11 ^ W15);
      W11 = SM3_E(W11, W02, W08, W14, W05);
      R1(A, B, C, D, E, F, G, H, 0xC451979C, W12, W12 ^ W00);
      W12 = SM3_E(W12, W03, W09, W15, W06);
      R1(D, A, B, C, H, E, F, G, 0x88A32F39, W13, W13 ^ W01);
      W13 = SM3_E(W13, W04, W10, W00, W07);
      R1(C, D, A, B, G, H, E, F, 0x11465E73, W14, W14 ^ W02);
      W14 = SM3_E(W14, W05, W11, W01, W08);
      R1(B, C, D, A, F, G, H, E, 0x228CBCE6, W15, W15 ^ W03);
      W15 = SM3_E(W15, W06, W12, W02, W09);
      R2(A, B, C, D, E, F, G, H, 0x9D8A7A87, W00, W00 ^ W04);
      W00 = SM3_E(W00, W07, W13, W03, W10);
      R2(D, A, B, C, H, E, F, G, 0x3B14F50F, W01, W01 ^ W05);
      W01 = SM3_E(W01, W08, W14, W04, W11);
      R2(C, D, A, B, G, H, E, F, 0x7629EA1E, W02, W02 ^ W06);
      W02 = SM3_E(W02, W09, W15, W05, W12);
      R2(B, C, D, A, F, G, H, E, 0xEC53D43C, W03, W03 ^ W07);
      W03 = SM3_E(W03, W10, W00, W06, W13);
      R2(A, B, C, D, E, F, G, H, 0xD8A7A879, W04, W04 ^ W08);
      W04 = SM3_E(W04, W11, W01, W07, W14);
      R2(D, A, B, C, H, E, F, G, 0xB14F50F3, W05, W05 ^ W09);
      W05 = SM3_E(W05, W12, W02, W08, W15);
      R2(C, D, A, B, G, H, E, F, 0x629EA1E7, W06, W06 ^ W10);
      W06 = SM3_E(W06, W13, W03, W09, W00);
      R2(B, C, D, A, F, G, H, E, 0xC53D43CE, W07, W07 ^ W11);
      W07 = SM3_E(W07, W14, W04, W10, W01);
      R2(A, B, C, D, E, F, G, H, 0x8A7A879D, W08, W08 ^ W12);
      W08 = SM3_E(W08, W15, W05, W11, W02);
      R2(D, A, B, C, H, E, F, G, 0x14F50F3B, W09, W09 ^ W13);
      W09 = SM3_E(W09, W00, W06, W12, W03);
      R2(C, D, A, B, G, H, E, F, 0x29EA1E76, W10, W10 ^ W14);
      W10 = SM3_E(W10, W01, W07, W13, W04);
      R2(B, C, D, A, F, G, H, E, 0x53D43CEC, W11, W11 ^ W15);
      W11 = SM3_E(W11, W02, W08, W14, W05);
      R2(A, B, C, D, E, F, G, H, 0xA7A879D8, W12, W12 ^ W00);
      W12 = SM3_E(W12, W03, W09, W15, W06);
      R2(D, A, B, C, H, E, F, G, 0x4F50F3B1, W13, W13 ^ W01);
      W13 = SM3_E(W13, W04, W10, W00, W07);
      R2(C, D, A, B, G, H, E, F, 0x9EA1E762, W14, W14 ^ W02);
      W14 = SM3_E(W14, W05, W11, W01, W08);
      R2(B, C, D, A, F, G, H, E, 0x3D43CEC5, W15, W15 ^ W03);
      W15 = SM3_E(W15, W06, W12, W02, W09);
      R2(A, B, C, D, E, F, G, H, 0x7A879D8A, W00, W00 ^ W04);
      W00 = SM3_E(W00, W07, W13, W03, W10);
      R2(D, A, B, C, H, E, F, G, 0xF50F3B14, W01, W01 ^ W05);
      W01 = SM3_E(W01, W08, W14, W04, W11);
      R2(C, D, A, B, G, H, E, F, 0xEA1E7629, W02, W02 ^ W06);
      W02 = SM3_E(W02, W09, W15, W05, W12);
      R2(B, C, D, A, F, G, H, E, 0xD43CEC53, W03, W03 ^ W07);
      W03 = SM3_E(W03, W10, W00, W06, W13);
      R2(A, B, C, D, E, F, G, H, 0xA879D8A7, W04, W04 ^ W08);
      W04 = SM3_E(W04, W11, W01, W07, W14);
      R2(D, A, B, C, H, E, F, G, 0x50F3B14F, W05, W05 ^ W09);
      W05 = SM3_E(W05, W12, W02, W08, W15);
      R2(C, D, A, B, G, H, E, F, 0xA1E7629E, W06, W06 ^ W10);
      W06 = SM3_E(W06, W13, W03, W09, W00);
      R2(B, C, D, A, F, G, H, E, 0x43CEC53D, W07, W07 ^ W11);
      W07 = SM3_E(W07, W14, W04, W10, W01);
      R2(A, B, C, D, E, F, G, H, 0x879D8A7A, W08, W08 ^ W12);
      W08 = SM3_E(W08, W15, W05, W11, W02);
      R2(D, A, B, C, H, E, F, G, 0x0F3B14F5, W09, W09 ^ W13);
      W09 = SM3_E(W09, W00, W06, W12, W03);
      R2(C, D, A, B, G, H, E, F, 0x1E7629EA, W10, W10 ^ W14);
      W10 = SM3_E(W10, W01, W07, W13, W04);
      R2(B, C, D, A, F, G, H, E, 0x3CEC53D4, W11, W11 ^ W15);
      W11 = SM3_E(W11, W02, W08, W14, W05);
      R2(A, B, C, D, E, F, G, H, 0x79D8A7A8, W12, W12 ^ W00);
      W12 = SM3_E(W12, W03, W09, W15, W06);
      R2(D, A, B, C, H, E, F, G, 0xF3B14F50, W13, W13 ^ W01);
      W13 = SM3_E(W13, W04, W10, W00, W07);
      R2(C, D, A, B, G, H, E, F, 0xE7629EA1, W14, W14 ^ W02);
      W14 = SM3_E(W14, W05, W11, W01, W08);
      R2(B, C, D, A, F, G, H, E, 0xCEC53D43, W15, W15 ^ W03);
      W15 = SM3_E(W15, W06, W12, W02, W09);
      R2(A, B, C, D, E, F, G, H, 0x9D8A7A87, W00, W00 ^ W04);
      W00 = SM3_E(W00, W07, W13, W03, W10);
      R2(D, A, B, C, H, E, F, G, 0x3B14F50F, W01, W01 ^ W05);
      W01 = SM3_E(W01, W08, W14, W04, W11);
      R2(C, D, A, B, G, H, E, F, 0x7629EA1E, W02, W02 ^ W06);
      W02 = SM3_E(W02, W09, W15, W05, W12);
      R2(B, C, D, A, F, G, H, E, 0xEC53D43C, W03, W03 ^ W07);
      W03 = SM3_E(W03, W10, W00, W06, W13);
      R2(A, B, C, D, E, F, G, H, 0xD8A7A879, W04, W04 ^ W08);
      R2(D, A, B, C, H, E, F, G, 0xB14F50F3, W05, W05 ^ W09);
      R2(C, D, A, B, G, H, E, F, 0x629EA1E7, W06, W06 ^ W10);
      R2(B, C, D, A, F, G, H, E, 0xC53D43CE, W07, W07 ^ W11);
      R2(A, B, C, D, E, F, G, H, 0x8A7A879D, W08, W08 ^ W12);
      R2(D, A, B, C, H, E, F, G, 0x14F50F3B, W09, W09 ^ W13);
      R2(C, D, A, B, G, H, E, F, 0x29EA1E76, W10, W10 ^ W14);
      R2(B, C, D, A, F, G, H, E, 0x53D43CEC, W11, W11 ^ W15);
      R2(A, B, C, D, E, F, G, H, 0xA7A879D8, W12, W12 ^ W00);
      R2(D, A, B, C, H, E, F, G, 0x4F50F3B1, W13, W13 ^ W01);
      R2(C, D, A, B, G, H, E, F, 0x9EA1E762, W14, W14 ^ W02);
      R2(B, C, D, A, F, G, H, E, 0x3D43CEC5, W15, W15 ^ W03);

      A = (m_digest[0] ^= A);
      B = (m_digest[1] ^= B);
      C = (m_digest[2] ^= C);
      D = (m_digest[3] ^= D);
      E = (m_digest[4] ^= E);
      F = (m_digest[5] ^= F);
      G = (m_digest[6] ^= G);
      H = (m_digest[7] ^= H);

      input += hash_block_size();
      }
   }

/*
* Copy out the digest
*/
void SM3::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void SM3::clear()
   {
   MDx_HashFunction::clear();
   std::copy(std::begin(SM3_IV), std::end(SM3_IV), m_digest.begin());
   }

}
/*
* (C) 2016,2020 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_SYSTEM_RNG)
#endif

namespace Botan {

void Stateful_RNG::clear()
   {
   lock_guard_type<recursive_mutex_type> lock(m_mutex);
   m_reseed_counter = 0;
   m_last_pid = 0;
   clear_state();
   }

void Stateful_RNG::force_reseed()
   {
   lock_guard_type<recursive_mutex_type> lock(m_mutex);
   m_reseed_counter = 0;
   }

bool Stateful_RNG::is_seeded() const
   {
   lock_guard_type<recursive_mutex_type> lock(m_mutex);
   return m_reseed_counter > 0;
   }

void Stateful_RNG::add_entropy(const uint8_t input[], size_t input_len)
   {
   lock_guard_type<recursive_mutex_type> lock(m_mutex);

   update(input, input_len);

   if(8*input_len >= security_level())
      {
      reset_reseed_counter();
      }
   }

void Stateful_RNG::initialize_with(const uint8_t input[], size_t len)
   {
   lock_guard_type<recursive_mutex_type> lock(m_mutex);

   clear();
   add_entropy(input, len);
   }

void Stateful_RNG::randomize(uint8_t output[], size_t output_len)
   {
   randomize_with_input(output, output_len, nullptr, 0);
   }

void Stateful_RNG::randomize_with_ts_input(uint8_t output[], size_t output_len)
   {
   uint8_t additional_input[20] = { 0 };

   store_le(OS::get_high_resolution_clock(), additional_input);

#if defined(BOTAN_HAS_SYSTEM_RNG)
   System_RNG system_rng;
   system_rng.randomize(additional_input + 8, sizeof(additional_input) - 8);
#else
   store_le(OS::get_system_timestamp_ns(), additional_input + 8);
   store_le(OS::get_process_id(), additional_input + 16);
#endif

   randomize_with_input(output, output_len, additional_input, sizeof(additional_input));
   }

void Stateful_RNG::randomize_with_input(uint8_t output[], size_t output_len,
                                        const uint8_t input[], size_t input_len)
   {
   if(output_len == 0)
      return;

   lock_guard_type<recursive_mutex_type> lock(m_mutex);

   const size_t max_per_request = max_number_of_bytes_per_request();

   if(max_per_request == 0) // no limit
      {
      reseed_check();
      this->generate_output(output, output_len, input, input_len);
      }
   else
      {
      while(output_len > 0)
         {
         const size_t this_req = std::min(max_per_request, output_len);

         /*
         * We split the request into several requests to the underlying DRBG but
         * pass the input to each invocation. It might be more sensible to only
         * provide it for the first invocation, however between 2.0 and 2.15
         * HMAC_DRBG always provided it for all requests so retain that here.
         */

         reseed_check();
         this->generate_output(output, this_req, input, input_len);

         output += this_req;
         output_len -= this_req;
         }
      }
   }

size_t Stateful_RNG::reseed(Entropy_Sources& srcs,
                            size_t poll_bits,
                            std::chrono::milliseconds poll_timeout)
   {
   lock_guard_type<recursive_mutex_type> lock(m_mutex);

   const size_t bits_collected = RandomNumberGenerator::reseed(srcs, poll_bits, poll_timeout);

   if(bits_collected >= security_level())
      {
      reset_reseed_counter();
      }

   return bits_collected;
   }

void Stateful_RNG::reseed_from_rng(RandomNumberGenerator& rng, size_t poll_bits)
   {
   lock_guard_type<recursive_mutex_type> lock(m_mutex);

   RandomNumberGenerator::reseed_from_rng(rng, poll_bits);

   if(poll_bits >= security_level())
      {
      reset_reseed_counter();
      }
   }

void Stateful_RNG::reset_reseed_counter()
   {
   // Lock is held whenever this function is called
   m_reseed_counter = 1;
   }

void Stateful_RNG::reseed_check()
   {
   // Lock is held whenever this function is called

   const uint32_t cur_pid = OS::get_process_id();

   const bool fork_detected = (m_last_pid > 0) && (cur_pid != m_last_pid);

   if(is_seeded() == false ||
      fork_detected ||
      (m_reseed_interval > 0 && m_reseed_counter >= m_reseed_interval))
      {
      m_reseed_counter = 0;
      m_last_pid = cur_pid;

      if(m_underlying_rng)
         {
         reseed_from_rng(*m_underlying_rng, security_level());
         }

      if(m_entropy_sources)
         {
         reseed(*m_entropy_sources, security_level());
         }

      if(!is_seeded())
         {
         if(fork_detected)
            throw Invalid_State("Detected use of fork but cannot reseed DRBG");
         else
            throw PRNG_Unseeded(name());
         }
      }
   else
      {
      BOTAN_ASSERT(m_reseed_counter != 0, "RNG is seeded");
      m_reseed_counter += 1;
      }
   }

}
/*
* Stream Ciphers
* (C) 2015,2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#if defined(BOTAN_HAS_CHACHA)
#endif

#if defined(BOTAN_HAS_SALSA20)
#endif

#if defined(BOTAN_HAS_SHAKE_CIPHER)
#endif

#if defined(BOTAN_HAS_CTR_BE)
#endif

#if defined(BOTAN_HAS_OFB)
#endif

#if defined(BOTAN_HAS_RC4)
#endif

#if defined(BOTAN_HAS_OPENSSL)
#endif

namespace Botan {

std::unique_ptr<StreamCipher> StreamCipher::create(const std::string& algo_spec,
                                                   const std::string& provider)
   {
   const SCAN_Name req(algo_spec);

#if defined(BOTAN_HAS_CTR_BE)
   if((req.algo_name() == "CTR-BE" || req.algo_name() == "CTR") && req.arg_count_between(1,2))
      {
      if(provider.empty() || provider == "base")
         {
         auto cipher = BlockCipher::create(req.arg(0));
         if(cipher)
            {
            size_t ctr_size = req.arg_as_integer(1, cipher->block_size());
            return std::unique_ptr<StreamCipher>(new CTR_BE(cipher.release(), ctr_size));
            }
         }
      }
#endif

#if defined(BOTAN_HAS_CHACHA)
   if(req.algo_name() == "ChaCha")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new ChaCha(req.arg_as_integer(0, 20)));
      }

   if(req.algo_name() == "ChaCha20")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new ChaCha(20));
      }
#endif

#if defined(BOTAN_HAS_SALSA20)
   if(req.algo_name() == "Salsa20")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new Salsa20);
      }
#endif

#if defined(BOTAN_HAS_SHAKE_CIPHER)
   if(req.algo_name() == "SHAKE-128" || req.algo_name() == "SHAKE-128-XOF")
      {
      if(provider.empty() || provider == "base")
         return std::unique_ptr<StreamCipher>(new SHAKE_128_Cipher);
      }
#endif

#if defined(BOTAN_HAS_OFB)
   if(req.algo_name() == "OFB" && req.arg_count() == 1)
      {
      if(provider.empty() || provider == "base")
         {
         if(auto c = BlockCipher::create(req.arg(0)))
            return std::unique_ptr<StreamCipher>(new OFB(c.release()));
         }
      }
#endif

#if defined(BOTAN_HAS_RC4)

   if(req.algo_name() == "RC4" ||
      req.algo_name() == "ARC4" ||
      req.algo_name() == "MARK-4")
      {
      const size_t skip = (req.algo_name() == "MARK-4") ? 256 : req.arg_as_integer(0, 0);

#if defined(BOTAN_HAS_OPENSSL)
      if(provider.empty() || provider == "openssl")
         {
         return std::unique_ptr<StreamCipher>(make_openssl_rc4(skip));
         }
#endif

      if(provider.empty() || provider == "base")
         {
         return std::unique_ptr<StreamCipher>(new RC4(skip));
         }
      }

#endif

   BOTAN_UNUSED(req);
   BOTAN_UNUSED(provider);

   return nullptr;
   }

//static
std::unique_ptr<StreamCipher>
StreamCipher::create_or_throw(const std::string& algo,
                             const std::string& provider)
   {
   if(auto sc = StreamCipher::create(algo, provider))
      {
      return sc;
      }
   throw Lookup_Error("Stream cipher", algo, provider);
   }

std::vector<std::string> StreamCipher::providers(const std::string& algo_spec)
   {
   return probe_providers_of<StreamCipher>(algo_spec, {"base", "openssl"});
   }

}
/*
* Streebog
* (C) 2017 Ribose Inc.
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

extern const uint64_t STREEBOG_Ax[8][256];
extern const uint64_t STREEBOG_C[12][8];

std::unique_ptr<HashFunction> Streebog::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new Streebog(*this));
   }

Streebog::Streebog(size_t output_bits) :
   m_output_bits(output_bits),
   m_count(0),
   m_position(0),
   m_buffer(64),
   m_h(8),
   m_S(8)
   {
   if(output_bits != 256 && output_bits != 512)
      throw Invalid_Argument("Streebog: Invalid output length " +
                             std::to_string(output_bits));

   clear();
   }

std::string Streebog::name() const
   {
   return "Streebog-" + std::to_string(m_output_bits);
   }

/*
* Clear memory of sensitive data
*/
void Streebog::clear()
   {
   m_count = 0;
   m_position = 0;
   zeroise(m_buffer);
   zeroise(m_S);

   const uint64_t fill = (m_output_bits == 512) ? 0 : 0x0101010101010101;
   std::fill(m_h.begin(), m_h.end(), fill);
   }

/*
* Update the hash
*/
void Streebog::add_data(const uint8_t input[], size_t length)
   {
   const size_t block_size = m_buffer.size();

   if(m_position)
      {
      buffer_insert(m_buffer, m_position, input, length);

      if(m_position + length >= block_size)
         {
         compress(m_buffer.data());
         m_count += 512;
         input += (block_size - m_position);
         length -= (block_size - m_position);
         m_position = 0;
         }
      }

   const size_t full_blocks = length / block_size;
   const size_t remaining   = length % block_size;

   for(size_t i = 0; i != full_blocks; ++i)
      {
      compress(input + block_size * i);
      m_count += 512;
      }

   buffer_insert(m_buffer, m_position, input + full_blocks * block_size, remaining);
   m_position += remaining;
   }

/*
* Finalize a hash
*/
void Streebog::final_result(uint8_t output[])
   {
   m_buffer[m_position++] = 0x01;

   if(m_position != m_buffer.size())
      clear_mem(&m_buffer[m_position], m_buffer.size() - m_position);

   compress(m_buffer.data());
   m_count += (m_position - 1) * 8;

   zeroise(m_buffer);
   store_le(m_count, m_buffer.data());
   compress(m_buffer.data(), true);

   compress_64(m_S.data(), true);
   // FIXME
   std::memcpy(output, &m_h[8 - output_length() / 8], output_length());
   clear();
   }

namespace {

inline uint64_t force_le(uint64_t x)
   {
#if defined(BOTAN_TARGET_CPU_IS_LITTLE_ENDIAN)
   return x;
#elif defined(BOTAN_TARGET_CPU_IS_BIG_ENDIAN)
   return reverse_bytes(x);
#else
   store_le(x, reinterpret_cast<uint8_t*>(&x));
   return x;
#endif
   }

inline void lps(uint64_t block[8])
   {
   uint8_t r[64];
   // FIXME
   std::memcpy(r, block, 64);

   for(int i = 0; i < 8; ++i)
      {
      block[i] = force_le(STREEBOG_Ax[0][r[i + 0*8]]) ^
                 force_le(STREEBOG_Ax[1][r[i + 1*8]]) ^
                 force_le(STREEBOG_Ax[2][r[i + 2*8]]) ^
                 force_le(STREEBOG_Ax[3][r[i + 3*8]]) ^
                 force_le(STREEBOG_Ax[4][r[i + 4*8]]) ^
                 force_le(STREEBOG_Ax[5][r[i + 5*8]]) ^
                 force_le(STREEBOG_Ax[6][r[i + 6*8]]) ^
                 force_le(STREEBOG_Ax[7][r[i + 7*8]]);
      }
   }

} //namespace

void Streebog::compress(const uint8_t input[], bool last_block)
   {
   uint64_t M[8];
   std::memcpy(M, input, 64);

   compress_64(M, last_block);
   }

void Streebog::compress_64(const uint64_t M[], bool last_block)
   {
   uint64_t N = force_le(last_block ? 0ULL : m_count);

   uint64_t hN[8];
   uint64_t A[8];

   copy_mem(hN, m_h.data(), 8);
   hN[0] ^= N;
   lps(hN);

   copy_mem(A, hN, 8);

   for(size_t i = 0; i != 8; ++i)
      {
      hN[i] ^= M[i];
      }

   for(size_t i = 0; i < 12; ++i)
      {
      for(size_t j = 0; j != 8; ++j)
         A[j] ^= force_le(STREEBOG_C[i][j]);
      lps(A);

      lps(hN);
      for(size_t j = 0; j != 8; ++j)
         hN[j] ^= A[j];
      }

   for(size_t i = 0; i != 8; ++i)
      {
      m_h[i] ^= hN[i] ^ M[i];
      }

   if(!last_block)
      {
      uint64_t carry = 0;
      for(int i = 0; i < 8; i++)
         {
         const uint64_t m = force_le(M[i]);
         const uint64_t hi = force_le(m_S[i]);
         const uint64_t t = hi + m + carry;

         m_S[i] = force_le(t);
         if(t != m)
            carry = (t < m);
         }
      }
   }

}
/*
 * Derived from:
 * https://github.com/degtyarevalexey/streebog
 *
 * Copyright (c) 2013, Alexey Degtyarev <alexey@renatasystems.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


namespace Botan {

extern const uint64_t STREEBOG_Ax[8][256] =
   {
      {
      0xd01f715b5c7ef8e6ULL, 0x16fa240980778325ULL, 0xa8a42e857ee049c8ULL,
      0x6ac1068fa186465bULL, 0x6e417bd7a2e9320bULL, 0x665c8167a437daabULL,
      0x7666681aa89617f6ULL, 0x4b959163700bdcf5ULL, 0xf14be6b78df36248ULL,
      0xc585bd689a625cffULL, 0x9557d7fca67d82cbULL, 0x89f0b969af6dd366ULL,
      0xb0833d48749f6c35ULL, 0xa1998c23b1ecbc7cULL, 0x8d70c431ac02a736ULL,
      0xd6dfbc2fd0a8b69eULL, 0x37aeb3e551fa198bULL, 0x0b7d128a40b5cf9cULL,
      0x5a8f2008b5780cbcULL, 0xedec882284e333e5ULL, 0xd25fc177d3c7c2ceULL,
      0x5e0f5d50b61778ecULL, 0x1d873683c0c24cb9ULL, 0xad040bcbb45d208cULL,
      0x2f89a0285b853c76ULL, 0x5732fff6791b8d58ULL, 0x3e9311439ef6ec3fULL,
      0xc9183a809fd3c00fULL, 0x83adf3f5260a01eeULL, 0xa6791941f4e8ef10ULL,
      0x103ae97d0ca1cd5dULL, 0x2ce948121dee1b4aULL, 0x39738421dbf2bf53ULL,
      0x093da2a6cf0cf5b4ULL, 0xcd9847d89cbcb45fULL, 0xf9561c078b2d8ae8ULL,
      0x9c6a755a6971777fULL, 0xbc1ebaa0712ef0c5ULL, 0x72e61542abf963a6ULL,
      0x78bb5fde229eb12eULL, 0x14ba94250fceb90dULL, 0x844d6697630e5282ULL,
      0x98ea08026a1e032fULL, 0xf06bbea144217f5cULL, 0xdb6263d11ccb377aULL,
      0x641c314b2b8ee083ULL, 0x320e96ab9b4770cfULL, 0x1ee7deb986a96b85ULL,
      0xe96cf57a878c47b5ULL, 0xfdd6615f8842feb8ULL, 0xc83862965601dd1bULL,
      0x2ea9f83e92572162ULL, 0xf876441142ff97fcULL, 0xeb2c455608357d9dULL,
      0x5612a7e0b0c9904cULL, 0x6c01cbfb2d500823ULL, 0x4548a6a7fa037a2dULL,
      0xabc4c6bf388b6ef4ULL, 0xbade77d4fdf8bebdULL, 0x799b07c8eb4cac3aULL,
      0x0c9d87e805b19cf0ULL, 0xcb588aac106afa27ULL, 0xea0c1d40c1e76089ULL,
      0x2869354a1e816f1aULL, 0xff96d17307fbc490ULL, 0x9f0a9d602f1a5043ULL,
      0x96373fc6e016a5f7ULL, 0x5292dab8b3a6e41cULL, 0x9b8ae0382c752413ULL,
      0x4f15ec3b7364a8a5ULL, 0x3fb349555724f12bULL, 0xc7c50d4415db66d7ULL,
      0x92b7429ee379d1a7ULL, 0xd37f99611a15dfdaULL, 0x231427c05e34a086ULL,
      0xa439a96d7b51d538ULL, 0xb403401077f01865ULL, 0xdda2aea5901d7902ULL,
      0x0a5d4a9c8967d288ULL, 0xc265280adf660f93ULL, 0x8bb0094520d4e94eULL,
      0x2a29856691385532ULL, 0x42a833c5bf072941ULL, 0x73c64d54622b7eb2ULL,
      0x07e095624504536cULL, 0x8a905153e906f45aULL, 0x6f6123c16b3b2f1fULL,
      0xc6e55552dc097bc3ULL, 0x4468feb133d16739ULL, 0xe211e7f0c7398829ULL,
      0xa2f96419f7879b40ULL, 0x19074bdbc3ad38e9ULL, 0xf4ebc3f9474e0b0cULL,
      0x43886bd376d53455ULL, 0xd8028beb5aa01046ULL, 0x51f23282f5cdc320ULL,
      0xe7b1c2be0d84e16dULL, 0x081dfab006dee8a0ULL, 0x3b33340d544b857bULL,
      0x7f5bcabc679ae242ULL, 0x0edd37c48a08a6d8ULL, 0x81ed43d9a9b33bc6ULL,
      0xb1a3655ebd4d7121ULL, 0x69a1eeb5e7ed6167ULL, 0xf6ab73d5c8f73124ULL,
      0x1a67a3e185c61fd5ULL, 0x2dc91004d43c065eULL, 0x0240b02c8fb93a28ULL,
      0x90f7f2b26cc0eb8fULL, 0x3cd3a16f114fd617ULL, 0xaae49ea9f15973e0ULL,
      0x06c0cd748cd64e78ULL, 0xda423bc7d5192a6eULL, 0xc345701c16b41287ULL,
      0x6d2193ede4821537ULL, 0xfcf639494190e3acULL, 0x7c3b228621f1c57eULL,
      0xfb16ac2b0494b0c0ULL, 0xbf7e529a3745d7f9ULL, 0x6881b6a32e3f7c73ULL,
      0xca78d2bad9b8e733ULL, 0xbbfe2fc2342aa3a9ULL, 0x0dbddffecc6381e4ULL,
      0x70a6a56e2440598eULL, 0xe4d12a844befc651ULL, 0x8c509c2765d0ba22ULL,
      0xee8c6018c28814d9ULL, 0x17da7c1f49a59e31ULL, 0x609c4c1328e194d3ULL,
      0xb3e3d57232f44b09ULL, 0x91d7aaa4a512f69bULL, 0x0ffd6fd243dabbccULL,
      0x50d26a943c1fde34ULL, 0x6be15e9968545b4fULL, 0x94778fea6faf9fdfULL,
      0x2b09dd7058ea4826ULL, 0x677cd9716de5c7bfULL, 0x49d5214fffb2e6ddULL,
      0x0360e83a466b273cULL, 0x1fc786af4f7b7691ULL, 0xa0b9d435783ea168ULL,
      0xd49f0c035f118cb6ULL, 0x01205816c9d21d14ULL, 0xac2453dd7d8f3d98ULL,
      0x545217cc3f70aa64ULL, 0x26b4028e9489c9c2ULL, 0xdec2469fd6765e3eULL,
      0x04807d58036f7450ULL, 0xe5f17292823ddb45ULL, 0xf30b569b024a5860ULL,
      0x62dcfc3fa758aefbULL, 0xe84cad6c4e5e5aa1ULL, 0xccb81fce556ea94bULL,
      0x53b282ae7a74f908ULL, 0x1b47fbf74c1402c1ULL, 0x368eebf39828049fULL,
      0x7afbeff2ad278b06ULL, 0xbe5e0a8cfe97caedULL, 0xcfd8f7f413058e77ULL,
      0xf78b2bc301252c30ULL, 0x4d555c17fcdd928dULL, 0x5f2f05467fc565f8ULL,
      0x24f4b2a21b30f3eaULL, 0x860dd6bbecb768aaULL, 0x4c750401350f8f99ULL,
      0x0000000000000000ULL, 0xecccd0344d312ef1ULL, 0xb5231806be220571ULL,
      0xc105c030990d28afULL, 0x653c695de25cfd97ULL, 0x159acc33c61ca419ULL,
      0xb89ec7f872418495ULL, 0xa9847693b73254dcULL, 0x58cf90243ac13694ULL,
      0x59efc832f3132b80ULL, 0x5c4fed7c39ae42c4ULL, 0x828dabe3efd81cfaULL,
      0xd13f294d95ace5f2ULL, 0x7d1b7a90e823d86aULL, 0xb643f03cf849224dULL,
      0x3df3f979d89dcb03ULL, 0x7426d836272f2ddeULL, 0xdfe21e891fa4432aULL,
      0x3a136c1b9d99986fULL, 0xfa36f43dcd46add4ULL, 0xc025982650df35bbULL,
      0x856d3e81aadc4f96ULL, 0xc4a5e57e53b041ebULL, 0x4708168b75ba4005ULL,
      0xaf44bbe73be41aa4ULL, 0x971767d029c4b8e3ULL, 0xb9be9feebb939981ULL,
      0x215497ecd18d9aaeULL, 0x316e7e91dd2c57f3ULL, 0xcef8afe2dad79363ULL,
      0x3853dc371220a247ULL, 0x35ee03c9de4323a3ULL, 0xe6919aa8c456fc79ULL,
      0xe05157dc4880b201ULL, 0x7bdbb7e464f59612ULL, 0x127a59518318f775ULL,
      0x332ecebd52956ddbULL, 0x8f30741d23bb9d1eULL, 0xd922d3fd93720d52ULL,
      0x7746300c61440ae2ULL, 0x25d4eab4d2e2eefeULL, 0x75068020eefd30caULL,
      0x135a01474acaea61ULL, 0x304e268714fe4ae7ULL, 0xa519f17bb283c82cULL,
      0xdc82f6b359cf6416ULL, 0x5baf781e7caa11a8ULL, 0xb2c38d64fb26561dULL,
      0x34ce5bdf17913eb7ULL, 0x5d6fb56af07c5fd0ULL, 0x182713cd0a7f25fdULL,
      0x9e2ac576e6c84d57ULL, 0x9aaab82ee5a73907ULL, 0xa3d93c0f3e558654ULL,
      0x7e7b92aaae48ff56ULL, 0x872d8ead256575beULL, 0x41c8dbfff96c0e7dULL,
      0x99ca5014a3cc1e3bULL, 0x40e883e930be1369ULL, 0x1ca76e95091051adULL,
      0x4e35b42dbab6b5b1ULL, 0x05a0254ecabd6944ULL, 0xe1710fca8152af15ULL,
      0xf22b0e8dcb984574ULL, 0xb763a82a319b3f59ULL, 0x63fca4296e8ab3efULL,
      0x9d4a2d4ca0a36a6bULL, 0xe331bfe60eeb953dULL, 0xd5bf541596c391a2ULL,
      0xf5cb9bef8e9c1618ULL, 0x46284e9dbc685d11ULL, 0x2074cffa185f87baULL,
      0xbd3ee2b6b8fcedd1ULL, 0xae64e3f1f23607b0ULL, 0xfeb68965ce29d984ULL,
      0x55724fdaf6a2b770ULL, 0x29496d5cd753720eULL, 0xa75941573d3af204ULL,
      0x8e102c0bea69800aULL, 0x111ab16bc573d049ULL, 0xd7ffe439197aab8aULL,
      0xefac380e0b5a09cdULL, 0x48f579593660fbc9ULL, 0x22347fd697e6bd92ULL,
      0x61bc1405e13389c7ULL, 0x4ab5c975b9d9c1e1ULL, 0x80cd1bcf606126d2ULL,
      0x7186fd78ed92449aULL, 0x93971a882aabccb3ULL, 0x88d0e17f66bfce72ULL,
      0x27945a985d5bd4d6ULL
      },
      {
      0xde553f8c05a811c8ULL, 0x1906b59631b4f565ULL, 0x436e70d6b1964ff7ULL,
      0x36d343cb8b1e9d85ULL, 0x843dfacc858aab5aULL, 0xfdfc95c299bfc7f9ULL,
      0x0f634bdea1d51fa2ULL, 0x6d458b3b76efb3cdULL, 0x85c3f77cf8593f80ULL,
      0x3c91315fbe737cb2ULL, 0x2148b03366ace398ULL, 0x18f8b8264c6761bfULL,
      0xc830c1c495c9fb0fULL, 0x981a76102086a0aaULL, 0xaa16012142f35760ULL,
      0x35cc54060c763cf6ULL, 0x42907d66cc45db2dULL, 0x8203d44b965af4bcULL,
      0x3d6f3cefc3a0e868ULL, 0xbc73ff69d292bda7ULL, 0x8722ed0102e20a29ULL,
      0x8f8185e8cd34deb7ULL, 0x9b0561dda7ee01d9ULL, 0x5335a0193227fad6ULL,
      0xc9cecc74e81a6fd5ULL, 0x54f5832e5c2431eaULL, 0x99e47ba05d553470ULL,
      0xf7bee756acd226ceULL, 0x384e05a5571816fdULL, 0xd1367452a47d0e6aULL,
      0xf29fde1c386ad85bULL, 0x320c77316275f7caULL, 0xd0c879e2d9ae9ab0ULL,
      0xdb7406c69110ef5dULL, 0x45505e51a2461011ULL, 0xfc029872e46c5323ULL,
      0xfa3cb6f5f7bc0cc5ULL, 0x031f17cd8768a173ULL, 0xbd8df2d9af41297dULL,
      0x9d3b4f5ab43e5e3fULL, 0x4071671b36feee84ULL, 0x716207e7d3e3b83dULL,
      0x48d20ff2f9283a1aULL, 0x27769eb4757cbc7eULL, 0x5c56ebc793f2e574ULL,
      0xa48b474f9ef5dc18ULL, 0x52cbada94ff46e0cULL, 0x60c7da982d8199c6ULL,
      0x0e9d466edc068b78ULL, 0x4eec2175eaf865fcULL, 0x550b8e9e21f7a530ULL,
      0x6b7ba5bc653fec2bULL, 0x5eb7f1ba6949d0ddULL, 0x57ea94e3db4c9099ULL,
      0xf640eae6d101b214ULL, 0xdd4a284182c0b0bbULL, 0xff1d8fbf6304f250ULL,
      0xb8accb933bf9d7e8ULL, 0xe8867c478eb68c4dULL, 0x3f8e2692391bddc1ULL,
      0xcb2fd60912a15a7cULL, 0xaec935dbab983d2fULL, 0xf55ffd2b56691367ULL,
      0x80e2ce366ce1c115ULL, 0x179bf3f8edb27e1dULL, 0x01fe0db07dd394daULL,
      0xda8a0b76ecc37b87ULL, 0x44ae53e1df9584cbULL, 0xb310b4b77347a205ULL,
      0xdfab323c787b8512ULL, 0x3b511268d070b78eULL, 0x65e6e3d2b9396753ULL,
      0x6864b271e2574d58ULL, 0x259784c98fc789d7ULL, 0x02e11a7dfabb35a9ULL,
      0x8841a6dfa337158bULL, 0x7ade78c39b5dcdd0ULL, 0xb7cf804d9a2cc84aULL,
      0x20b6bd831b7f7742ULL, 0x75bd331d3a88d272ULL, 0x418f6aab4b2d7a5eULL,
      0xd9951cbb6babdaf4ULL, 0xb6318dfde7ff5c90ULL, 0x1f389b112264aa83ULL,
      0x492c024284fbaec0ULL, 0xe33a0363c608f9a0ULL, 0x2688930408af28a4ULL,
      0xc7538a1a341ce4adULL, 0x5da8e677ee2171aeULL, 0x8c9e92254a5c7fc4ULL,
      0x63d8cd55aae938b5ULL, 0x29ebd8daa97a3706ULL, 0x959827b37be88aa1ULL,
      0x1484e4356adadf6eULL, 0xa7945082199d7d6bULL, 0xbf6ce8a455fa1cd4ULL,
      0x9cc542eac9edcae5ULL, 0x79c16f0e1c356ca3ULL, 0x89bfab6fdee48151ULL,
      0xd4174d1830c5f0ffULL, 0x9258048415eb419dULL, 0x6139d72850520d1cULL,
      0x6a85a80c18ec78f1ULL, 0xcd11f88e0171059aULL, 0xcceff53e7ca29140ULL,
      0xd229639f2315af19ULL, 0x90b91ef9ef507434ULL, 0x5977d28d074a1be1ULL,
      0x311360fce51d56b9ULL, 0xc093a92d5a1f2f91ULL, 0x1a19a25bb6dc5416ULL,
      0xeb996b8a09de2d3eULL, 0xfee3820f1ed7668aULL, 0xd7085ad5b7ad518cULL,
      0x7fff41890fe53345ULL, 0xec5948bd67dde602ULL, 0x2fd5f65dbaaa68e0ULL,
      0xa5754affe32648c2ULL, 0xf8ddac880d07396cULL, 0x6fa491468c548664ULL,
      0x0c7c5c1326bdbed1ULL, 0x4a33158f03930fb3ULL, 0x699abfc19f84d982ULL,
      0xe4fa2054a80b329cULL, 0x6707f9af438252faULL, 0x08a368e9cfd6d49eULL,
      0x47b1442c58fd25b8ULL, 0xbbb3dc5ebc91769bULL, 0x1665fe489061eac7ULL,
      0x33f27a811fa66310ULL, 0x93a609346838d547ULL, 0x30ed6d4c98cec263ULL,
      0x1dd9816cd8df9f2aULL, 0x94662a03063b1e7bULL, 0x83fdd9fbeb896066ULL,
      0x7b207573e68e590aULL, 0x5f49fc0a149a4407ULL, 0x343259b671a5a82cULL,
      0xfbc2bb458a6f981fULL, 0xc272b350a0a41a38ULL, 0x3aaf1fd8ada32354ULL,
      0x6cbb868b0b3c2717ULL, 0xa2b569c88d2583feULL, 0xf180c9d1bf027928ULL,
      0xaf37386bd64ba9f5ULL, 0x12bacab2790a8088ULL, 0x4c0d3b0810435055ULL,
      0xb2eeb9070e9436dfULL, 0xc5b29067cea7d104ULL, 0xdcb425f1ff132461ULL,
      0x4f122cc5972bf126ULL, 0xac282fa651230886ULL, 0xe7e537992f6393efULL,
      0xe61b3a2952b00735ULL, 0x709c0a57ae302ce7ULL, 0xe02514ae416058d3ULL,
      0xc44c9dd7b37445deULL, 0x5a68c5408022ba92ULL, 0x1c278cdca50c0bf0ULL,
      0x6e5a9cf6f18712beULL, 0x86dce0b17f319ef3ULL, 0x2d34ec2040115d49ULL,
      0x4bcd183f7e409b69ULL, 0x2815d56ad4a9a3dcULL, 0x24698979f2141d0dULL,
      0x0000000000000000ULL, 0x1ec696a15fb73e59ULL, 0xd86b110b16784e2eULL,
      0x8e7f8858b0e74a6dULL, 0x063e2e8713d05fe6ULL, 0xe2c40ed3bbdb6d7aULL,
      0xb1f1aeca89fc97acULL, 0xe1db191e3cb3cc09ULL, 0x6418ee62c4eaf389ULL,
      0xc6ad87aa49cf7077ULL, 0xd6f65765ca7ec556ULL, 0x9afb6c6dda3d9503ULL,
      0x7ce05644888d9236ULL, 0x8d609f95378feb1eULL, 0x23a9aa4e9c17d631ULL,
      0x6226c0e5d73aac6fULL, 0x56149953a69f0443ULL, 0xeeb852c09d66d3abULL,
      0x2b0ac2a753c102afULL, 0x07c023376e03cb3cULL, 0x2ccae1903dc2c993ULL,
      0xd3d76e2f5ec63bc3ULL, 0x9e2458973356ff4cULL, 0xa66a5d32644ee9b1ULL,
      0x0a427294356de137ULL, 0x783f62be61e6f879ULL, 0x1344c70204d91452ULL,
      0x5b96c8f0fdf12e48ULL, 0xa90916ecc59bf613ULL, 0xbe92e5142829880eULL,
      0x727d102a548b194eULL, 0x1be7afebcb0fc0ccULL, 0x3e702b2244c8491bULL,
      0xd5e940a84d166425ULL, 0x66f9f41f3e51c620ULL, 0xabe80c913f20c3baULL,
      0xf07ec461c2d1edf2ULL, 0xf361d3ac45b94c81ULL, 0x0521394a94b8fe95ULL,
      0xadd622162cf09c5cULL, 0xe97871f7f3651897ULL, 0xf4a1f09b2bba87bdULL,
      0x095d6559b2054044ULL, 0x0bbc7f2448be75edULL, 0x2af4cf172e129675ULL,
      0x157ae98517094bb4ULL, 0x9fda55274e856b96ULL, 0x914713499283e0eeULL,
      0xb952c623462a4332ULL, 0x74433ead475b46a8ULL, 0x8b5eb112245fb4f8ULL,
      0xa34b6478f0f61724ULL, 0x11a5dd7ffe6221fbULL, 0xc16da49d27ccbb4bULL,
      0x76a224d0bde07301ULL, 0x8aa0bca2598c2022ULL, 0x4df336b86d90c48fULL,
      0xea67663a740db9e4ULL, 0xef465f70e0b54771ULL, 0x39b008152acb8227ULL,
      0x7d1e5bf4f55e06ecULL, 0x105bd0cf83b1b521ULL, 0x775c2960c033e7dbULL,
      0x7e014c397236a79fULL, 0x811cc386113255cfULL, 0xeda7450d1a0e72d8ULL,
      0x5889df3d7a998f3bULL, 0x2e2bfbedc779fc3aULL, 0xce0eef438619a4e9ULL,
      0x372d4e7bf6cd095fULL, 0x04df34fae96b6a4fULL, 0xf923a13870d4adb6ULL,
      0xa1aa7e050a4d228dULL, 0xa8f71b5cb84862c9ULL, 0xb52e9a306097fde3ULL,
      0x0d8251a35b6e2a0bULL, 0x2257a7fee1c442ebULL, 0x73831d9a29588d94ULL,
      0x51d4ba64c89ccf7fULL, 0x502ab7d4b54f5ba5ULL, 0x97793dce8153bf08ULL,
      0xe5042de4d5d8a646ULL, 0x9687307efc802bd2ULL, 0xa05473b5779eb657ULL,
      0xb4d097801d446939ULL, 0xcff0e2f3fbca3033ULL, 0xc38cbee0dd778ee2ULL,
      0x464f499c252eb162ULL, 0xcad1dbb96f72cea6ULL, 0xba4dd1eec142e241ULL,
      0xb00fa37af42f0376ULL
      },
      {
      0xcce4cd3aa968b245ULL, 0x089d5484e80b7fafULL, 0x638246c1b3548304ULL,
      0xd2fe0ec8c2355492ULL, 0xa7fbdf7ff2374eeeULL, 0x4df1600c92337a16ULL,
      0x84e503ea523b12fbULL, 0x0790bbfd53ab0c4aULL, 0x198a780f38f6ea9dULL,
      0x2ab30c8f55ec48cbULL, 0xe0f7fed6b2c49db5ULL, 0xb6ecf3f422cadbdcULL,
      0x409c9a541358df11ULL, 0xd3ce8a56dfde3fe3ULL, 0xc3e9224312c8c1a0ULL,
      0x0d6dfa58816ba507ULL, 0xddf3e1b179952777ULL, 0x04c02a42748bb1d9ULL,
      0x94c2abff9f2decb8ULL, 0x4f91752da8f8acf4ULL, 0x78682befb169bf7bULL,
      0xe1c77a48af2ff6c4ULL, 0x0c5d7ec69c80ce76ULL, 0x4cc1e4928fd81167ULL,
      0xfeed3d24d9997b62ULL, 0x518bb6dfc3a54a23ULL, 0x6dbf2d26151f9b90ULL,
      0xb5bc624b05ea664fULL, 0xe86aaa525acfe21aULL, 0x4801ced0fb53a0beULL,
      0xc91463e6c00868edULL, 0x1027a815cd16fe43ULL, 0xf67069a0319204cdULL,
      0xb04ccc976c8abce7ULL, 0xc0b9b3fc35e87c33ULL, 0xf380c77c58f2de65ULL,
      0x50bb3241de4e2152ULL, 0xdf93f490435ef195ULL, 0xf1e0d25d62390887ULL,
      0xaf668bfb1a3c3141ULL, 0xbc11b251f00a7291ULL, 0x73a5eed47e427d47ULL,
      0x25bee3f6ee4c3b2eULL, 0x43cc0beb34786282ULL, 0xc824e778dde3039cULL,
      0xf97d86d98a327728ULL, 0xf2b043e24519b514ULL, 0xe297ebf7880f4b57ULL,
      0x3a94a49a98fab688ULL, 0x868516cb68f0c419ULL, 0xeffa11af0964ee50ULL,
      0xa4ab4ec0d517f37dULL, 0xa9c6b498547c567aULL, 0x8e18424f80fbbbb6ULL,
      0x0bcdc53bcf2bc23cULL, 0x137739aaea3643d0ULL, 0x2c1333ec1bac2ff0ULL,
      0x8d48d3f0a7db0625ULL, 0x1e1ac3f26b5de6d7ULL, 0xf520f81f16b2b95eULL,
      0x9f0f6ec450062e84ULL, 0x0130849e1deb6b71ULL, 0xd45e31ab8c7533a9ULL,
      0x652279a2fd14e43fULL, 0x3209f01e70f1c927ULL, 0xbe71a770cac1a473ULL,
      0x0e3d6be7a64b1894ULL, 0x7ec8148cff29d840ULL, 0xcb7476c7fac3be0fULL,
      0x72956a4a63a91636ULL, 0x37f95ec21991138fULL, 0x9e3fea5a4ded45f5ULL,
      0x7b38ba50964902e8ULL, 0x222e580bbde73764ULL, 0x61e253e0899f55e6ULL,
      0xfc8d2805e352ad80ULL, 0x35994be3235ac56dULL, 0x09add01af5e014deULL,
      0x5e8659a6780539c6ULL, 0xb17c48097161d796ULL, 0x026015213acbd6e2ULL,
      0xd1ae9f77e515e901ULL, 0xb7dc776a3f21b0adULL, 0xaba6a1b96eb78098ULL,
      0x9bcf4486248d9f5dULL, 0x582666c536455efdULL, 0xfdbdac9bfeb9c6f1ULL,
      0xc47999be4163cdeaULL, 0x765540081722a7efULL, 0x3e548ed8ec710751ULL,
      0x3d041f67cb51bac2ULL, 0x7958af71ac82d40aULL, 0x36c9da5c047a78feULL,
      0xed9a048e33af38b2ULL, 0x26ee7249c96c86bdULL, 0x900281bdeba65d61ULL,
      0x11172c8bd0fd9532ULL, 0xea0abf73600434f8ULL, 0x42fc8f75299309f3ULL,
      0x34a9cf7d3eb1ae1cULL, 0x2b838811480723baULL, 0x5ce64c8742ceef24ULL,
      0x1adae9b01fd6570eULL, 0x3c349bf9d6bad1b3ULL, 0x82453c891c7b75c0ULL,
      0x97923a40b80d512bULL, 0x4a61dbf1c198765cULL, 0xb48ce6d518010d3eULL,
      0xcfb45c858e480fd6ULL, 0xd933cbf30d1e96aeULL, 0xd70ea014ab558e3aULL,
      0xc189376228031742ULL, 0x9262949cd16d8b83ULL, 0xeb3a3bed7def5f89ULL,
      0x49314a4ee6b8cbcfULL, 0xdcc3652f647e4c06ULL, 0xda635a4c2a3e2b3dULL,
      0x470c21a940f3d35bULL, 0x315961a157d174b4ULL, 0x6672e81dda3459acULL,
      0x5b76f77a1165e36eULL, 0x445cb01667d36ec8ULL, 0xc5491d205c88a69bULL,
      0x456c34887a3805b9ULL, 0xffddb9bac4721013ULL, 0x99af51a71e4649bfULL,
      0xa15be01cbc7729d5ULL, 0x52db2760e485f7b0ULL, 0x8c78576eba306d54ULL,
      0xae560f6507d75a30ULL, 0x95f22f6182c687c9ULL, 0x71c5fbf54489aba5ULL,
      0xca44f259e728d57eULL, 0x88b87d2ccebbdc8dULL, 0xbab18d32be4a15aaULL,
      0x8be8ec93e99b611eULL, 0x17b713e89ebdf209ULL, 0xb31c5d284baa0174ULL,
      0xeeca9531148f8521ULL, 0xb8d198138481c348ULL, 0x8988f9b2d350b7fcULL,
      0xb9e11c8d996aa839ULL, 0x5a4673e40c8e881fULL, 0x1687977683569978ULL,
      0xbf4123eed72acf02ULL, 0x4ea1f1b3b513c785ULL, 0xe767452be16f91ffULL,
      0x7505d1b730021a7cULL, 0xa59bca5ec8fc980cULL, 0xad069eda20f7e7a3ULL,
      0x38f4b1bba231606aULL, 0x60d2d77e94743e97ULL, 0x9affc0183966f42cULL,
      0x248e6768f3a7505fULL, 0xcdd449a4b483d934ULL, 0x87b59255751baf68ULL,
      0x1bea6d2e023d3c7fULL, 0x6b1f12455b5ffcabULL, 0x743555292de9710dULL,
      0xd8034f6d10f5fddfULL, 0xc6198c9f7ba81b08ULL, 0xbb8109aca3a17edbULL,
      0xfa2d1766ad12cabbULL, 0xc729080166437079ULL, 0x9c5fff7b77269317ULL,
      0x0000000000000000ULL, 0x15d706c9a47624ebULL, 0x6fdf38072fd44d72ULL,
      0x5fb6dd3865ee52b7ULL, 0xa33bf53d86bcff37ULL, 0xe657c1b5fc84fa8eULL,
      0xaa962527735cebe9ULL, 0x39c43525bfda0b1bULL, 0x204e4d2a872ce186ULL,
      0x7a083ece8ba26999ULL, 0x554b9c9db72efbfaULL, 0xb22cd9b656416a05ULL,
      0x96a2bedea5e63a5aULL, 0x802529a826b0a322ULL, 0x8115ad363b5bc853ULL,
      0x8375b81701901eb1ULL, 0x3069e53f4a3a1fc5ULL, 0xbd2136cfede119e0ULL,
      0x18bafc91251d81ecULL, 0x1d4a524d4c7d5b44ULL, 0x05f0aedc6960daa8ULL,
      0x29e39d3072ccf558ULL, 0x70f57f6b5962c0d4ULL, 0x989fd53903ad22ceULL,
      0xf84d024797d91c59ULL, 0x547b1803aac5908bULL, 0xf0d056c37fd263f6ULL,
      0xd56eb535919e58d8ULL, 0x1c7ad6d351963035ULL, 0x2e7326cd2167f912ULL,
      0xac361a443d1c8cd2ULL, 0x697f076461942a49ULL, 0x4b515f6fdc731d2dULL,
      0x8ad8680df4700a6fULL, 0x41ac1eca0eb3b460ULL, 0x7d988533d80965d3ULL,
      0xa8f6300649973d0bULL, 0x7765c4960ac9cc9eULL, 0x7ca801adc5e20ea2ULL,
      0xdea3700e5eb59ae4ULL, 0xa06b6482a19c42a4ULL, 0x6a2f96db46b497daULL,
      0x27def6d7d487edccULL, 0x463ca5375d18b82aULL, 0xa6cb5be1efdc259fULL,
      0x53eba3fef96e9cc1ULL, 0xce84d81b93a364a7ULL, 0xf4107c810b59d22fULL,
      0x333974806d1aa256ULL, 0x0f0def79bba073e5ULL, 0x231edc95a00c5c15ULL,
      0xe437d494c64f2c6cULL, 0x91320523f64d3610ULL, 0x67426c83c7df32ddULL,
      0x6eefbc99323f2603ULL, 0x9d6f7be56acdf866ULL, 0x5916e25b2bae358cULL,
      0x7ff89012e2c2b331ULL, 0x035091bf2720bd93ULL, 0x561b0d22900e4669ULL,
      0x28d319ae6f279e29ULL, 0x2f43a2533c8c9263ULL, 0xd09e1be9f8fe8270ULL,
      0xf740ed3e2c796fbcULL, 0xdb53ded237d5404cULL, 0x62b2c25faebfe875ULL,
      0x0afd41a5d2c0a94dULL, 0x6412fd3ce0ff8f4eULL, 0xe3a76f6995e42026ULL,
      0x6c8fa9b808f4f0e1ULL, 0xc2d9a6dd0f23aad1ULL, 0x8f28c6d19d10d0c7ULL,
      0x85d587744fd0798aULL, 0xa20b71a39b579446ULL, 0x684f83fa7c7f4138ULL,
      0xe507500adba4471dULL, 0x3f640a46f19a6c20ULL, 0x1247bd34f7dd28a1ULL,
      0x2d23b77206474481ULL, 0x93521002cc86e0f2ULL, 0x572b89bc8de52d18ULL,
      0xfb1d93f8b0f9a1caULL, 0xe95a2ecc4724896bULL, 0x3ba420048511ddf9ULL,
      0xd63e248ab6bee54bULL, 0x5dd6c8195f258455ULL, 0x06a03f634e40673bULL,
      0x1f2a476c76b68da6ULL, 0x217ec9b49ac78af7ULL, 0xecaa80102e4453c3ULL,
      0x14e78257b99d4f9aULL
      },
      {
      0x20329b2cc87bba05ULL, 0x4f5eb6f86546a531ULL, 0xd4f44775f751b6b1ULL,
      0x8266a47b850dfa8bULL, 0xbb986aa15a6ca985ULL, 0xc979eb08f9ae0f99ULL,
      0x2da6f447a2375ea1ULL, 0x1e74275dcd7d8576ULL, 0xbc20180a800bc5f8ULL,
      0xb4a2f701b2dc65beULL, 0xe726946f981b6d66ULL, 0x48e6c453bf21c94cULL,
      0x42cad9930f0a4195ULL, 0xefa47b64aacccd20ULL, 0x71180a8960409a42ULL,
      0x8bb3329bf6a44e0cULL, 0xd34c35de2d36daccULL, 0xa92f5b7cbc23dc96ULL,
      0xb31a85aa68bb09c3ULL, 0x13e04836a73161d2ULL, 0xb24dfc4129c51d02ULL,
      0x8ae44b70b7da5acdULL, 0xe671ed84d96579a7ULL, 0xa4bb3417d66f3832ULL,
      0x4572ab38d56d2de8ULL, 0xb1b47761ea47215cULL, 0xe81c09cf70aba15dULL,
      0xffbdb872ce7f90acULL, 0xa8782297fd5dc857ULL, 0x0d946f6b6a4ce4a4ULL,
      0xe4df1f4f5b995138ULL, 0x9ebc71edca8c5762ULL, 0x0a2c1dc0b02b88d9ULL,
      0x3b503c115d9d7b91ULL, 0xc64376a8111ec3a2ULL, 0xcec199a323c963e4ULL,
      0xdc76a87ec58616f7ULL, 0x09d596e073a9b487ULL, 0x14583a9d7d560dafULL,
      0xf4c6dc593f2a0cb4ULL, 0xdd21d19584f80236ULL, 0x4a4836983ddde1d3ULL,
      0xe58866a41ae745f9ULL, 0xf591a5b27e541875ULL, 0x891dc05074586693ULL,
      0x5b068c651810a89eULL, 0xa30346bc0c08544fULL, 0x3dbf3751c684032dULL,
      0x2a1e86ec785032dcULL, 0xf73f5779fca830eaULL, 0xb60c05ca30204d21ULL,
      0x0cc316802b32f065ULL, 0x8770241bdd96be69ULL, 0xb861e18199ee95dbULL,
      0xf805cad91418fcd1ULL, 0x29e70dccbbd20e82ULL, 0xc7140f435060d763ULL,
      0x0f3a9da0e8b0cc3bULL, 0xa2543f574d76408eULL, 0xbd7761e1c175d139ULL,
      0x4b1f4f737ca3f512ULL, 0x6dc2df1f2fc137abULL, 0xf1d05c3967b14856ULL,
      0xa742bf3715ed046cULL, 0x654030141d1697edULL, 0x07b872abda676c7dULL,
      0x3ce84eba87fa17ecULL, 0xc1fb0403cb79afdfULL, 0x3e46bc7105063f73ULL,
      0x278ae987121cd678ULL, 0xa1adb4778ef47cd0ULL, 0x26dd906c5362c2b9ULL,
      0x05168060589b44e2ULL, 0xfbfc41f9d79ac08fULL, 0x0e6de44ba9ced8faULL,
      0x9feb08068bf243a3ULL, 0x7b341749d06b129bULL, 0x229c69e74a87929aULL,
      0xe09ee6c4427c011bULL, 0x5692e30e725c4c3aULL, 0xda99a33e5e9f6e4bULL,
      0x353dd85af453a36bULL, 0x25241b4c90e0fee7ULL, 0x5de987258309d022ULL,
      0xe230140fc0802984ULL, 0x93281e86a0c0b3c6ULL, 0xf229d719a4337408ULL,
      0x6f6c2dd4ad3d1f34ULL, 0x8ea5b2fbae3f0aeeULL, 0x8331dd90c473ee4aULL,
      0x346aa1b1b52db7aaULL, 0xdf8f235e06042aa9ULL, 0xcc6f6b68a1354b7bULL,
      0x6c95a6f46ebf236aULL, 0x52d31a856bb91c19ULL, 0x1a35ded6d498d555ULL,
      0xf37eaef2e54d60c9ULL, 0x72e181a9a3c2a61cULL, 0x98537aad51952fdeULL,
      0x16f6c856ffaa2530ULL, 0xd960281e9d1d5215ULL, 0x3a0745fa1ce36f50ULL,
      0x0b7b642bf1559c18ULL, 0x59a87eae9aec8001ULL, 0x5e100c05408bec7cULL,
      0x0441f98b19e55023ULL, 0xd70dcc5534d38aefULL, 0x927f676de1bea707ULL,
      0x9769e70db925e3e5ULL, 0x7a636ea29115065aULL, 0x468b201816ef11b6ULL,
      0xab81a9b73edff409ULL, 0xc0ac7de88a07bb1eULL, 0x1f235eb68c0391b7ULL,
      0x6056b074458dd30fULL, 0xbe8eeac102f7ed67ULL, 0xcd381283e04b5fbaULL,
      0x5cbefecec277c4e3ULL, 0xd21b4c356c48ce0dULL, 0x1019c31664b35d8cULL,
      0x247362a7d19eea26ULL, 0xebe582efb3299d03ULL, 0x02aef2cb82fc289fULL,
      0x86275df09ce8aaa8ULL, 0x28b07427faac1a43ULL, 0x38a9b7319e1f47cfULL,
      0xc82e92e3b8d01b58ULL, 0x06ef0b409b1978bcULL, 0x62f842bfc771fb90ULL,
      0x9904034610eb3b1fULL, 0xded85ab5477a3e68ULL, 0x90d195a663428f98ULL,
      0x5384636e2ac708d8ULL, 0xcbd719c37b522706ULL, 0xae9729d76644b0ebULL,
      0x7c8c65e20a0c7ee6ULL, 0x80c856b007f1d214ULL, 0x8c0b40302cc32271ULL,
      0xdbcedad51fe17a8aULL, 0x740e8ae938dbdea0ULL, 0xa615c6dc549310adULL,
      0x19cc55f6171ae90bULL, 0x49b1bdb8fe5fdd8dULL, 0xed0a89af2830e5bfULL,
      0x6a7aadb4f5a65bd6ULL, 0x7e22972988f05679ULL, 0xf952b3325566e810ULL,
      0x39fecedadf61530eULL, 0x6101c99f04f3c7ceULL, 0x2e5f7f6761b562ffULL,
      0xf08725d226cf5c97ULL, 0x63af3b54860fef51ULL, 0x8ff2cb10ef411e2fULL,
      0x884ab9bb35267252ULL, 0x4df04433e7ba8daeULL, 0x9afd8866d3690741ULL,
      0x66b9bb34de94abb3ULL, 0x9baaf18d92171380ULL, 0x543c11c5f0a064a5ULL,
      0x17a1b1bdbed431f1ULL, 0xb5f58eeaf3a2717fULL, 0xc355f6c849858740ULL,
      0xec5df044694ef17eULL, 0xd83751f5dc6346d4ULL, 0xfc4433520dfdacf2ULL,
      0x0000000000000000ULL, 0x5a51f58e596ebc5fULL, 0x3285aaf12e34cf16ULL,
      0x8d5c39db6dbd36b0ULL, 0x12b731dde64f7513ULL, 0x94906c2d7aa7dfbbULL,
      0x302b583aacc8e789ULL, 0x9d45facd090e6b3cULL, 0x2165e2c78905aec4ULL,
      0x68d45f7f775a7349ULL, 0x189b2c1d5664fdcaULL, 0xe1c99f2f030215daULL,
      0x6983269436246788ULL, 0x8489af3b1e148237ULL, 0xe94b702431d5b59cULL,
      0x33d2d31a6f4adbd7ULL, 0xbfd9932a4389f9a6ULL, 0xb0e30e8aab39359dULL,
      0xd1e2c715afcaf253ULL, 0x150f43763c28196eULL, 0xc4ed846393e2eb3dULL,
      0x03f98b20c3823c5eULL, 0xfd134ab94c83b833ULL, 0x556b682eb1de7064ULL,
      0x36c4537a37d19f35ULL, 0x7559f30279a5ca61ULL, 0x799ae58252973a04ULL,
      0x9c12832648707ffdULL, 0x78cd9c6913e92ec5ULL, 0x1d8dac7d0effb928ULL,
      0x439da0784e745554ULL, 0x413352b3cc887dcbULL, 0xbacf134a1b12bd44ULL,
      0x114ebafd25cd494dULL, 0x2f08068c20cb763eULL, 0x76a07822ba27f63fULL,
      0xeab2fb04f25789c2ULL, 0xe3676de481fe3d45ULL, 0x1b62a73d95e6c194ULL,
      0x641749ff5c68832cULL, 0xa5ec4dfc97112cf3ULL, 0xf6682e92bdd6242bULL,
      0x3f11c59a44782bb2ULL, 0x317c21d1edb6f348ULL, 0xd65ab5be75ad9e2eULL,
      0x6b2dd45fb4d84f17ULL, 0xfaab381296e4d44eULL, 0xd0b5befeeeb4e692ULL,
      0x0882ef0b32d7a046ULL, 0x512a91a5a83b2047ULL, 0x963e9ee6f85bf724ULL,
      0x4e09cf132438b1f0ULL, 0x77f701c9fb59e2feULL, 0x7ddb1c094b726a27ULL,
      0x5f4775ee01f5f8bdULL, 0x9186ec4d223c9b59ULL, 0xfeeac1998f01846dULL,
      0xac39db1ce4b89874ULL, 0xb75b7c21715e59e0ULL, 0xafc0503c273aa42aULL,
      0x6e3b543fec430bf5ULL, 0x704f7362213e8e83ULL, 0x58ff0745db9294c0ULL,
      0x67eec2df9feabf72ULL, 0xa0facd9ccf8a6811ULL, 0xb936986ad890811aULL,
      0x95c715c63bd9cb7aULL, 0xca8060283a2c33c7ULL, 0x507de84ee9453486ULL,
      0x85ded6d05f6a96f6ULL, 0x1cdad5964f81ade9ULL, 0xd5a33e9eb62fa270ULL,
      0x40642b588df6690aULL, 0x7f75eec2c98e42b8ULL, 0x2cf18dace3494a60ULL,
      0x23cb100c0bf9865bULL, 0xeef3028febb2d9e1ULL, 0x4425d2d394133929ULL,
      0xaad6d05c7fa1e0c8ULL, 0xad6ea2f7a5c68cb5ULL, 0xc2028f2308fb9381ULL,
      0x819f2f5b468fc6d5ULL, 0xc5bafd88d29cfffcULL, 0x47dc59f357910577ULL,
      0x2b49ff07392e261dULL, 0x57c59ae5332258fbULL, 0x73b6f842e2bcb2ddULL,
      0xcf96e04862b77725ULL, 0x4ca73dd8a6c4996fULL, 0x015779eb417e14c1ULL,
      0x37932a9176af8bf4ULL
      },
      {
      0x190a2c9b249df23eULL, 0x2f62f8b62263e1e9ULL, 0x7a7f754740993655ULL,
      0x330b7ba4d5564d9fULL, 0x4c17a16a46672582ULL, 0xb22f08eb7d05f5b8ULL,
      0x535f47f40bc148ccULL, 0x3aec5d27d4883037ULL, 0x10ed0a1825438f96ULL,
      0x516101f72c233d17ULL, 0x13cc6f949fd04eaeULL, 0x739853c441474bfdULL,
      0x653793d90d3f5b1bULL, 0x5240647b96b0fc2fULL, 0x0c84890ad27623e0ULL,
      0xd7189b32703aaea3ULL, 0x2685de3523bd9c41ULL, 0x99317c5b11bffefaULL,
      0x0d9baa854f079703ULL, 0x70b93648fbd48ac5ULL, 0xa80441fce30bc6beULL,
      0x7287704bdc36ff1eULL, 0xb65384ed33dc1f13ULL, 0xd36417343ee34408ULL,
      0x39cd38ab6e1bf10fULL, 0x5ab861770a1f3564ULL, 0x0ebacf09f594563bULL,
      0xd04572b884708530ULL, 0x3cae9722bdb3af47ULL, 0x4a556b6f2f5cbaf2ULL,
      0xe1704f1f76c4bd74ULL, 0x5ec4ed7144c6dfcfULL, 0x16afc01d4c7810e6ULL,
      0x283f113cd629ca7aULL, 0xaf59a8761741ed2dULL, 0xeed5a3991e215facULL,
      0x3bf37ea849f984d4ULL, 0xe413e096a56ce33cULL, 0x2c439d3a98f020d1ULL,
      0x637559dc6404c46bULL, 0x9e6c95d1e5f5d569ULL, 0x24bb9836045fe99aULL,
      0x44efa466dac8ecc9ULL, 0xc6eab2a5c80895d6ULL, 0x803b50c035220cc4ULL,
      0x0321658cba93c138ULL, 0x8f9ebc465dc7ee1cULL, 0xd15a5137190131d3ULL,
      0x0fa5ec8668e5e2d8ULL, 0x91c979578d1037b1ULL, 0x0642ca05693b9f70ULL,
      0xefca80168350eb4fULL, 0x38d21b24f36a45ecULL, 0xbeab81e1af73d658ULL,
      0x8cbfd9cae7542f24ULL, 0xfd19cc0d81f11102ULL, 0x0ac6430fbb4dbc90ULL,
      0x1d76a09d6a441895ULL, 0x2a01573ff1cbbfa1ULL, 0xb572e161894fde2bULL,
      0x8124734fa853b827ULL, 0x614b1fdf43e6b1b0ULL, 0x68ac395c4238cc18ULL,
      0x21d837bfd7f7b7d2ULL, 0x20c714304a860331ULL, 0x5cfaab726324aa14ULL,
      0x74c5ba4eb50d606eULL, 0xf3a3030474654739ULL, 0x23e671bcf015c209ULL,
      0x45f087e947b9582aULL, 0xd8bd77b418df4c7bULL, 0xe06f6c90ebb50997ULL,
      0x0bd96080263c0873ULL, 0x7e03f9410e40dcfeULL, 0xb8e94be4c6484928ULL,
      0xfb5b0608e8ca8e72ULL, 0x1a2b49179e0e3306ULL, 0x4e29e76961855059ULL,
      0x4f36c4e6fcf4e4baULL, 0x49740ee395cf7bcaULL, 0xc2963ea386d17f7dULL,
      0x90d65ad810618352ULL, 0x12d34c1b02a1fa4dULL, 0xfa44258775bb3a91ULL,
      0x18150f14b9ec46ddULL, 0x1491861e6b9a653dULL, 0x9a1019d7ab2c3fc2ULL,
      0x3668d42d06fe13d7ULL, 0xdcc1fbb25606a6d0ULL, 0x969490dd795a1c22ULL,
      0x3549b1a1bc6dd2efULL, 0xc94f5e23a0ed770eULL, 0xb9f6686b5b39fdcbULL,
      0xc4d4f4a6efeae00dULL, 0xe732851a1fff2204ULL, 0x94aad6de5eb869f9ULL,
      0x3f8ff2ae07206e7fULL, 0xfe38a9813b62d03aULL, 0xa7a1ad7a8bee2466ULL,
      0x7b6056c8dde882b6ULL, 0x302a1e286fc58ca7ULL, 0x8da0fa457a259bc7ULL,
      0xb3302b64e074415bULL, 0x5402ae7eff8b635fULL, 0x08f8050c9cafc94bULL,
      0xae468bf98a3059ceULL, 0x88c355cca98dc58fULL, 0xb10e6d67c7963480ULL,
      0xbad70de7e1aa3cf3ULL, 0xbfb4a26e320262bbULL, 0xcb711820870f02d5ULL,
      0xce12b7a954a75c9dULL, 0x563ce87dd8691684ULL, 0x9f73b65e7884618aULL,
      0x2b1e74b06cba0b42ULL, 0x47cec1ea605b2df1ULL, 0x1c698312f735ac76ULL,
      0x5fdbcefed9b76b2cULL, 0x831a354c8fb1cdfcULL, 0x820516c312c0791fULL,
      0xb74ca762aeadabf0ULL, 0xfc06ef821c80a5e1ULL, 0x5723cbf24518a267ULL,
      0x9d4df05d5f661451ULL, 0x588627742dfd40bfULL, 0xda8331b73f3d39a0ULL,
      0x17b0e392d109a405ULL, 0xf965400bcf28fba9ULL, 0x7c3dbf4229a2a925ULL,
      0x023e460327e275dbULL, 0x6cd0b55a0ce126b3ULL, 0xe62da695828e96e7ULL,
      0x42ad6e63b3f373b9ULL, 0xe50cc319381d57dfULL, 0xc5cbd729729b54eeULL,
      0x46d1e265fd2a9912ULL, 0x6428b056904eeff8ULL, 0x8be23040131e04b7ULL,
      0x6709d5da2add2ec0ULL, 0x075de98af44a2b93ULL, 0x8447dcc67bfbe66fULL,
      0x6616f655b7ac9a23ULL, 0xd607b8bded4b1a40ULL, 0x0563af89d3a85e48ULL,
      0x3db1b4ad20c21ba4ULL, 0x11f22997b8323b75ULL, 0x292032b34b587e99ULL,
      0x7f1cdace9331681dULL, 0x8e819fc9c0b65affULL, 0xa1e3677fe2d5bb16ULL,
      0xcd33d225ee349da5ULL, 0xd9a2543b85aef898ULL, 0x795e10cbfa0af76dULL,
      0x25a4bbb9992e5d79ULL, 0x78413344677b438eULL, 0xf0826688cef68601ULL,
      0xd27b34bba392f0ebULL, 0x551d8df162fad7bcULL, 0x1e57c511d0d7d9adULL,
      0xdeffbdb171e4d30bULL, 0xf4feea8e802f6caaULL, 0xa480c8f6317de55eULL,
      0xa0fc44f07fa40ff5ULL, 0x95b5f551c3c9dd1aULL, 0x22f952336d6476eaULL,
      0x0000000000000000ULL, 0xa6be8ef5169f9085ULL, 0xcc2cf1aa73452946ULL,
      0x2e7ddb39bf12550aULL, 0xd526dd3157d8db78ULL, 0x486b2d6c08becf29ULL,
      0x9b0f3a58365d8b21ULL, 0xac78cdfaadd22c15ULL, 0xbc95c7e28891a383ULL,
      0x6a927f5f65dab9c3ULL, 0xc3891d2c1ba0cb9eULL, 0xeaa92f9f50f8b507ULL,
      0xcf0d9426c9d6e87eULL, 0xca6e3baf1a7eb636ULL, 0xab25247059980786ULL,
      0x69b31ad3df4978fbULL, 0xe2512a93cc577c4cULL, 0xff278a0ea61364d9ULL,
      0x71a615c766a53e26ULL, 0x89dc764334fc716cULL, 0xf87a638452594f4aULL,
      0xf2bc208be914f3daULL, 0x8766b94ac1682757ULL, 0xbbc82e687cdb8810ULL,
      0x626a7a53f9757088ULL, 0xa2c202f358467a2eULL, 0x4d0882e5db169161ULL,
      0x09e7268301de7da8ULL, 0xe897699c771ac0dcULL, 0xc8507dac3d9cc3edULL,
      0xc0a878a0a1330aa6ULL, 0x978bb352e42ba8c1ULL, 0xe9884a13ea6b743fULL,
      0x279afdbabecc28a2ULL, 0x047c8c064ed9eaabULL, 0x507e2278b15289f4ULL,
      0x599904fbb08cf45cULL, 0xbd8ae46d15e01760ULL, 0x31353da7f2b43844ULL,
      0x8558ff49e68a528cULL, 0x76fbfc4d92ef15b5ULL, 0x3456922e211c660cULL,
      0x86799ac55c1993b4ULL, 0x3e90d1219a51da9cULL, 0x2d5cbeb505819432ULL,
      0x982e5fd48cce4a19ULL, 0xdb9c1238a24c8d43ULL, 0xd439febecaa96f9bULL,
      0x418c0bef0960b281ULL, 0x158ea591f6ebd1deULL, 0x1f48e69e4da66d4eULL,
      0x8afd13cf8e6fb054ULL, 0xf5e1c9011d5ed849ULL, 0xe34e091c5126c8afULL,
      0xad67ee7530a398f6ULL, 0x43b24dec2e82c75aULL, 0x75da99c1287cd48dULL,
      0x92e81cdb3783f689ULL, 0xa3dd217cc537cecdULL, 0x60543c50de970553ULL,
      0x93f73f54aaf2426aULL, 0xa91b62737e7a725dULL, 0xf19d4507538732e2ULL,
      0x77e4dfc20f9ea156ULL, 0x7d229ccdb4d31dc6ULL, 0x1b346a98037f87e5ULL,
      0xedf4c615a4b29e94ULL, 0x4093286094110662ULL, 0xb0114ee85ae78063ULL,
      0x6ff1d0d6b672e78bULL, 0x6dcf96d591909250ULL, 0xdfe09e3eec9567e8ULL,
      0x3214582b4827f97cULL, 0xb46dc2ee143e6ac8ULL, 0xf6c0ac8da7cd1971ULL,
      0xebb60c10cd8901e4ULL, 0xf7df8f023abcad92ULL, 0x9c52d3d2c217a0b2ULL,
      0x6b8d5cd0f8ab0d20ULL, 0x3777f7a29b8fa734ULL, 0x011f238f9d71b4e3ULL,
      0xc1b75b2f3c42be45ULL, 0x5de588fdfe551ef7ULL, 0x6eeef3592b035368ULL,
      0xaa3a07ffc4e9b365ULL, 0xecebe59a39c32a77ULL, 0x5ba742f8976e8187ULL,
      0x4b4a48e0b22d0e11ULL, 0xddded83dcb771233ULL, 0xa59feb79ac0c51bdULL,
      0xc7f5912a55792135ULL
      },
      {
      0x6d6ae04668a9b08aULL, 0x3ab3f04b0be8c743ULL, 0xe51e166b54b3c908ULL,
      0xbe90a9eb35c2f139ULL, 0xb2c7066637f2bec1ULL, 0xaa6945613392202cULL,
      0x9a28c36f3b5201ebULL, 0xddce5a93ab536994ULL, 0x0e34133ef6382827ULL,
      0x52a02ba1ec55048bULL, 0xa2f88f97c4b2a177ULL, 0x8640e513ca2251a5ULL,
      0xcdf1d36258137622ULL, 0xfe6cb708dedf8ddbULL, 0x8a174a9ec8121e5dULL,
      0x679896036b81560eULL, 0x59ed033395795feeULL, 0x1dd778ab8b74edafULL,
      0xee533ef92d9f926dULL, 0x2a8c79baf8a8d8f5ULL, 0x6bcf398e69b119f6ULL,
      0xe20491742fafdd95ULL, 0x276488e0809c2aecULL, 0xea955b82d88f5cceULL,
      0x7102c63a99d9e0c4ULL, 0xf9763017a5c39946ULL, 0x429fa2501f151b3dULL,
      0x4659c72bea05d59eULL, 0x984b7fdccf5a6634ULL, 0xf742232953fbb161ULL,
      0x3041860e08c021c7ULL, 0x747bfd9616cd9386ULL, 0x4bb1367192312787ULL,
      0x1b72a1638a6c44d3ULL, 0x4a0e68a6e8359a66ULL, 0x169a5039f258b6caULL,
      0xb98a2ef44edee5a4ULL, 0xd9083fe85e43a737ULL, 0x967f6ce239624e13ULL,
      0x8874f62d3c1a7982ULL, 0x3c1629830af06e3fULL, 0x9165ebfd427e5a8eULL,
      0xb5dd81794ceeaa5cULL, 0x0de8f15a7834f219ULL, 0x70bd98ede3dd5d25ULL,
      0xaccc9ca9328a8950ULL, 0x56664eda1945ca28ULL, 0x221db34c0f8859aeULL,
      0x26dbd637fa98970dULL, 0x1acdffb4f068f932ULL, 0x4585254f64090fa0ULL,
      0x72de245e17d53afaULL, 0x1546b25d7c546cf4ULL, 0x207e0ffffb803e71ULL,
      0xfaaad2732bcf4378ULL, 0xb462dfae36ea17bdULL, 0xcf926fd1ac1b11fdULL,
      0xe0672dc7dba7ba4aULL, 0xd3fa49ad5d6b41b3ULL, 0x8ba81449b216a3bcULL,
      0x14f9ec8a0650d115ULL, 0x40fc1ee3eb1d7ce2ULL, 0x23a2ed9b758ce44fULL,
      0x782c521b14fddc7eULL, 0x1c68267cf170504eULL, 0xbcf31558c1ca96e6ULL,
      0xa781b43b4ba6d235ULL, 0xf6fd7dfe29ff0c80ULL, 0xb0a4bad5c3fad91eULL,
      0xd199f51ea963266cULL, 0x414340349119c103ULL, 0x5405f269ed4dadf7ULL,
      0xabd61bb649969dcdULL, 0x6813dbeae7bdc3c8ULL, 0x65fb2ab09f8931d1ULL,
      0xf1e7fae152e3181dULL, 0xc1a67cef5a2339daULL, 0x7a4feea8e0f5bba1ULL,
      0x1e0b9acf05783791ULL, 0x5b8ebf8061713831ULL, 0x80e53cdbcb3af8d9ULL,
      0x7e898bd315e57502ULL, 0xc6bcfbf0213f2d47ULL, 0x95a38e86b76e942dULL,
      0x092e94218d243cbaULL, 0x8339debf453622e7ULL, 0xb11be402b9fe64ffULL,
      0x57d9100d634177c9ULL, 0xcc4e8db52217cbc3ULL, 0x3b0cae9c71ec7aa2ULL,
      0xfb158ca451cbfe99ULL, 0x2b33276d82ac6514ULL, 0x01bf5ed77a04bde1ULL,
      0xc5601994af33f779ULL, 0x75c4a3416cc92e67ULL, 0xf3844652a6eb7fc2ULL,
      0x3487e375fdd0ef64ULL, 0x18ae430704609eedULL, 0x4d14efb993298efbULL,
      0x815a620cb13e4538ULL, 0x125c354207487869ULL, 0x9eeea614ce42cf48ULL,
      0xce2d3106d61fac1cULL, 0xbbe99247bad6827bULL, 0x071a871f7b1c149dULL,
      0x2e4a1cc10db81656ULL, 0x77a71ff298c149b8ULL, 0x06a5d9c80118a97cULL,
      0xad73c27e488e34b1ULL, 0x443a7b981e0db241ULL, 0xe3bbcfa355ab6074ULL,
      0x0af276450328e684ULL, 0x73617a896dd1871bULL, 0x58525de4ef7de20fULL,
      0xb7be3dcab8e6cd83ULL, 0x19111dd07e64230cULL, 0x842359a03e2a367aULL,
      0x103f89f1f3401fb6ULL, 0xdc710444d157d475ULL, 0xb835702334da5845ULL,
      0x4320fc876511a6dcULL, 0xd026abc9d3679b8dULL, 0x17250eee885c0b2bULL,
      0x90dab52a387ae76fULL, 0x31fed8d972c49c26ULL, 0x89cba8fa461ec463ULL,
      0x2ff5421677bcabb7ULL, 0x396f122f85e41d7dULL, 0xa09b332430bac6a8ULL,
      0xc888e8ced7070560ULL, 0xaeaf201ac682ee8fULL, 0x1180d7268944a257ULL,
      0xf058a43628e7a5fcULL, 0xbd4c4b8fbbce2b07ULL, 0xa1246df34abe7b49ULL,
      0x7d5569b79be9af3cULL, 0xa9b5a705bd9efa12ULL, 0xdb6b835baa4bc0e8ULL,
      0x05793bac8f147342ULL, 0x21c1512881848390ULL, 0xfdb0556c50d357e5ULL,
      0x613d4fcb6a99ff72ULL, 0x03dce2648e0cda3eULL, 0xe949b9e6568386f0ULL,
      0xfc0f0bbb2ad7ea04ULL, 0x6a70675913b5a417ULL, 0x7f36d5046fe1c8e3ULL,
      0x0c57af8d02304ff8ULL, 0x32223abdfcc84618ULL, 0x0891caf6f720815bULL,
      0xa63eeaec31a26fd4ULL, 0x2507345374944d33ULL, 0x49d28ac266394058ULL,
      0xf5219f9aa7f3d6beULL, 0x2d96fea583b4cc68ULL, 0x5a31e1571b7585d0ULL,
      0x8ed12fe53d02d0feULL, 0xdfade6205f5b0e4bULL, 0x4cabb16ee92d331aULL,
      0x04c6657bf510cea3ULL, 0xd73c2cd6a87b8f10ULL, 0xe1d87310a1a307abULL,
      0x6cd5be9112ad0d6bULL, 0x97c032354366f3f2ULL, 0xd4e0ceb22677552eULL,
      0x0000000000000000ULL, 0x29509bde76a402cbULL, 0xc27a9e8bd42fe3e4ULL,
      0x5ef7842cee654b73ULL, 0xaf107ecdbc86536eULL, 0x3fcacbe784fcb401ULL,
      0xd55f90655c73e8cfULL, 0xe6c2f40fdabf1336ULL, 0xe8f6e7312c873b11ULL,
      0xeb2a0555a28be12fULL, 0xe4a148bc2eb774e9ULL, 0x9b979db84156bc0aULL,
      0x6eb60222e6a56ab4ULL, 0x87ffbbc4b026ec44ULL, 0xc703a5275b3b90a6ULL,
      0x47e699fc9001687fULL, 0x9c8d1aa73a4aa897ULL, 0x7cea3760e1ed12ddULL,
      0x4ec80ddd1d2554c5ULL, 0x13e36b957d4cc588ULL, 0x5d2b66486069914dULL,
      0x92b90999cc7280b0ULL, 0x517cc9c56259deb5ULL, 0xc937b619ad03b881ULL,
      0xec30824ad997f5b2ULL, 0xa45d565fc5aa080bULL, 0xd6837201d27f32f1ULL,
      0x635ef3789e9198adULL, 0x531f75769651b96aULL, 0x4f77530a6721e924ULL,
      0x486dd4151c3dfdb9ULL, 0x5f48dafb9461f692ULL, 0x375b011173dc355aULL,
      0x3da9775470f4d3deULL, 0x8d0dcd81b30e0ac0ULL, 0x36e45fc609d888bbULL,
      0x55baacbe97491016ULL, 0x8cb29356c90ab721ULL, 0x76184125e2c5f459ULL,
      0x99f4210bb55edbd5ULL, 0x6f095cf59ca1d755ULL, 0x9f51f8c3b44672a9ULL,
      0x3538bda287d45285ULL, 0x50c39712185d6354ULL, 0xf23b1885dcefc223ULL,
      0x79930ccc6ef9619fULL, 0xed8fdc9da3934853ULL, 0xcb540aaa590bdf5eULL,
      0x5c94389f1a6d2cacULL, 0xe77daad8a0bbaed7ULL, 0x28efc5090ca0bf2aULL,
      0xbf2ff73c4fc64cd8ULL, 0xb37858b14df60320ULL, 0xf8c96ec0dfc724a7ULL,
      0x828680683f329f06ULL, 0x941cd051cd6a29ccULL, 0xc3c5c05cae2b5e05ULL,
      0xb601631dc2e27062ULL, 0xc01922382027843bULL, 0x24b86a840e90f0d2ULL,
      0xd245177a276ffc52ULL, 0x0f8b4de98c3c95c6ULL, 0x3e759530fef809e0ULL,
      0x0b4d2892792c5b65ULL, 0xc4df4743d5374a98ULL, 0xa5e20888bfaeb5eaULL,
      0xba56cc90c0d23f9aULL, 0x38d04cf8ffe0a09cULL, 0x62e1adafe495254cULL,
      0x0263bcb3f40867dfULL, 0xcaeb547d230f62bfULL, 0x6082111c109d4293ULL,
      0xdad4dd8cd04f7d09ULL, 0xefec602e579b2f8cULL, 0x1fb4c4187f7c8a70ULL,
      0xffd3e9dfa4db303aULL, 0x7bf0b07f9af10640ULL, 0xf49ec14dddf76b5fULL,
      0x8f6e713247066d1fULL, 0x339d646a86ccfbf9ULL, 0x64447467e58d8c30ULL,
      0x2c29a072f9b07189ULL, 0xd8b7613f24471ad6ULL, 0x6627c8d41185ebefULL,
      0xa347d140beb61c96ULL, 0xde12b8f7255fb3aaULL, 0x9d324470404e1576ULL,
      0x9306574eb6763d51ULL, 0xa80af9d2c79a47f3ULL, 0x859c0777442e8b9bULL,
      0x69ac853d9db97e29ULL
      },
      {
      0xc3407dfc2de6377eULL, 0x5b9e93eea4256f77ULL, 0xadb58fdd50c845e0ULL,
      0x5219ff11a75bed86ULL, 0x356b61cfd90b1de9ULL, 0xfb8f406e25abe037ULL,
      0x7a5a0231c0f60796ULL, 0x9d3cd216e1f5020bULL, 0x0c6550fb6b48d8f3ULL,
      0xf57508c427ff1c62ULL, 0x4ad35ffa71cb407dULL, 0x6290a2da1666aa6dULL,
      0xe284ec2349355f9fULL, 0xb3c307c53d7c84ecULL, 0x05e23c0468365a02ULL,
      0x190bac4d6c9ebfa8ULL, 0x94bbbee9e28b80faULL, 0xa34fc777529cb9b5ULL,
      0xcc7b39f095bcd978ULL, 0x2426addb0ce532e3ULL, 0x7e79329312ce4fc7ULL,
      0xab09a72eebec2917ULL, 0xf8d15499f6b9d6c2ULL, 0x1a55b8babf8c895dULL,
      0xdb8add17fb769a85ULL, 0xb57f2f368658e81bULL, 0x8acd36f18f3f41f6ULL,
      0x5ce3b7bba50f11d3ULL, 0x114dcc14d5ee2f0aULL, 0xb91a7fcded1030e8ULL,
      0x81d5425fe55de7a1ULL, 0xb6213bc1554adeeeULL, 0x80144ef95f53f5f2ULL,
      0x1e7688186db4c10cULL, 0x3b912965db5fe1bcULL, 0xc281715a97e8252dULL,
      0x54a5d7e21c7f8171ULL, 0x4b12535ccbc5522eULL, 0x1d289cefbea6f7f9ULL,
      0x6ef5f2217d2e729eULL, 0xe6a7dc819b0d17ceULL, 0x1b94b41c05829b0eULL,
      0x33d7493c622f711eULL, 0xdcf7f942fa5ce421ULL, 0x600fba8b7f7a8ecbULL,
      0x46b60f011a83988eULL, 0x235b898e0dcf4c47ULL, 0x957ab24f588592a9ULL,
      0x4354330572b5c28cULL, 0xa5f3ef84e9b8d542ULL, 0x8c711e02341b2d01ULL,
      0x0b1874ae6a62a657ULL, 0x1213d8e306fc19ffULL, 0xfe6d7c6a4d9dba35ULL,
      0x65ed868f174cd4c9ULL, 0x88522ea0e6236550ULL, 0x899322065c2d7703ULL,
      0xc01e690bfef4018bULL, 0x915982ed8abddaf8ULL, 0xbe675b98ec3a4e4cULL,
      0xa996bf7f82f00db1ULL, 0xe1daf8d49a27696aULL, 0x2effd5d3dc8986e7ULL,
      0xd153a51f2b1a2e81ULL, 0x18caa0ebd690adfbULL, 0x390e3134b243c51aULL,
      0x2778b92cdff70416ULL, 0x029f1851691c24a6ULL, 0x5e7cafeacc133575ULL,
      0xfa4e4cc89fa5f264ULL, 0x5a5f9f481e2b7d24ULL, 0x484c47ab18d764dbULL,
      0x400a27f2a1a7f479ULL, 0xaeeb9b2a83da7315ULL, 0x721c626879869734ULL,
      0x042330a2d2384851ULL, 0x85f672fd3765aff0ULL, 0xba446b3a3e02061dULL,
      0x73dd6ecec3888567ULL, 0xffac70ccf793a866ULL, 0xdfa9edb5294ed2d4ULL,
      0x6c6aea7014325638ULL, 0x834a5a0e8c41c307ULL, 0xcdba35562fb2cb2bULL,
      0x0ad97808d06cb404ULL, 0x0f3b440cb85aee06ULL, 0xe5f9c876481f213bULL,
      0x98deee1289c35809ULL, 0x59018bbfcd394bd1ULL, 0xe01bf47220297b39ULL,
      0xde68e1139340c087ULL, 0x9fa3ca4788e926adULL, 0xbb85679c840c144eULL,
      0x53d8f3b71d55ffd5ULL, 0x0da45c5dd146caa0ULL, 0x6f34fe87c72060cdULL,
      0x57fbc315cf6db784ULL, 0xcee421a1fca0fddeULL, 0x3d2d0196607b8d4bULL,
      0x642c8a29ad42c69aULL, 0x14aff010bdd87508ULL, 0xac74837beac657b3ULL,
      0x3216459ad821634dULL, 0x3fb219c70967a9edULL, 0x06bc28f3bb246cf7ULL,
      0xf2082c9126d562c6ULL, 0x66b39278c45ee23cULL, 0xbd394f6f3f2878b9ULL,
      0xfd33689d9e8f8cc0ULL, 0x37f4799eb017394fULL, 0x108cc0b26fe03d59ULL,
      0xda4bd1b1417888d6ULL, 0xb09d1332ee6eb219ULL, 0x2f3ed975668794b4ULL,
      0x58c0871977375982ULL, 0x7561463d78ace990ULL, 0x09876cff037e82f1ULL,
      0x7fb83e35a8c05d94ULL, 0x26b9b58a65f91645ULL, 0xef20b07e9873953fULL,
      0x3148516d0b3355b8ULL, 0x41cb2b541ba9e62aULL, 0x790416c613e43163ULL,
      0xa011d380818e8f40ULL, 0x3a5025c36151f3efULL, 0xd57095bdf92266d0ULL,
      0x498d4b0da2d97688ULL, 0x8b0c3a57353153a5ULL, 0x21c491df64d368e1ULL,
      0x8f2f0af5e7091bf4ULL, 0x2da1c1240f9bb012ULL, 0xc43d59a92ccc49daULL,
      0xbfa6573e56345c1fULL, 0x828b56a8364fd154ULL, 0x9a41f643e0df7cafULL,
      0xbcf843c985266aeaULL, 0x2b1de9d7b4bfdce5ULL, 0x20059d79dedd7ab2ULL,
      0x6dabe6d6ae3c446bULL, 0x45e81bf6c991ae7bULL, 0x6351ae7cac68b83eULL,
      0xa432e32253b6c711ULL, 0xd092a9b991143cd2ULL, 0xcac711032e98b58fULL,
      0xd8d4c9e02864ac70ULL, 0xc5fc550f96c25b89ULL, 0xd7ef8dec903e4276ULL,
      0x67729ede7e50f06fULL, 0xeac28c7af045cf3dULL, 0xb15c1f945460a04aULL,
      0x9cfddeb05bfb1058ULL, 0x93c69abce3a1fe5eULL, 0xeb0380dc4a4bdd6eULL,
      0xd20db1e8f8081874ULL, 0x229a8528b7c15e14ULL, 0x44291750739fbc28ULL,
      0xd3ccbd4e42060a27ULL, 0xf62b1c33f4ed2a97ULL, 0x86a8660ae4779905ULL,
      0xd62e814a2a305025ULL, 0x477703a7a08d8addULL, 0x7b9b0e977af815c5ULL,
      0x78c51a60a9ea2330ULL, 0xa6adfb733aaae3b7ULL, 0x97e5aa1e3199b60fULL,
      0x0000000000000000ULL, 0xf4b404629df10e31ULL, 0x5564db44a6719322ULL,
      0x9207961a59afec0dULL, 0x9624a6b88b97a45cULL, 0x363575380a192b1cULL,
      0x2c60cd82b595a241ULL, 0x7d272664c1dc7932ULL, 0x7142769faa94a1c1ULL,
      0xa1d0df263b809d13ULL, 0x1630e841d4c451aeULL, 0xc1df65ad44fa13d8ULL,
      0x13d2d445bcf20bacULL, 0xd915c546926abe23ULL, 0x38cf3d92084dd749ULL,
      0xe766d0272103059dULL, 0xc7634d5effde7f2fULL, 0x077d2455012a7ea4ULL,
      0xedbfa82ff16fb199ULL, 0xaf2a978c39d46146ULL, 0x42953fa3c8bbd0dfULL,
      0xcb061da59496a7dcULL, 0x25e7a17db6eb20b0ULL, 0x34aa6d6963050fbaULL,
      0xa76cf7d580a4f1e4ULL, 0xf7ea10954ee338c4ULL, 0xfcf2643b24819e93ULL,
      0xcf252d0746aeef8dULL, 0x4ef06f58a3f3082cULL, 0x563acfb37563a5d7ULL,
      0x5086e740ce47c920ULL, 0x2982f186dda3f843ULL, 0x87696aac5e798b56ULL,
      0x5d22bb1d1f010380ULL, 0x035e14f7d31236f5ULL, 0x3cec0d30da759f18ULL,
      0xf3c920379cdb7095ULL, 0xb8db736b571e22bbULL, 0xdd36f5e44052f672ULL,
      0xaac8ab8851e23b44ULL, 0xa857b3d938fe1fe2ULL, 0x17f1e4e76eca43fdULL,
      0xec7ea4894b61a3caULL, 0x9e62c6e132e734feULL, 0xd4b1991b432c7483ULL,
      0x6ad6c283af163acfULL, 0x1ce9904904a8e5aaULL, 0x5fbda34c761d2726ULL,
      0xf910583f4cb7c491ULL, 0xc6a241f845d06d7cULL, 0x4f3163fe19fd1a7fULL,
      0xe99c988d2357f9c8ULL, 0x8eee06535d0709a7ULL, 0x0efa48aa0254fc55ULL,
      0xb4be23903c56fa48ULL, 0x763f52caabbedf65ULL, 0xeee1bcd8227d876cULL,
      0xe345e085f33b4dccULL, 0x3e731561b369bbbeULL, 0x2843fd2067adea10ULL,
      0x2adce5710eb1ceb6ULL, 0xb7e03767ef44ccbdULL, 0x8db012a48e153f52ULL,
      0x61ceb62dc5749c98ULL, 0xe85d942b9959eb9bULL, 0x4c6f7709caef2c8aULL,
      0x84377e5b8d6bbda3ULL, 0x30895dcbb13d47ebULL, 0x74a04a9bc2a2fbc3ULL,
      0x6b17ce251518289cULL, 0xe438c4d0f2113368ULL, 0x1fb784bed7bad35fULL,
      0x9b80fae55ad16efcULL, 0x77fe5e6c11b0cd36ULL, 0xc858095247849129ULL,
      0x08466059b97090a2ULL, 0x01c10ca6ba0e1253ULL, 0x6988d6747c040c3aULL,
      0x6849dad2c60a1e69ULL, 0x5147ebe67449db73ULL, 0xc99905f4fd8a837aULL,
      0x991fe2b433cd4a5aULL, 0xf09734c04fc94660ULL, 0xa28ecbd1e892abe6ULL,
      0xf1563866f5c75433ULL, 0x4dae7baf70e13ed9ULL, 0x7ce62ac27bd26b61ULL,
      0x70837a39109ab392ULL, 0x90988e4b30b3c8abULL, 0xb2020b63877296bfULL,
      0x156efcb607d6675bULL
      },
      {
      0xe63f55ce97c331d0ULL, 0x25b506b0015bba16ULL, 0xc8706e29e6ad9ba8ULL,
      0x5b43d3775d521f6aULL, 0x0bfa3d577035106eULL, 0xab95fc172afb0e66ULL,
      0xf64b63979e7a3276ULL, 0xf58b4562649dad4bULL, 0x48f7c3dbae0c83f1ULL,
      0xff31916642f5c8c5ULL, 0xcbb048dc1c4a0495ULL, 0x66b8f83cdf622989ULL,
      0x35c130e908e2b9b0ULL, 0x7c761a61f0b34fa1ULL, 0x3601161cf205268dULL,
      0x9e54ccfe2219b7d6ULL, 0x8b7d90a538940837ULL, 0x9cd403588ea35d0bULL,
      0xbc3c6fea9ccc5b5aULL, 0xe5ff733b6d24aeedULL, 0xceed22de0f7eb8d2ULL,
      0xec8581cab1ab545eULL, 0xb96105e88ff8e71dULL, 0x8ca03501871a5eadULL,
      0x76ccce65d6db2a2fULL, 0x5883f582a7b58057ULL, 0x3f7be4ed2e8adc3eULL,
      0x0fe7be06355cd9c9ULL, 0xee054e6c1d11be83ULL, 0x1074365909b903a6ULL,
      0x5dde9f80b4813c10ULL, 0x4a770c7d02b6692cULL, 0x5379c8d5d7809039ULL,
      0xb4067448161ed409ULL, 0x5f5e5026183bd6cdULL, 0xe898029bf4c29df9ULL,
      0x7fb63c940a54d09cULL, 0xc5171f897f4ba8bcULL, 0xa6f28db7b31d3d72ULL,
      0x2e4f3be7716eaa78ULL, 0x0d6771a099e63314ULL, 0x82076254e41bf284ULL,
      0x2f0fd2b42733df98ULL, 0x5c9e76d3e2dc49f0ULL, 0x7aeb569619606cdbULL,
      0x83478b07b2468764ULL, 0xcfadcb8d5923cd32ULL, 0x85dac7f05b95a41eULL,
      0xb5469d1b4043a1e9ULL, 0xb821ecbbd9a592fdULL, 0x1b8e0b0e798c13c8ULL,
      0x62a57b6d9a0be02eULL, 0xfcf1b793b81257f8ULL, 0x9d94ea0bd8fe28ebULL,
      0x4cea408aeb654a56ULL, 0x23284a47e888996cULL, 0x2d8f1d128b893545ULL,
      0xf4cbac3132c0d8abULL, 0xbd7c86b9ca912ebaULL, 0x3a268eef3dbe6079ULL,
      0xf0d62f6077a9110cULL, 0x2735c916ade150cbULL, 0x89fd5f03942ee2eaULL,
      0x1acee25d2fd16628ULL, 0x90f39bab41181bffULL, 0x430dfe8cde39939fULL,
      0xf70b8ac4c8274796ULL, 0x1c53aeaac6024552ULL, 0x13b410acf35e9c9bULL,
      0xa532ab4249faa24fULL, 0x2b1251e5625a163fULL, 0xd7e3e676da4841c7ULL,
      0xa7b264e4e5404892ULL, 0xda8497d643ae72d3ULL, 0x861ae105a1723b23ULL,
      0x38a6414991048aa4ULL, 0x6578dec92585b6b4ULL, 0x0280cfa6acbaeaddULL,
      0x88bdb650c273970aULL, 0x9333bd5ebbff84c2ULL, 0x4e6a8f2c47dfa08bULL,
      0x321c954db76cef2aULL, 0x418d312a72837942ULL, 0xb29b38bfffcdf773ULL,
      0x6c022c38f90a4c07ULL, 0x5a033a240b0f6a8aULL, 0x1f93885f3ce5da6fULL,
      0xc38a537e96988bc6ULL, 0x39e6a81ac759ff44ULL, 0x29929e43cee0fce2ULL,
      0x40cdd87924de0ca2ULL, 0xe9d8ebc8a29fe819ULL, 0x0c2798f3cfbb46f4ULL,
      0x55e484223e53b343ULL, 0x4650948ecd0d2fd8ULL, 0x20e86cb2126f0651ULL,
      0x6d42c56baf5739e7ULL, 0xa06fc1405ace1e08ULL, 0x7babbfc54f3d193bULL,
      0x424d17df8864e67fULL, 0xd8045870ef14980eULL, 0xc6d7397c85ac3781ULL,
      0x21a885e1443273b1ULL, 0x67f8116f893f5c69ULL, 0x24f5efe35706cff6ULL,
      0xd56329d076f2ab1aULL, 0x5e1eb9754e66a32dULL, 0x28d2771098bd8902ULL,
      0x8f6013f47dfdc190ULL, 0x17a993fdb637553cULL, 0xe0a219397e1012aaULL,
      0x786b9930b5da8606ULL, 0x6e82e39e55b0a6daULL, 0x875a0856f72f4ec3ULL,
      0x3741ff4fa458536dULL, 0xac4859b3957558fcULL, 0x7ef6d5c75c09a57cULL,
      0xc04a758b6c7f14fbULL, 0xf9acdd91ab26ebbfULL, 0x7391a467c5ef9668ULL,
      0x335c7c1ee1319acaULL, 0xa91533b18641e4bbULL, 0xe4bf9a683b79db0dULL,
      0x8e20faa72ba0b470ULL, 0x51f907737b3a7ae4ULL, 0x2268a314bed5ec8cULL,
      0xd944b123b949edeeULL, 0x31dcb3b84d8b7017ULL, 0xd3fe65279f218860ULL,
      0x097af2f1dc8ffab3ULL, 0x9b09a6fc312d0b91ULL, 0xcc6ded78a3c4520fULL,
      0x3481d9ba5ebfcc50ULL, 0x4f2a667f1182d56bULL, 0xdfd9fdd4509ace94ULL,
      0x26752045fbbc252bULL, 0xbffc491f662bc467ULL, 0xdd593272fc202449ULL,
      0x3cbbc218d46d4303ULL, 0x91b372f817456e1fULL, 0x681faf69bc6385a0ULL,
      0xb686bbeebaa43ed4ULL, 0x1469b5084cd0ca01ULL, 0x98c98009cbca94acULL,
      0x6438379a73d8c354ULL, 0xc2caba2dc0c5fe26ULL, 0x3e3b0dbe78d7a9deULL,
      0x50b9ee202d670f04ULL, 0x4590b27b37eab0e5ULL, 0x6025b4cb36b10af3ULL,
      0xfb2c1237079c0162ULL, 0xa12f28130c936be8ULL, 0x4b37e52e54eb1cccULL,
      0x083a1ba28ad28f53ULL, 0xc10a9cd83a22611bULL, 0x9f1425ad7444c236ULL,
      0x069d4cf7e9d3237aULL, 0xedc56899e7f621beULL, 0x778c273680865fcfULL,
      0x309c5aeb1bd605f7ULL, 0x8de0dc52d1472b4dULL, 0xf8ec34c2fd7b9e5fULL,
      0xea18cd3d58787724ULL, 0xaad515447ca67b86ULL, 0x9989695a9d97e14cULL,
      0x0000000000000000ULL, 0xf196c63321f464ecULL, 0x71116bc169557cb5ULL,
      0xaf887f466f92c7c1ULL, 0x972e3e0ffe964d65ULL, 0x190ec4a8d536f915ULL,
      0x95aef1a9522ca7b8ULL, 0xdc19db21aa7d51a9ULL, 0x94ee18fa0471d258ULL,
      0x8087adf248a11859ULL, 0xc457f6da2916dd5cULL, 0xfa6cfb6451c17482ULL,
      0xf256e0c6db13fbd1ULL, 0x6a9f60cf10d96f7dULL, 0x4daaa9d9bd383fb6ULL,
      0x03c026f5fae79f3dULL, 0xde99148706c7bb74ULL, 0x2a52b8b6340763dfULL,
      0x6fc20acd03edd33aULL, 0xd423c08320afdefaULL, 0xbbe1ca4e23420dc0ULL,
      0x966ed75ca8cb3885ULL, 0xeb58246e0e2502c4ULL, 0x055d6a021334bc47ULL,
      0xa47242111fa7d7afULL, 0xe3623fcc84f78d97ULL, 0x81c744a11efc6db9ULL,
      0xaec8961539cfb221ULL, 0xf31609958d4e8e31ULL, 0x63e5923ecc5695ceULL,
      0x47107ddd9b505a38ULL, 0xa3afe7b5a0298135ULL, 0x792b7063e387f3e6ULL,
      0x0140e953565d75e0ULL, 0x12f4f9ffa503e97bULL, 0x750ce8902c3cb512ULL,
      0xdbc47e8515f30733ULL, 0x1ed3610c6ab8af8fULL, 0x5239218681dde5d9ULL,
      0xe222d69fd2aaf877ULL, 0xfe71783514a8bd25ULL, 0xcaf0a18f4a177175ULL,
      0x61655d9860ec7f13ULL, 0xe77fbc9dc19e4430ULL, 0x2ccff441ddd440a5ULL,
      0x16e97aaee06a20dcULL, 0xa855dae2d01c915bULL, 0x1d1347f9905f30b2ULL,
      0xb7c652bdecf94b34ULL, 0xd03e43d265c6175dULL, 0xfdb15ec0ee4f2218ULL,
      0x57644b8492e9599eULL, 0x07dda5a4bf8e569aULL, 0x54a46d71680ec6a3ULL,
      0x5624a2d7c4b42c7eULL, 0xbebca04c3076b187ULL, 0x7d36f332a6ee3a41ULL,
      0x3b6667bc6be31599ULL, 0x695f463aea3ef040ULL, 0xad08b0e0c3282d1cULL,
      0xb15b1e4a052a684eULL, 0x44d05b2861b7c505ULL, 0x15295c5b1a8dbfe1ULL,
      0x744c01c37a61c0f2ULL, 0x59c31cd1f1e8f5b7ULL, 0xef45a73f4b4ccb63ULL,
      0x6bdf899c46841a9dULL, 0x3dfb2b4b823036e3ULL, 0xa2ef0ee6f674f4d5ULL,
      0x184e2dfb836b8cf5ULL, 0x1134df0a5fe47646ULL, 0xbaa1231d751f7820ULL,
      0xd17eaa81339b62bdULL, 0xb01bf71953771daeULL, 0x849a2ea30dc8d1feULL,
      0x705182923f080955ULL, 0x0ea757556301ac29ULL, 0x041d83514569c9a7ULL,
      0x0abad4042668658eULL, 0x49b72a88f851f611ULL, 0x8a3d79f66ec97dd7ULL,
      0xcd2d042bf59927efULL, 0xc930877ab0f0ee48ULL, 0x9273540deda2f122ULL,
      0xc797d02fd3f14261ULL, 0xe1e2f06a284d674aULL, 0xd2be8c74c97cfd80ULL,
      0x9a494faf67707e71ULL, 0xb3dbd1eca9908293ULL, 0x72d14d3493b2e388ULL,
      0xd6a30f258c153427ULL
      }
   };

extern const uint64_t STREEBOG_C[12][8] =
   {
      {
      0xdd806559f2a64507ULL,
      0x05767436cc744d23ULL,
      0xa2422a08a460d315ULL,
      0x4b7ce09192676901ULL,
      0x714eb88d7585c4fcULL,
      0x2f6a76432e45d016ULL,
      0xebcb2f81c0657c1fULL,
      0xb1085bda1ecadae9ULL
      },
      {
      0xe679047021b19bb7ULL,
      0x55dda21bd7cbcd56ULL,
      0x5cb561c2db0aa7caULL,
      0x9ab5176b12d69958ULL,
      0x61d55e0f16b50131ULL,
      0xf3feea720a232b98ULL,
      0x4fe39d460f70b5d7ULL,
      0x6fa3b58aa99d2f1aULL
      },
      {
      0x991e96f50aba0ab2ULL,
      0xc2b6f443867adb31ULL,
      0xc1c93a376062db09ULL,
      0xd3e20fe490359eb1ULL,
      0xf2ea7514b1297b7bULL,
      0x06f15e5f529c1f8bULL,
      0x0a39fc286a3d8435ULL,
      0xf574dcac2bce2fc7ULL
      },
      {
      0x220cbebc84e3d12eULL,
      0x3453eaa193e837f1ULL,
      0xd8b71333935203beULL,
      0xa9d72c82ed03d675ULL,
      0x9d721cad685e353fULL,
      0x488e857e335c3c7dULL,
      0xf948e1a05d71e4ddULL,
      0xef1fdfb3e81566d2ULL
      },
      {
      0x601758fd7c6cfe57ULL,
      0x7a56a27ea9ea63f5ULL,
      0xdfff00b723271a16ULL,
      0xbfcd1747253af5a3ULL,
      0x359e35d7800fffbdULL,
      0x7f151c1f1686104aULL,
      0x9a3f410c6ca92363ULL,
      0x4bea6bacad474799ULL
      },
      {
      0xfa68407a46647d6eULL,
      0xbf71c57236904f35ULL,
      0x0af21f66c2bec6b6ULL,
      0xcffaa6b71c9ab7b4ULL,
      0x187f9ab49af08ec6ULL,
      0x2d66c4f95142a46cULL,
      0x6fa4c33b7a3039c0ULL,
      0xae4faeae1d3ad3d9ULL
      },
      {
      0x8886564d3a14d493ULL,
      0x3517454ca23c4af3ULL,
      0x06476983284a0504ULL,
      0x0992abc52d822c37ULL,
      0xd3473e33197a93c9ULL,
      0x399ec6c7e6bf87c9ULL,
      0x51ac86febf240954ULL,
      0xf4c70e16eeaac5ecULL
      },
      {
      0xa47f0dd4bf02e71eULL,
      0x36acc2355951a8d9ULL,
      0x69d18d2bd1a5c42fULL,
      0xf4892bcb929b0690ULL,
      0x89b4443b4ddbc49aULL,
      0x4eb7f8719c36de1eULL,
      0x03e7aa020c6e4141ULL,
      0x9b1f5b424d93c9a7ULL
      },
      {
      0x7261445183235adbULL,
      0x0e38dc92cb1f2a60ULL,
      0x7b2b8a9aa6079c54ULL,
      0x800a440bdbb2ceb1ULL,
      0x3cd955b7e00d0984ULL,
      0x3a7d3a1b25894224ULL,
      0x944c9ad8ec165fdeULL,
      0x378f5a541631229bULL
      },
      {
      0x74b4c7fb98459cedULL,
      0x3698fad1153bb6c3ULL,
      0x7a1e6c303b7652f4ULL,
      0x9fe76702af69334bULL,
      0x1fffe18a1b336103ULL,
      0x8941e71cff8a78dbULL,
      0x382ae548b2e4f3f3ULL,
      0xabbedea680056f52ULL
      },
      {
      0x6bcaa4cd81f32d1bULL,
      0xdea2594ac06fd85dULL,
      0xefbacd1d7d476e98ULL,
      0x8a1d71efea48b9caULL,
      0x2001802114846679ULL,
      0xd8fa6bbbebab0761ULL,
      0x3002c6cd635afe94ULL,
      0x7bcd9ed0efc889fbULL
      },
      {
      0x48bc924af11bd720ULL,
      0xfaf417d5d9b21b99ULL,
      0xe71da4aa88e12852ULL,
      0x5d80ef9d1891cc86ULL,
      0xf82012d430219f9bULL,
      0xcda43c32bcdf1d77ULL,
      0xd21380b00449b17aULL,
      0x378ee767f11631baULL
      }
   };

}
/*
* Threefish-512
* (C) 2013,2014,2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

#define THREEFISH_ROUND(X0,X1,X2,X3,X4,X5,X6,X7,ROT1,ROT2,ROT3,ROT4) \
   do {                                                              \
      X0 += X4;                                                      \
      X1 += X5;                                                      \
      X2 += X6;                                                      \
      X3 += X7;                                                      \
      X4 = rotl<ROT1>(X4);                                           \
      X5 = rotl<ROT2>(X5);                                           \
      X6 = rotl<ROT3>(X6);                                           \
      X7 = rotl<ROT4>(X7);                                           \
      X4 ^= X0;                                                      \
      X5 ^= X1;                                                      \
      X6 ^= X2;                                                      \
      X7 ^= X3;                                                      \
   } while(0)

#define THREEFISH_INJECT_KEY(r)              \
   do {                                      \
      X0 += m_K[(r  ) % 9];                  \
      X1 += m_K[(r+1) % 9];                  \
      X2 += m_K[(r+2) % 9];                  \
      X3 += m_K[(r+3) % 9];                  \
      X4 += m_K[(r+4) % 9];                  \
      X5 += m_K[(r+5) % 9] + m_T[(r  ) % 3]; \
      X6 += m_K[(r+6) % 9] + m_T[(r+1) % 3]; \
      X7 += m_K[(r+7) % 9] + (r);            \
   } while(0)

#define THREEFISH_ENC_8_ROUNDS(R1,R2)                         \
   do {                                                       \
      THREEFISH_ROUND(X0,X2,X4,X6, X1,X3,X5,X7, 46,36,19,37); \
      THREEFISH_ROUND(X2,X4,X6,X0, X1,X7,X5,X3, 33,27,14,42); \
      THREEFISH_ROUND(X4,X6,X0,X2, X1,X3,X5,X7, 17,49,36,39); \
      THREEFISH_ROUND(X6,X0,X2,X4, X1,X7,X5,X3, 44, 9,54,56); \
      THREEFISH_INJECT_KEY(R1);                               \
                                                              \
      THREEFISH_ROUND(X0,X2,X4,X6, X1,X3,X5,X7, 39,30,34,24); \
      THREEFISH_ROUND(X2,X4,X6,X0, X1,X7,X5,X3, 13,50,10,17); \
      THREEFISH_ROUND(X4,X6,X0,X2, X1,X3,X5,X7, 25,29,39,43); \
      THREEFISH_ROUND(X6,X0,X2,X4, X1,X7,X5,X3,  8,35,56,22); \
      THREEFISH_INJECT_KEY(R2);                               \
   } while(0)

void Threefish_512::skein_feedfwd(const secure_vector<uint64_t>& M,
                                  const secure_vector<uint64_t>& T)
   {
   BOTAN_ASSERT(m_K.size() == 9, "Key was set");
   BOTAN_ASSERT(M.size() == 8, "Single block");

   m_T[0] = T[0];
   m_T[1] = T[1];
   m_T[2] = T[0] ^ T[1];

   uint64_t X0 = M[0];
   uint64_t X1 = M[1];
   uint64_t X2 = M[2];
   uint64_t X3 = M[3];
   uint64_t X4 = M[4];
   uint64_t X5 = M[5];
   uint64_t X6 = M[6];
   uint64_t X7 = M[7];

   THREEFISH_INJECT_KEY(0);

   THREEFISH_ENC_8_ROUNDS(1,2);
   THREEFISH_ENC_8_ROUNDS(3,4);
   THREEFISH_ENC_8_ROUNDS(5,6);
   THREEFISH_ENC_8_ROUNDS(7,8);
   THREEFISH_ENC_8_ROUNDS(9,10);
   THREEFISH_ENC_8_ROUNDS(11,12);
   THREEFISH_ENC_8_ROUNDS(13,14);
   THREEFISH_ENC_8_ROUNDS(15,16);
   THREEFISH_ENC_8_ROUNDS(17,18);

   m_K[0] = M[0] ^ X0;
   m_K[1] = M[1] ^ X1;
   m_K[2] = M[2] ^ X2;
   m_K[3] = M[3] ^ X3;
   m_K[4] = M[4] ^ X4;
   m_K[5] = M[5] ^ X5;
   m_K[6] = M[6] ^ X6;
   m_K[7] = M[7] ^ X7;

   m_K[8] = m_K[0] ^ m_K[1] ^ m_K[2] ^ m_K[3] ^
            m_K[4] ^ m_K[5] ^ m_K[6] ^ m_K[7] ^ 0x1BD11BDAA9FC1A22;
   }

size_t Threefish_512::parallelism() const
   {
#if defined(BOTAN_HAS_THREEFISH_512_AVX2)
   if(CPUID::has_avx2())
      {
      return 2;
      }
#endif

   return 1;
   }

std::string Threefish_512::provider() const
   {
#if defined(BOTAN_HAS_THREEFISH_512_AVX2)
   if(CPUID::has_avx2())
      {
      return "avx2";
      }
#endif

   return "base";
   }

void Threefish_512::encrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_K.empty() == false);

#if defined(BOTAN_HAS_THREEFISH_512_AVX2)
   if(CPUID::has_avx2())
      {
      return avx2_encrypt_n(in, out, blocks);
      }
#endif

   BOTAN_PARALLEL_SIMD_FOR(size_t i = 0; i < blocks; ++i)
      {
      uint64_t X0, X1, X2, X3, X4, X5, X6, X7;
      load_le(in + BLOCK_SIZE*i, X0, X1, X2, X3, X4, X5, X6, X7);

      THREEFISH_INJECT_KEY(0);

      THREEFISH_ENC_8_ROUNDS(1,2);
      THREEFISH_ENC_8_ROUNDS(3,4);
      THREEFISH_ENC_8_ROUNDS(5,6);
      THREEFISH_ENC_8_ROUNDS(7,8);
      THREEFISH_ENC_8_ROUNDS(9,10);
      THREEFISH_ENC_8_ROUNDS(11,12);
      THREEFISH_ENC_8_ROUNDS(13,14);
      THREEFISH_ENC_8_ROUNDS(15,16);
      THREEFISH_ENC_8_ROUNDS(17,18);

      store_le(out + BLOCK_SIZE*i, X0, X1, X2, X3, X4, X5, X6, X7);
      }
   }

#undef THREEFISH_ENC_8_ROUNDS
#undef THREEFISH_INJECT_KEY
#undef THREEFISH_ROUND

void Threefish_512::decrypt_n(const uint8_t in[], uint8_t out[], size_t blocks) const
   {
   verify_key_set(m_K.empty() == false);

#if defined(BOTAN_HAS_THREEFISH_512_AVX2)
   if(CPUID::has_avx2())
      {
      return avx2_decrypt_n(in, out, blocks);
      }
#endif

#define THREEFISH_ROUND(X0,X1,X2,X3,X4,X5,X6,X7,ROT1,ROT2,ROT3,ROT4) \
   do {                                                              \
      X4 ^= X0;                                                      \
      X5 ^= X1;                                                      \
      X6 ^= X2;                                                      \
      X7 ^= X3;                                                      \
      X4 = rotr<ROT1>(X4);                                           \
      X5 = rotr<ROT2>(X5);                                           \
      X6 = rotr<ROT3>(X6);                                           \
      X7 = rotr<ROT4>(X7);                                           \
      X0 -= X4;                                                      \
      X1 -= X5;                                                      \
      X2 -= X6;                                                      \
      X3 -= X7;                                                      \
   } while(0)

#define THREEFISH_INJECT_KEY(r)              \
   do {                                      \
      X0 -= m_K[(r  ) % 9];                  \
      X1 -= m_K[(r+1) % 9];                  \
      X2 -= m_K[(r+2) % 9];                  \
      X3 -= m_K[(r+3) % 9];                  \
      X4 -= m_K[(r+4) % 9];                  \
      X5 -= m_K[(r+5) % 9] + m_T[(r  ) % 3]; \
      X6 -= m_K[(r+6) % 9] + m_T[(r+1) % 3]; \
      X7 -= m_K[(r+7) % 9] + (r);            \
   } while(0)

#define THREEFISH_DEC_8_ROUNDS(R1,R2)                         \
   do {                                                       \
      THREEFISH_ROUND(X6,X0,X2,X4, X1,X7,X5,X3,  8,35,56,22); \
      THREEFISH_ROUND(X4,X6,X0,X2, X1,X3,X5,X7, 25,29,39,43); \
      THREEFISH_ROUND(X2,X4,X6,X0, X1,X7,X5,X3, 13,50,10,17); \
      THREEFISH_ROUND(X0,X2,X4,X6, X1,X3,X5,X7, 39,30,34,24); \
      THREEFISH_INJECT_KEY(R1);                               \
                                                              \
      THREEFISH_ROUND(X6,X0,X2,X4, X1,X7,X5,X3, 44, 9,54,56); \
      THREEFISH_ROUND(X4,X6,X0,X2, X1,X3,X5,X7, 17,49,36,39); \
      THREEFISH_ROUND(X2,X4,X6,X0, X1,X7,X5,X3, 33,27,14,42); \
      THREEFISH_ROUND(X0,X2,X4,X6, X1,X3,X5,X7, 46,36,19,37); \
      THREEFISH_INJECT_KEY(R2);                               \
   } while(0)

   BOTAN_PARALLEL_SIMD_FOR(size_t i = 0; i < blocks; ++i)
      {
      uint64_t X0, X1, X2, X3, X4, X5, X6, X7;
      load_le(in + BLOCK_SIZE*i, X0, X1, X2, X3, X4, X5, X6, X7);

      THREEFISH_INJECT_KEY(18);

      THREEFISH_DEC_8_ROUNDS(17,16);
      THREEFISH_DEC_8_ROUNDS(15,14);
      THREEFISH_DEC_8_ROUNDS(13,12);
      THREEFISH_DEC_8_ROUNDS(11,10);
      THREEFISH_DEC_8_ROUNDS(9,8);
      THREEFISH_DEC_8_ROUNDS(7,6);
      THREEFISH_DEC_8_ROUNDS(5,4);
      THREEFISH_DEC_8_ROUNDS(3,2);
      THREEFISH_DEC_8_ROUNDS(1,0);

      store_le(out + BLOCK_SIZE*i, X0, X1, X2, X3, X4, X5, X6, X7);
      }

#undef THREEFISH_DEC_8_ROUNDS
#undef THREEFISH_INJECT_KEY
#undef THREEFISH_ROUND
   }

void Threefish_512::set_tweak(const uint8_t tweak[], size_t len)
   {
   BOTAN_ARG_CHECK(len == 16, "Threefish-512 requires 128 bit tweak");

   m_T.resize(3);
   m_T[0] = load_le<uint64_t>(tweak, 0);
   m_T[1] = load_le<uint64_t>(tweak, 1);
   m_T[2] = m_T[0] ^ m_T[1];
   }

void Threefish_512::key_schedule(const uint8_t key[], size_t)
   {
   // todo: define key schedule for smaller keys
   m_K.resize(9);

   for(size_t i = 0; i != 8; ++i)
      m_K[i] = load_le<uint64_t>(key, i);

   m_K[8] = m_K[0] ^ m_K[1] ^ m_K[2] ^ m_K[3] ^
            m_K[4] ^ m_K[5] ^ m_K[6] ^ m_K[7] ^ 0x1BD11BDAA9FC1A22;

   // Reset tweak to all zeros on key reset
   m_T.resize(3);
   zeroise(m_T);
   }

void Threefish_512::clear()
   {
   zap(m_K);
   zap(m_T);
   }

}
/*
* S-Box Tables for Tiger
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

const uint64_t Tiger::SBOX1[256] = {
   0x02AAB17CF7E90C5E, 0xAC424B03E243A8EC, 0x72CD5BE30DD5FCD3,
   0x6D019B93F6F97F3A, 0xCD9978FFD21F9193, 0x7573A1C9708029E2,
   0xB164326B922A83C3, 0x46883EEE04915870, 0xEAACE3057103ECE6,
   0xC54169B808A3535C, 0x4CE754918DDEC47C, 0x0AA2F4DFDC0DF40C,
   0x10B76F18A74DBEFA, 0xC6CCB6235AD1AB6A, 0x13726121572FE2FF,
   0x1A488C6F199D921E, 0x4BC9F9F4DA0007CA, 0x26F5E6F6E85241C7,
   0x859079DBEA5947B6, 0x4F1885C5C99E8C92, 0xD78E761EA96F864B,
   0x8E36428C52B5C17D, 0x69CF6827373063C1, 0xB607C93D9BB4C56E,
   0x7D820E760E76B5EA, 0x645C9CC6F07FDC42, 0xBF38A078243342E0,
   0x5F6B343C9D2E7D04, 0xF2C28AEB600B0EC6, 0x6C0ED85F7254BCAC,
   0x71592281A4DB4FE5, 0x1967FA69CE0FED9F, 0xFD5293F8B96545DB,
   0xC879E9D7F2A7600B, 0x860248920193194E, 0xA4F9533B2D9CC0B3,
   0x9053836C15957613, 0xDB6DCF8AFC357BF1, 0x18BEEA7A7A370F57,
   0x037117CA50B99066, 0x6AB30A9774424A35, 0xF4E92F02E325249B,
   0x7739DB07061CCAE1, 0xD8F3B49CECA42A05, 0xBD56BE3F51382F73,
   0x45FAED5843B0BB28, 0x1C813D5C11BF1F83, 0x8AF0E4B6D75FA169,
   0x33EE18A487AD9999, 0x3C26E8EAB1C94410, 0xB510102BC0A822F9,
   0x141EEF310CE6123B, 0xFC65B90059DDB154, 0xE0158640C5E0E607,
   0x884E079826C3A3CF, 0x930D0D9523C535FD, 0x35638D754E9A2B00,
   0x4085FCCF40469DD5, 0xC4B17AD28BE23A4C, 0xCAB2F0FC6A3E6A2E,
   0x2860971A6B943FCD, 0x3DDE6EE212E30446, 0x6222F32AE01765AE,
   0x5D550BB5478308FE, 0xA9EFA98DA0EDA22A, 0xC351A71686C40DA7,
   0x1105586D9C867C84, 0xDCFFEE85FDA22853, 0xCCFBD0262C5EEF76,
   0xBAF294CB8990D201, 0xE69464F52AFAD975, 0x94B013AFDF133E14,
   0x06A7D1A32823C958, 0x6F95FE5130F61119, 0xD92AB34E462C06C0,
   0xED7BDE33887C71D2, 0x79746D6E6518393E, 0x5BA419385D713329,
   0x7C1BA6B948A97564, 0x31987C197BFDAC67, 0xDE6C23C44B053D02,
   0x581C49FED002D64D, 0xDD474D6338261571, 0xAA4546C3E473D062,
   0x928FCE349455F860, 0x48161BBACAAB94D9, 0x63912430770E6F68,
   0x6EC8A5E602C6641C, 0x87282515337DDD2B, 0x2CDA6B42034B701B,
   0xB03D37C181CB096D, 0xE108438266C71C6F, 0x2B3180C7EB51B255,
   0xDF92B82F96C08BBC, 0x5C68C8C0A632F3BA, 0x5504CC861C3D0556,
   0xABBFA4E55FB26B8F, 0x41848B0AB3BACEB4, 0xB334A273AA445D32,
   0xBCA696F0A85AD881, 0x24F6EC65B528D56C, 0x0CE1512E90F4524A,
   0x4E9DD79D5506D35A, 0x258905FAC6CE9779, 0x2019295B3E109B33,
   0xF8A9478B73A054CC, 0x2924F2F934417EB0, 0x3993357D536D1BC4,
   0x38A81AC21DB6FF8B, 0x47C4FBF17D6016BF, 0x1E0FAADD7667E3F5,
   0x7ABCFF62938BEB96, 0xA78DAD948FC179C9, 0x8F1F98B72911E50D,
   0x61E48EAE27121A91, 0x4D62F7AD31859808, 0xECEBA345EF5CEAEB,
   0xF5CEB25EBC9684CE, 0xF633E20CB7F76221, 0xA32CDF06AB8293E4,
   0x985A202CA5EE2CA4, 0xCF0B8447CC8A8FB1, 0x9F765244979859A3,
   0xA8D516B1A1240017, 0x0BD7BA3EBB5DC726, 0xE54BCA55B86ADB39,
   0x1D7A3AFD6C478063, 0x519EC608E7669EDD, 0x0E5715A2D149AA23,
   0x177D4571848FF194, 0xEEB55F3241014C22, 0x0F5E5CA13A6E2EC2,
   0x8029927B75F5C361, 0xAD139FABC3D6E436, 0x0D5DF1A94CCF402F,
   0x3E8BD948BEA5DFC8, 0xA5A0D357BD3FF77E, 0xA2D12E251F74F645,
   0x66FD9E525E81A082, 0x2E0C90CE7F687A49, 0xC2E8BCBEBA973BC5,
   0x000001BCE509745F, 0x423777BBE6DAB3D6, 0xD1661C7EAEF06EB5,
   0xA1781F354DAACFD8, 0x2D11284A2B16AFFC, 0xF1FC4F67FA891D1F,
   0x73ECC25DCB920ADA, 0xAE610C22C2A12651, 0x96E0A810D356B78A,
   0x5A9A381F2FE7870F, 0xD5AD62EDE94E5530, 0xD225E5E8368D1427,
   0x65977B70C7AF4631, 0x99F889B2DE39D74F, 0x233F30BF54E1D143,
   0x9A9675D3D9A63C97, 0x5470554FF334F9A8, 0x166ACB744A4F5688,
   0x70C74CAAB2E4AEAD, 0xF0D091646F294D12, 0x57B82A89684031D1,
   0xEFD95A5A61BE0B6B, 0x2FBD12E969F2F29A, 0x9BD37013FEFF9FE8,
   0x3F9B0404D6085A06, 0x4940C1F3166CFE15, 0x09542C4DCDF3DEFB,
   0xB4C5218385CD5CE3, 0xC935B7DC4462A641, 0x3417F8A68ED3B63F,
   0xB80959295B215B40, 0xF99CDAEF3B8C8572, 0x018C0614F8FCB95D,
   0x1B14ACCD1A3ACDF3, 0x84D471F200BB732D, 0xC1A3110E95E8DA16,
   0x430A7220BF1A82B8, 0xB77E090D39DF210E, 0x5EF4BD9F3CD05E9D,
   0x9D4FF6DA7E57A444, 0xDA1D60E183D4A5F8, 0xB287C38417998E47,
   0xFE3EDC121BB31886, 0xC7FE3CCC980CCBEF, 0xE46FB590189BFD03,
   0x3732FD469A4C57DC, 0x7EF700A07CF1AD65, 0x59C64468A31D8859,
   0x762FB0B4D45B61F6, 0x155BAED099047718, 0x68755E4C3D50BAA6,
   0xE9214E7F22D8B4DF, 0x2ADDBF532EAC95F4, 0x32AE3909B4BD0109,
   0x834DF537B08E3450, 0xFA209DA84220728D, 0x9E691D9B9EFE23F7,
   0x0446D288C4AE8D7F, 0x7B4CC524E169785B, 0x21D87F0135CA1385,
   0xCEBB400F137B8AA5, 0x272E2B66580796BE, 0x3612264125C2B0DE,
   0x057702BDAD1EFBB2, 0xD4BABB8EACF84BE9, 0x91583139641BC67B,
   0x8BDC2DE08036E024, 0x603C8156F49F68ED, 0xF7D236F7DBEF5111,
   0x9727C4598AD21E80, 0xA08A0896670A5FD7, 0xCB4A8F4309EBA9CB,
   0x81AF564B0F7036A1, 0xC0B99AA778199ABD, 0x959F1EC83FC8E952,
   0x8C505077794A81B9, 0x3ACAAF8F056338F0, 0x07B43F50627A6778,
   0x4A44AB49F5ECCC77, 0x3BC3D6E4B679EE98, 0x9CC0D4D1CF14108C,
   0x4406C00B206BC8A0, 0x82A18854C8D72D89, 0x67E366B35C3C432C,
   0xB923DD61102B37F2, 0x56AB2779D884271D, 0xBE83E1B0FF1525AF,
   0xFB7C65D4217E49A9, 0x6BDBE0E76D48E7D4, 0x08DF828745D9179E,
   0x22EA6A9ADD53BD34, 0xE36E141C5622200A, 0x7F805D1B8CB750EE,
   0xAFE5C7A59F58E837, 0xE27F996A4FB1C23C, 0xD3867DFB0775F0D0,
   0xD0E673DE6E88891A, 0x123AEB9EAFB86C25, 0x30F1D5D5C145B895,
   0xBB434A2DEE7269E7, 0x78CB67ECF931FA38, 0xF33B0372323BBF9C,
   0x52D66336FB279C74, 0x505F33AC0AFB4EAA, 0xE8A5CD99A2CCE187,
   0x534974801E2D30BB, 0x8D2D5711D5876D90, 0x1F1A412891BC038E,
   0xD6E2E71D82E56648, 0x74036C3A497732B7, 0x89B67ED96361F5AB,
   0xFFED95D8F1EA02A2, 0xE72B3BD61464D43D, 0xA6300F170BDC4820,
   0xEBC18760ED78A77A };

const uint64_t Tiger::SBOX2[256] = {
   0xE6A6BE5A05A12138, 0xB5A122A5B4F87C98, 0x563C6089140B6990,
   0x4C46CB2E391F5DD5, 0xD932ADDBC9B79434, 0x08EA70E42015AFF5,
   0xD765A6673E478CF1, 0xC4FB757EAB278D99, 0xDF11C6862D6E0692,
   0xDDEB84F10D7F3B16, 0x6F2EF604A665EA04, 0x4A8E0F0FF0E0DFB3,
   0xA5EDEEF83DBCBA51, 0xFC4F0A2A0EA4371E, 0xE83E1DA85CB38429,
   0xDC8FF882BA1B1CE2, 0xCD45505E8353E80D, 0x18D19A00D4DB0717,
   0x34A0CFEDA5F38101, 0x0BE77E518887CAF2, 0x1E341438B3C45136,
   0xE05797F49089CCF9, 0xFFD23F9DF2591D14, 0x543DDA228595C5CD,
   0x661F81FD99052A33, 0x8736E641DB0F7B76, 0x15227725418E5307,
   0xE25F7F46162EB2FA, 0x48A8B2126C13D9FE, 0xAFDC541792E76EEA,
   0x03D912BFC6D1898F, 0x31B1AAFA1B83F51B, 0xF1AC2796E42AB7D9,
   0x40A3A7D7FCD2EBAC, 0x1056136D0AFBBCC5, 0x7889E1DD9A6D0C85,
   0xD33525782A7974AA, 0xA7E25D09078AC09B, 0xBD4138B3EAC6EDD0,
   0x920ABFBE71EB9E70, 0xA2A5D0F54FC2625C, 0xC054E36B0B1290A3,
   0xF6DD59FF62FE932B, 0x3537354511A8AC7D, 0xCA845E9172FADCD4,
   0x84F82B60329D20DC, 0x79C62CE1CD672F18, 0x8B09A2ADD124642C,
   0xD0C1E96A19D9E726, 0x5A786A9B4BA9500C, 0x0E020336634C43F3,
   0xC17B474AEB66D822, 0x6A731AE3EC9BAAC2, 0x8226667AE0840258,
   0x67D4567691CAECA5, 0x1D94155C4875ADB5, 0x6D00FD985B813FDF,
   0x51286EFCB774CD06, 0x5E8834471FA744AF, 0xF72CA0AEE761AE2E,
   0xBE40E4CDAEE8E09A, 0xE9970BBB5118F665, 0x726E4BEB33DF1964,
   0x703B000729199762, 0x4631D816F5EF30A7, 0xB880B5B51504A6BE,
   0x641793C37ED84B6C, 0x7B21ED77F6E97D96, 0x776306312EF96B73,
   0xAE528948E86FF3F4, 0x53DBD7F286A3F8F8, 0x16CADCE74CFC1063,
   0x005C19BDFA52C6DD, 0x68868F5D64D46AD3, 0x3A9D512CCF1E186A,
   0x367E62C2385660AE, 0xE359E7EA77DCB1D7, 0x526C0773749ABE6E,
   0x735AE5F9D09F734B, 0x493FC7CC8A558BA8, 0xB0B9C1533041AB45,
   0x321958BA470A59BD, 0x852DB00B5F46C393, 0x91209B2BD336B0E5,
   0x6E604F7D659EF19F, 0xB99A8AE2782CCB24, 0xCCF52AB6C814C4C7,
   0x4727D9AFBE11727B, 0x7E950D0C0121B34D, 0x756F435670AD471F,
   0xF5ADD442615A6849, 0x4E87E09980B9957A, 0x2ACFA1DF50AEE355,
   0xD898263AFD2FD556, 0xC8F4924DD80C8FD6, 0xCF99CA3D754A173A,
   0xFE477BACAF91BF3C, 0xED5371F6D690C12D, 0x831A5C285E687094,
   0xC5D3C90A3708A0A4, 0x0F7F903717D06580, 0x19F9BB13B8FDF27F,
   0xB1BD6F1B4D502843, 0x1C761BA38FFF4012, 0x0D1530C4E2E21F3B,
   0x8943CE69A7372C8A, 0xE5184E11FEB5CE66, 0x618BDB80BD736621,
   0x7D29BAD68B574D0B, 0x81BB613E25E6FE5B, 0x071C9C10BC07913F,
   0xC7BEEB7909AC2D97, 0xC3E58D353BC5D757, 0xEB017892F38F61E8,
   0xD4EFFB9C9B1CC21A, 0x99727D26F494F7AB, 0xA3E063A2956B3E03,
   0x9D4A8B9A4AA09C30, 0x3F6AB7D500090FB4, 0x9CC0F2A057268AC0,
   0x3DEE9D2DEDBF42D1, 0x330F49C87960A972, 0xC6B2720287421B41,
   0x0AC59EC07C00369C, 0xEF4EAC49CB353425, 0xF450244EEF0129D8,
   0x8ACC46E5CAF4DEB6, 0x2FFEAB63989263F7, 0x8F7CB9FE5D7A4578,
   0x5BD8F7644E634635, 0x427A7315BF2DC900, 0x17D0C4AA2125261C,
   0x3992486C93518E50, 0xB4CBFEE0A2D7D4C3, 0x7C75D6202C5DDD8D,
   0xDBC295D8E35B6C61, 0x60B369D302032B19, 0xCE42685FDCE44132,
   0x06F3DDB9DDF65610, 0x8EA4D21DB5E148F0, 0x20B0FCE62FCD496F,
   0x2C1B912358B0EE31, 0xB28317B818F5A308, 0xA89C1E189CA6D2CF,
   0x0C6B18576AAADBC8, 0xB65DEAA91299FAE3, 0xFB2B794B7F1027E7,
   0x04E4317F443B5BEB, 0x4B852D325939D0A6, 0xD5AE6BEEFB207FFC,
   0x309682B281C7D374, 0xBAE309A194C3B475, 0x8CC3F97B13B49F05,
   0x98A9422FF8293967, 0x244B16B01076FF7C, 0xF8BF571C663D67EE,
   0x1F0D6758EEE30DA1, 0xC9B611D97ADEB9B7, 0xB7AFD5887B6C57A2,
   0x6290AE846B984FE1, 0x94DF4CDEACC1A5FD, 0x058A5BD1C5483AFF,
   0x63166CC142BA3C37, 0x8DB8526EB2F76F40, 0xE10880036F0D6D4E,
   0x9E0523C9971D311D, 0x45EC2824CC7CD691, 0x575B8359E62382C9,
   0xFA9E400DC4889995, 0xD1823ECB45721568, 0xDAFD983B8206082F,
   0xAA7D29082386A8CB, 0x269FCD4403B87588, 0x1B91F5F728BDD1E0,
   0xE4669F39040201F6, 0x7A1D7C218CF04ADE, 0x65623C29D79CE5CE,
   0x2368449096C00BB1, 0xAB9BF1879DA503BA, 0xBC23ECB1A458058E,
   0x9A58DF01BB401ECC, 0xA070E868A85F143D, 0x4FF188307DF2239E,
   0x14D565B41A641183, 0xEE13337452701602, 0x950E3DCF3F285E09,
   0x59930254B9C80953, 0x3BF299408930DA6D, 0xA955943F53691387,
   0xA15EDECAA9CB8784, 0x29142127352BE9A0, 0x76F0371FFF4E7AFB,
   0x0239F450274F2228, 0xBB073AF01D5E868B, 0xBFC80571C10E96C1,
   0xD267088568222E23, 0x9671A3D48E80B5B0, 0x55B5D38AE193BB81,
   0x693AE2D0A18B04B8, 0x5C48B4ECADD5335F, 0xFD743B194916A1CA,
   0x2577018134BE98C4, 0xE77987E83C54A4AD, 0x28E11014DA33E1B9,
   0x270CC59E226AA213, 0x71495F756D1A5F60, 0x9BE853FB60AFEF77,
   0xADC786A7F7443DBF, 0x0904456173B29A82, 0x58BC7A66C232BD5E,
   0xF306558C673AC8B2, 0x41F639C6B6C9772A, 0x216DEFE99FDA35DA,
   0x11640CC71C7BE615, 0x93C43694565C5527, 0xEA038E6246777839,
   0xF9ABF3CE5A3E2469, 0x741E768D0FD312D2, 0x0144B883CED652C6,
   0xC20B5A5BA33F8552, 0x1AE69633C3435A9D, 0x97A28CA4088CFDEC,
   0x8824A43C1E96F420, 0x37612FA66EEEA746, 0x6B4CB165F9CF0E5A,
   0x43AA1C06A0ABFB4A, 0x7F4DC26FF162796B, 0x6CBACC8E54ED9B0F,
   0xA6B7FFEFD2BB253E, 0x2E25BC95B0A29D4F, 0x86D6A58BDEF1388C,
   0xDED74AC576B6F054, 0x8030BDBC2B45805D, 0x3C81AF70E94D9289,
   0x3EFF6DDA9E3100DB, 0xB38DC39FDFCC8847, 0x123885528D17B87E,
   0xF2DA0ED240B1B642, 0x44CEFADCD54BF9A9, 0x1312200E433C7EE6,
   0x9FFCC84F3A78C748, 0xF0CD1F72248576BB, 0xEC6974053638CFE4,
   0x2BA7B67C0CEC4E4C, 0xAC2F4DF3E5CE32ED, 0xCB33D14326EA4C11,
   0xA4E9044CC77E58BC, 0x5F513293D934FCEF, 0x5DC9645506E55444,
   0x50DE418F317DE40A, 0x388CB31A69DDE259, 0x2DB4A83455820A86,
   0x9010A91E84711AE9, 0x4DF7F0B7B1498371, 0xD62A2EABC0977179,
   0x22FAC097AA8D5C0E };

const uint64_t Tiger::SBOX3[256] = {
   0xF49FCC2FF1DAF39B, 0x487FD5C66FF29281, 0xE8A30667FCDCA83F,
   0x2C9B4BE3D2FCCE63, 0xDA3FF74B93FBBBC2, 0x2FA165D2FE70BA66,
   0xA103E279970E93D4, 0xBECDEC77B0E45E71, 0xCFB41E723985E497,
   0xB70AAA025EF75017, 0xD42309F03840B8E0, 0x8EFC1AD035898579,
   0x96C6920BE2B2ABC5, 0x66AF4163375A9172, 0x2174ABDCCA7127FB,
   0xB33CCEA64A72FF41, 0xF04A4933083066A5, 0x8D970ACDD7289AF5,
   0x8F96E8E031C8C25E, 0xF3FEC02276875D47, 0xEC7BF310056190DD,
   0xF5ADB0AEBB0F1491, 0x9B50F8850FD58892, 0x4975488358B74DE8,
   0xA3354FF691531C61, 0x0702BBE481D2C6EE, 0x89FB24057DEDED98,
   0xAC3075138596E902, 0x1D2D3580172772ED, 0xEB738FC28E6BC30D,
   0x5854EF8F63044326, 0x9E5C52325ADD3BBE, 0x90AA53CF325C4623,
   0xC1D24D51349DD067, 0x2051CFEEA69EA624, 0x13220F0A862E7E4F,
   0xCE39399404E04864, 0xD9C42CA47086FCB7, 0x685AD2238A03E7CC,
   0x066484B2AB2FF1DB, 0xFE9D5D70EFBF79EC, 0x5B13B9DD9C481854,
   0x15F0D475ED1509AD, 0x0BEBCD060EC79851, 0xD58C6791183AB7F8,
   0xD1187C5052F3EEE4, 0xC95D1192E54E82FF, 0x86EEA14CB9AC6CA2,
   0x3485BEB153677D5D, 0xDD191D781F8C492A, 0xF60866BAA784EBF9,
   0x518F643BA2D08C74, 0x8852E956E1087C22, 0xA768CB8DC410AE8D,
   0x38047726BFEC8E1A, 0xA67738B4CD3B45AA, 0xAD16691CEC0DDE19,
   0xC6D4319380462E07, 0xC5A5876D0BA61938, 0x16B9FA1FA58FD840,
   0x188AB1173CA74F18, 0xABDA2F98C99C021F, 0x3E0580AB134AE816,
   0x5F3B05B773645ABB, 0x2501A2BE5575F2F6, 0x1B2F74004E7E8BA9,
   0x1CD7580371E8D953, 0x7F6ED89562764E30, 0xB15926FF596F003D,
   0x9F65293DA8C5D6B9, 0x6ECEF04DD690F84C, 0x4782275FFF33AF88,
   0xE41433083F820801, 0xFD0DFE409A1AF9B5, 0x4325A3342CDB396B,
   0x8AE77E62B301B252, 0xC36F9E9F6655615A, 0x85455A2D92D32C09,
   0xF2C7DEA949477485, 0x63CFB4C133A39EBA, 0x83B040CC6EBC5462,
   0x3B9454C8FDB326B0, 0x56F56A9E87FFD78C, 0x2DC2940D99F42BC6,
   0x98F7DF096B096E2D, 0x19A6E01E3AD852BF, 0x42A99CCBDBD4B40B,
   0xA59998AF45E9C559, 0x366295E807D93186, 0x6B48181BFAA1F773,
   0x1FEC57E2157A0A1D, 0x4667446AF6201AD5, 0xE615EBCACFB0F075,
   0xB8F31F4F68290778, 0x22713ED6CE22D11E, 0x3057C1A72EC3C93B,
   0xCB46ACC37C3F1F2F, 0xDBB893FD02AAF50E, 0x331FD92E600B9FCF,
   0xA498F96148EA3AD6, 0xA8D8426E8B6A83EA, 0xA089B274B7735CDC,
   0x87F6B3731E524A11, 0x118808E5CBC96749, 0x9906E4C7B19BD394,
   0xAFED7F7E9B24A20C, 0x6509EADEEB3644A7, 0x6C1EF1D3E8EF0EDE,
   0xB9C97D43E9798FB4, 0xA2F2D784740C28A3, 0x7B8496476197566F,
   0x7A5BE3E6B65F069D, 0xF96330ED78BE6F10, 0xEEE60DE77A076A15,
   0x2B4BEE4AA08B9BD0, 0x6A56A63EC7B8894E, 0x02121359BA34FEF4,
   0x4CBF99F8283703FC, 0x398071350CAF30C8, 0xD0A77A89F017687A,
   0xF1C1A9EB9E423569, 0x8C7976282DEE8199, 0x5D1737A5DD1F7ABD,
   0x4F53433C09A9FA80, 0xFA8B0C53DF7CA1D9, 0x3FD9DCBC886CCB77,
   0xC040917CA91B4720, 0x7DD00142F9D1DCDF, 0x8476FC1D4F387B58,
   0x23F8E7C5F3316503, 0x032A2244E7E37339, 0x5C87A5D750F5A74B,
   0x082B4CC43698992E, 0xDF917BECB858F63C, 0x3270B8FC5BF86DDA,
   0x10AE72BB29B5DD76, 0x576AC94E7700362B, 0x1AD112DAC61EFB8F,
   0x691BC30EC5FAA427, 0xFF246311CC327143, 0x3142368E30E53206,
   0x71380E31E02CA396, 0x958D5C960AAD76F1, 0xF8D6F430C16DA536,
   0xC8FFD13F1BE7E1D2, 0x7578AE66004DDBE1, 0x05833F01067BE646,
   0xBB34B5AD3BFE586D, 0x095F34C9A12B97F0, 0x247AB64525D60CA8,
   0xDCDBC6F3017477D1, 0x4A2E14D4DECAD24D, 0xBDB5E6D9BE0A1EEB,
   0x2A7E70F7794301AB, 0xDEF42D8A270540FD, 0x01078EC0A34C22C1,
   0xE5DE511AF4C16387, 0x7EBB3A52BD9A330A, 0x77697857AA7D6435,
   0x004E831603AE4C32, 0xE7A21020AD78E312, 0x9D41A70C6AB420F2,
   0x28E06C18EA1141E6, 0xD2B28CBD984F6B28, 0x26B75F6C446E9D83,
   0xBA47568C4D418D7F, 0xD80BADBFE6183D8E, 0x0E206D7F5F166044,
   0xE258A43911CBCA3E, 0x723A1746B21DC0BC, 0xC7CAA854F5D7CDD3,
   0x7CAC32883D261D9C, 0x7690C26423BA942C, 0x17E55524478042B8,
   0xE0BE477656A2389F, 0x4D289B5E67AB2DA0, 0x44862B9C8FBBFD31,
   0xB47CC8049D141365, 0x822C1B362B91C793, 0x4EB14655FB13DFD8,
   0x1ECBBA0714E2A97B, 0x6143459D5CDE5F14, 0x53A8FBF1D5F0AC89,
   0x97EA04D81C5E5B00, 0x622181A8D4FDB3F3, 0xE9BCD341572A1208,
   0x1411258643CCE58A, 0x9144C5FEA4C6E0A4, 0x0D33D06565CF620F,
   0x54A48D489F219CA1, 0xC43E5EAC6D63C821, 0xA9728B3A72770DAF,
   0xD7934E7B20DF87EF, 0xE35503B61A3E86E5, 0xCAE321FBC819D504,
   0x129A50B3AC60BFA6, 0xCD5E68EA7E9FB6C3, 0xB01C90199483B1C7,
   0x3DE93CD5C295376C, 0xAED52EDF2AB9AD13, 0x2E60F512C0A07884,
   0xBC3D86A3E36210C9, 0x35269D9B163951CE, 0x0C7D6E2AD0CDB5FA,
   0x59E86297D87F5733, 0x298EF221898DB0E7, 0x55000029D1A5AA7E,
   0x8BC08AE1B5061B45, 0xC2C31C2B6C92703A, 0x94CC596BAF25EF42,
   0x0A1D73DB22540456, 0x04B6A0F9D9C4179A, 0xEFFDAFA2AE3D3C60,
   0xF7C8075BB49496C4, 0x9CC5C7141D1CD4E3, 0x78BD1638218E5534,
   0xB2F11568F850246A, 0xEDFABCFA9502BC29, 0x796CE5F2DA23051B,
   0xAAE128B0DC93537C, 0x3A493DA0EE4B29AE, 0xB5DF6B2C416895D7,
   0xFCABBD25122D7F37, 0x70810B58105DC4B1, 0xE10FDD37F7882A90,
   0x524DCAB5518A3F5C, 0x3C9E85878451255B, 0x4029828119BD34E2,
   0x74A05B6F5D3CECCB, 0xB610021542E13ECA, 0x0FF979D12F59E2AC,
   0x6037DA27E4F9CC50, 0x5E92975A0DF1847D, 0xD66DE190D3E623FE,
   0x5032D6B87B568048, 0x9A36B7CE8235216E, 0x80272A7A24F64B4A,
   0x93EFED8B8C6916F7, 0x37DDBFF44CCE1555, 0x4B95DB5D4B99BD25,
   0x92D3FDA169812FC0, 0xFB1A4A9A90660BB6, 0x730C196946A4B9B2,
   0x81E289AA7F49DA68, 0x64669A0F83B1A05F, 0x27B3FF7D9644F48B,
   0xCC6B615C8DB675B3, 0x674F20B9BCEBBE95, 0x6F31238275655982,
   0x5AE488713E45CF05, 0xBF619F9954C21157, 0xEABAC46040A8EAE9,
   0x454C6FE9F2C0C1CD, 0x419CF6496412691C, 0xD3DC3BEF265B0F70,
   0x6D0E60F5C3578A9E };

const uint64_t Tiger::SBOX4[256] = {
   0x5B0E608526323C55, 0x1A46C1A9FA1B59F5, 0xA9E245A17C4C8FFA,
   0x65CA5159DB2955D7, 0x05DB0A76CE35AFC2, 0x81EAC77EA9113D45,
   0x528EF88AB6AC0A0D, 0xA09EA253597BE3FF, 0x430DDFB3AC48CD56,
   0xC4B3A67AF45CE46F, 0x4ECECFD8FBE2D05E, 0x3EF56F10B39935F0,
   0x0B22D6829CD619C6, 0x17FD460A74DF2069, 0x6CF8CC8E8510ED40,
   0xD6C824BF3A6ECAA7, 0x61243D581A817049, 0x048BACB6BBC163A2,
   0xD9A38AC27D44CC32, 0x7FDDFF5BAAF410AB, 0xAD6D495AA804824B,
   0xE1A6A74F2D8C9F94, 0xD4F7851235DEE8E3, 0xFD4B7F886540D893,
   0x247C20042AA4BFDA, 0x096EA1C517D1327C, 0xD56966B4361A6685,
   0x277DA5C31221057D, 0x94D59893A43ACFF7, 0x64F0C51CCDC02281,
   0x3D33BCC4FF6189DB, 0xE005CB184CE66AF1, 0xFF5CCD1D1DB99BEA,
   0xB0B854A7FE42980F, 0x7BD46A6A718D4B9F, 0xD10FA8CC22A5FD8C,
   0xD31484952BE4BD31, 0xC7FA975FCB243847, 0x4886ED1E5846C407,
   0x28CDDB791EB70B04, 0xC2B00BE2F573417F, 0x5C9590452180F877,
   0x7A6BDDFFF370EB00, 0xCE509E38D6D9D6A4, 0xEBEB0F00647FA702,
   0x1DCC06CF76606F06, 0xE4D9F28BA286FF0A, 0xD85A305DC918C262,
   0x475B1D8732225F54, 0x2D4FB51668CCB5FE, 0xA679B9D9D72BBA20,
   0x53841C0D912D43A5, 0x3B7EAA48BF12A4E8, 0x781E0E47F22F1DDF,
   0xEFF20CE60AB50973, 0x20D261D19DFFB742, 0x16A12B03062A2E39,
   0x1960EB2239650495, 0x251C16FED50EB8B8, 0x9AC0C330F826016E,
   0xED152665953E7671, 0x02D63194A6369570, 0x5074F08394B1C987,
   0x70BA598C90B25CE1, 0x794A15810B9742F6, 0x0D5925E9FCAF8C6C,
   0x3067716CD868744E, 0x910AB077E8D7731B, 0x6A61BBDB5AC42F61,
   0x93513EFBF0851567, 0xF494724B9E83E9D5, 0xE887E1985C09648D,
   0x34B1D3C675370CFD, 0xDC35E433BC0D255D, 0xD0AAB84234131BE0,
   0x08042A50B48B7EAF, 0x9997C4EE44A3AB35, 0x829A7B49201799D0,
   0x263B8307B7C54441, 0x752F95F4FD6A6CA6, 0x927217402C08C6E5,
   0x2A8AB754A795D9EE, 0xA442F7552F72943D, 0x2C31334E19781208,
   0x4FA98D7CEAEE6291, 0x55C3862F665DB309, 0xBD0610175D53B1F3,
   0x46FE6CB840413F27, 0x3FE03792DF0CFA59, 0xCFE700372EB85E8F,
   0xA7BE29E7ADBCE118, 0xE544EE5CDE8431DD, 0x8A781B1B41F1873E,
   0xA5C94C78A0D2F0E7, 0x39412E2877B60728, 0xA1265EF3AFC9A62C,
   0xBCC2770C6A2506C5, 0x3AB66DD5DCE1CE12, 0xE65499D04A675B37,
   0x7D8F523481BFD216, 0x0F6F64FCEC15F389, 0x74EFBE618B5B13C8,
   0xACDC82B714273E1D, 0xDD40BFE003199D17, 0x37E99257E7E061F8,
   0xFA52626904775AAA, 0x8BBBF63A463D56F9, 0xF0013F1543A26E64,
   0xA8307E9F879EC898, 0xCC4C27A4150177CC, 0x1B432F2CCA1D3348,
   0xDE1D1F8F9F6FA013, 0x606602A047A7DDD6, 0xD237AB64CC1CB2C7,
   0x9B938E7225FCD1D3, 0xEC4E03708E0FF476, 0xFEB2FBDA3D03C12D,
   0xAE0BCED2EE43889A, 0x22CB8923EBFB4F43, 0x69360D013CF7396D,
   0x855E3602D2D4E022, 0x073805BAD01F784C, 0x33E17A133852F546,
   0xDF4874058AC7B638, 0xBA92B29C678AA14A, 0x0CE89FC76CFAADCD,
   0x5F9D4E0908339E34, 0xF1AFE9291F5923B9, 0x6E3480F60F4A265F,
   0xEEBF3A2AB29B841C, 0xE21938A88F91B4AD, 0x57DFEFF845C6D3C3,
   0x2F006B0BF62CAAF2, 0x62F479EF6F75EE78, 0x11A55AD41C8916A9,
   0xF229D29084FED453, 0x42F1C27B16B000E6, 0x2B1F76749823C074,
   0x4B76ECA3C2745360, 0x8C98F463B91691BD, 0x14BCC93CF1ADE66A,
   0x8885213E6D458397, 0x8E177DF0274D4711, 0xB49B73B5503F2951,
   0x10168168C3F96B6B, 0x0E3D963B63CAB0AE, 0x8DFC4B5655A1DB14,
   0xF789F1356E14DE5C, 0x683E68AF4E51DAC1, 0xC9A84F9D8D4B0FD9,
   0x3691E03F52A0F9D1, 0x5ED86E46E1878E80, 0x3C711A0E99D07150,
   0x5A0865B20C4E9310, 0x56FBFC1FE4F0682E, 0xEA8D5DE3105EDF9B,
   0x71ABFDB12379187A, 0x2EB99DE1BEE77B9C, 0x21ECC0EA33CF4523,
   0x59A4D7521805C7A1, 0x3896F5EB56AE7C72, 0xAA638F3DB18F75DC,
   0x9F39358DABE9808E, 0xB7DEFA91C00B72AC, 0x6B5541FD62492D92,
   0x6DC6DEE8F92E4D5B, 0x353F57ABC4BEEA7E, 0x735769D6DA5690CE,
   0x0A234AA642391484, 0xF6F9508028F80D9D, 0xB8E319A27AB3F215,
   0x31AD9C1151341A4D, 0x773C22A57BEF5805, 0x45C7561A07968633,
   0xF913DA9E249DBE36, 0xDA652D9B78A64C68, 0x4C27A97F3BC334EF,
   0x76621220E66B17F4, 0x967743899ACD7D0B, 0xF3EE5BCAE0ED6782,
   0x409F753600C879FC, 0x06D09A39B5926DB6, 0x6F83AEB0317AC588,
   0x01E6CA4A86381F21, 0x66FF3462D19F3025, 0x72207C24DDFD3BFB,
   0x4AF6B6D3E2ECE2EB, 0x9C994DBEC7EA08DE, 0x49ACE597B09A8BC4,
   0xB38C4766CF0797BA, 0x131B9373C57C2A75, 0xB1822CCE61931E58,
   0x9D7555B909BA1C0C, 0x127FAFDD937D11D2, 0x29DA3BADC66D92E4,
   0xA2C1D57154C2ECBC, 0x58C5134D82F6FE24, 0x1C3AE3515B62274F,
   0xE907C82E01CB8126, 0xF8ED091913E37FCB, 0x3249D8F9C80046C9,
   0x80CF9BEDE388FB63, 0x1881539A116CF19E, 0x5103F3F76BD52457,
   0x15B7E6F5AE47F7A8, 0xDBD7C6DED47E9CCF, 0x44E55C410228BB1A,
   0xB647D4255EDB4E99, 0x5D11882BB8AAFC30, 0xF5098BBB29D3212A,
   0x8FB5EA14E90296B3, 0x677B942157DD025A, 0xFB58E7C0A390ACB5,
   0x89D3674C83BD4A01, 0x9E2DA4DF4BF3B93B, 0xFCC41E328CAB4829,
   0x03F38C96BA582C52, 0xCAD1BDBD7FD85DB2, 0xBBB442C16082AE83,
   0xB95FE86BA5DA9AB0, 0xB22E04673771A93F, 0x845358C9493152D8,
   0xBE2A488697B4541E, 0x95A2DC2DD38E6966, 0xC02C11AC923C852B,
   0x2388B1990DF2A87B, 0x7C8008FA1B4F37BE, 0x1F70D0C84D54E503,
   0x5490ADEC7ECE57D4, 0x002B3C27D9063A3A, 0x7EAEA3848030A2BF,
   0xC602326DED2003C0, 0x83A7287D69A94086, 0xC57A5FCB30F57A8A,
   0xB56844E479EBE779, 0xA373B40F05DCBCE9, 0xD71A786E88570EE2,
   0x879CBACDBDE8F6A0, 0x976AD1BCC164A32F, 0xAB21E25E9666D78B,
   0x901063AAE5E5C33C, 0x9818B34448698D90, 0xE36487AE3E1E8ABB,
   0xAFBDF931893BDCB4, 0x6345A0DC5FBBD519, 0x8628FE269B9465CA,
   0x1E5D01603F9C51EC, 0x4DE44006A15049B7, 0xBF6C70E5F776CBB1,
   0x411218F2EF552BED, 0xCB0C0708705A36A3, 0xE74D14754F986044,
   0xCD56D9430EA8280E, 0xC12591D7535F5065, 0xC83223F1720AEF96,
   0xC3A0396F7363A51F };

}
/*
* Tiger
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> Tiger::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new Tiger(*this));
   }

namespace {

/*
* Tiger Mixing Function
*/
inline void mix(secure_vector<uint64_t>& X)
   {
   X[0] -= X[7] ^ 0xA5A5A5A5A5A5A5A5;
   X[1] ^= X[0];
   X[2] += X[1];
   X[3] -= X[2] ^ ((~X[1]) << 19);
   X[4] ^= X[3];
   X[5] += X[4];
   X[6] -= X[5] ^ ((~X[4]) >> 23);
   X[7] ^= X[6];

   X[0] += X[7];
   X[1] -= X[0] ^ ((~X[7]) << 19);
   X[2] ^= X[1];
   X[3] += X[2];
   X[4] -= X[3] ^ ((~X[2]) >> 23);
   X[5] ^= X[4];
   X[6] += X[5];
   X[7] -= X[6] ^ 0x0123456789ABCDEF;
   }

}

/*
* Tiger Compression Function
*/
void Tiger::compress_n(const uint8_t input[], size_t blocks)
   {
   uint64_t A = m_digest[0], B = m_digest[1], C = m_digest[2];

   for(size_t i = 0; i != blocks; ++i)
      {
      load_le(m_X.data(), input, m_X.size());

      pass(A, B, C, m_X, 5); mix(m_X);
      pass(C, A, B, m_X, 7); mix(m_X);
      pass(B, C, A, m_X, 9);

      for(size_t j = 3; j != m_passes; ++j)
         {
         mix(m_X);
         pass(A, B, C, m_X, 9);
         uint64_t T = A; A = C; C = B; B = T;
         }

      A = (m_digest[0] ^= A);
      B = m_digest[1] = B - m_digest[1];
      C = (m_digest[2] += C);

      input += hash_block_size();
      }
   }

/*
* Copy out the digest
*/
void Tiger::copy_out(uint8_t output[])
   {
   copy_out_vec_le(output, output_length(), m_digest);
   }

/*
* Tiger Pass
*/
void Tiger::pass(uint64_t& A, uint64_t& B, uint64_t& C,
                 const secure_vector<uint64_t>& X,
                 uint8_t mul)
   {
   C ^= X[0];
   A -= SBOX1[get_byte(7, C)] ^ SBOX2[get_byte(5, C)] ^
        SBOX3[get_byte(3, C)] ^ SBOX4[get_byte(1, C)];
   B += SBOX1[get_byte(0, C)] ^ SBOX2[get_byte(2, C)] ^
        SBOX3[get_byte(4, C)] ^ SBOX4[get_byte(6, C)];
   B *= mul;

   A ^= X[1];
   B -= SBOX1[get_byte(7, A)] ^ SBOX2[get_byte(5, A)] ^
        SBOX3[get_byte(3, A)] ^ SBOX4[get_byte(1, A)];
   C += SBOX1[get_byte(0, A)] ^ SBOX2[get_byte(2, A)] ^
        SBOX3[get_byte(4, A)] ^ SBOX4[get_byte(6, A)];
   C *= mul;

   B ^= X[2];
   C -= SBOX1[get_byte(7, B)] ^ SBOX2[get_byte(5, B)] ^
        SBOX3[get_byte(3, B)] ^ SBOX4[get_byte(1, B)];
   A += SBOX1[get_byte(0, B)] ^ SBOX2[get_byte(2, B)] ^
        SBOX3[get_byte(4, B)] ^ SBOX4[get_byte(6, B)];
   A *= mul;

   C ^= X[3];
   A -= SBOX1[get_byte(7, C)] ^ SBOX2[get_byte(5, C)] ^
        SBOX3[get_byte(3, C)] ^ SBOX4[get_byte(1, C)];
   B += SBOX1[get_byte(0, C)] ^ SBOX2[get_byte(2, C)] ^
        SBOX3[get_byte(4, C)] ^ SBOX4[get_byte(6, C)];
   B *= mul;

   A ^= X[4];
   B -= SBOX1[get_byte(7, A)] ^ SBOX2[get_byte(5, A)] ^
        SBOX3[get_byte(3, A)] ^ SBOX4[get_byte(1, A)];
   C += SBOX1[get_byte(0, A)] ^ SBOX2[get_byte(2, A)] ^
        SBOX3[get_byte(4, A)] ^ SBOX4[get_byte(6, A)];
   C *= mul;

   B ^= X[5];
   C -= SBOX1[get_byte(7, B)] ^ SBOX2[get_byte(5, B)] ^
        SBOX3[get_byte(3, B)] ^ SBOX4[get_byte(1, B)];
   A += SBOX1[get_byte(0, B)] ^ SBOX2[get_byte(2, B)] ^
        SBOX3[get_byte(4, B)] ^ SBOX4[get_byte(6, B)];
   A *= mul;

   C ^= X[6];
   A -= SBOX1[get_byte(7, C)] ^ SBOX2[get_byte(5, C)] ^
        SBOX3[get_byte(3, C)] ^ SBOX4[get_byte(1, C)];
   B += SBOX1[get_byte(0, C)] ^ SBOX2[get_byte(2, C)] ^
        SBOX3[get_byte(4, C)] ^ SBOX4[get_byte(6, C)];
   B *= mul;

   A ^= X[7];
   B -= SBOX1[get_byte(7, A)] ^ SBOX2[get_byte(5, A)] ^
        SBOX3[get_byte(3, A)] ^ SBOX4[get_byte(1, A)];
   C += SBOX1[get_byte(0, A)] ^ SBOX2[get_byte(2, A)] ^
        SBOX3[get_byte(4, A)] ^ SBOX4[get_byte(6, A)];
   C *= mul;
   }

/*
* Clear memory of sensitive data
*/
void Tiger::clear()
   {
   MDx_HashFunction::clear();
   zeroise(m_X);
   m_digest[0] = 0x0123456789ABCDEF;
   m_digest[1] = 0xFEDCBA9876543210;
   m_digest[2] = 0xF096A5B4C3B2E187;
   }

/*
* Return the name of this type
*/
std::string Tiger::name() const
   {
   return "Tiger(" + std::to_string(output_length()) + "," +
                     std::to_string(m_passes) + ")";
   }

/*
* Tiger Constructor
*/
Tiger::Tiger(size_t hash_len, size_t passes) :
   MDx_HashFunction(64, false, false),
   m_X(8),
   m_digest(3),
   m_hash_len(hash_len),
   m_passes(passes)
   {
   if(output_length() != 16 && output_length() != 20 && output_length() != 24)
      throw Invalid_Argument("Tiger: Illegal hash output size: " +
                             std::to_string(output_length()));

   if(passes < 3)
      throw Invalid_Argument("Tiger: Invalid number of passes: "
                             + std::to_string(passes));
   clear();
   }

}
/*
* Runtime assertion checking
* (C) 2010,2012,2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void throw_invalid_argument(const char* message,
                            const char* func,
                            const char* file)
   {
   std::ostringstream format;
   format << message << " in " << func << ":" << file;
   throw Invalid_Argument(format.str());
   }

void throw_invalid_state(const char* expr,
                         const char* func,
                         const char* file)
   {
   std::ostringstream format;
   format << "Invalid state: " << expr << " was false in " << func << ":" << file;
   throw Invalid_State(format.str());
   }

void assertion_failure(const char* expr_str,
                       const char* assertion_made,
                       const char* func,
                       const char* file,
                       int line)
   {
   std::ostringstream format;

   format << "False assertion ";

   if(assertion_made && assertion_made[0] != 0)
      format << "'" << assertion_made << "' (expression " << expr_str << ") ";
   else
      format << expr_str << " ";

   if(func)
      format << "in " << func << " ";

   format << "@" << file << ":" << line;

   throw Internal_Error(format.str());
   }

}
/*
* Calendar Functions
* (C) 1999-2010,2017 Jack Lloyd
* (C) 2015 Simon Warta (Kullo GmbH)
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <ctime>
#include <iomanip>
#include <stdlib.h>

namespace Botan {

namespace {

std::tm do_gmtime(std::time_t time_val)
   {
   std::tm tm;

#if defined(BOTAN_TARGET_OS_HAS_WIN32)
   ::gmtime_s(&tm, &time_val); // Windows
#elif defined(BOTAN_TARGET_OS_HAS_POSIX1)
   ::gmtime_r(&time_val, &tm); // Unix/SUSv2
#else
   std::tm* tm_p = std::gmtime(&time_val);
   if (tm_p == nullptr)
      throw Encoding_Error("time_t_to_tm could not convert");
   tm = *tm_p;
#endif

   return tm;
   }

/*
Portable replacement for timegm, _mkgmtime, etc

Algorithm due to Howard Hinnant

See https://howardhinnant.github.io/date_algorithms.html#days_from_civil
for details and explaination. The code is slightly simplified by our assumption
that the date is at least 1970, which is sufficient for our purposes.
*/
size_t days_since_epoch(uint32_t year, uint32_t month, uint32_t day)
   {
   if(month <= 2)
      year -= 1;
   const uint32_t era = year / 400;
   const uint32_t yoe = year - era * 400;      // [0, 399]
   const uint32_t doy = (153*(month + (month > 2 ? -3 : 9)) + 2)/5 + day-1;  // [0, 365]
   const uint32_t doe = yoe * 365 + yoe/4 - yoe/100 + doy;         // [0, 146096]
   return era * 146097 + doe - 719468;
   }

}

std::chrono::system_clock::time_point calendar_point::to_std_timepoint() const
   {
   if(get_year() < 1970)
      throw Invalid_Argument("calendar_point::to_std_timepoint() does not support years before 1970");

   // 32 bit time_t ends at January 19, 2038
   // https://msdn.microsoft.com/en-us/library/2093ets1.aspx
   // Throw after 2037 if 32 bit time_t is used

   BOTAN_IF_CONSTEXPR(sizeof(std::time_t) == 4)
      {
      if(get_year() > 2037)
         {
         throw Invalid_Argument("calendar_point::to_std_timepoint() does not support years after 2037 on this system");
         }
      }

   // This upper bound is completely arbitrary
   if(get_year() >= 2400)
      {
      throw Invalid_Argument("calendar_point::to_std_timepoint() does not support years after 2400");
      }

   const uint64_t seconds_64 = (days_since_epoch(get_year(), get_month(), get_day()) * 86400) +
                                (get_hour() * 60 * 60) + (get_minutes() * 60) + get_seconds();

   const time_t seconds_time_t = static_cast<time_t>(seconds_64);

   if(seconds_64 - seconds_time_t != 0)
      {
      throw Invalid_Argument("calendar_point::to_std_timepoint time_t overflow");
      }

   return std::chrono::system_clock::from_time_t(seconds_time_t);
   }

std::string calendar_point::to_string() const
   {
   // desired format: <YYYY>-<MM>-<dd>T<HH>:<mm>:<ss>
   std::stringstream output;
   output << std::setfill('0')
          << std::setw(4) << get_year() << "-"
          << std::setw(2) << get_month() << "-"
          << std::setw(2) << get_day() << "T"
          << std::setw(2) << get_hour() << ":"
          << std::setw(2) << get_minutes() << ":"
          << std::setw(2) << get_seconds();
   return output.str();
   }


calendar_point calendar_value(
   const std::chrono::system_clock::time_point& time_point)
   {
   std::tm tm = do_gmtime(std::chrono::system_clock::to_time_t(time_point));

   return calendar_point(tm.tm_year + 1900,
                         tm.tm_mon + 1,
                         tm.tm_mday,
                         tm.tm_hour,
                         tm.tm_min,
                         tm.tm_sec);
   }

}
/*
* Character Set Handling
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <cctype>

namespace Botan {

namespace {

void append_utf8_for(std::string& s, uint32_t c)
   {
   if(c >= 0xD800 && c < 0xE000)
      throw Decoding_Error("Invalid Unicode character");

   if(c <= 0x7F)
      {
      const uint8_t b0 = static_cast<uint8_t>(c);
      s.push_back(static_cast<char>(b0));
      }
   else if(c <= 0x7FF)
      {
      const uint8_t b0 = 0xC0 | static_cast<uint8_t>(c >> 6);
      const uint8_t b1 = 0x80 | static_cast<uint8_t>(c & 0x3F);
      s.push_back(static_cast<char>(b0));
      s.push_back(static_cast<char>(b1));
      }
   else if(c <= 0xFFFF)
      {
      const uint8_t b0 = 0xE0 | static_cast<uint8_t>(c >> 12);
      const uint8_t b1 = 0x80 | static_cast<uint8_t>((c >> 6) & 0x3F);
      const uint8_t b2 = 0x80 | static_cast<uint8_t>(c & 0x3F);
      s.push_back(static_cast<char>(b0));
      s.push_back(static_cast<char>(b1));
      s.push_back(static_cast<char>(b2));
      }
   else if(c <= 0x10FFFF)
      {
      const uint8_t b0 = 0xF0 | static_cast<uint8_t>(c >> 18);
      const uint8_t b1 = 0x80 | static_cast<uint8_t>((c >> 12) & 0x3F);
      const uint8_t b2 = 0x80 | static_cast<uint8_t>((c >> 6) & 0x3F);
      const uint8_t b3 = 0x80 | static_cast<uint8_t>(c & 0x3F);
      s.push_back(static_cast<char>(b0));
      s.push_back(static_cast<char>(b1));
      s.push_back(static_cast<char>(b2));
      s.push_back(static_cast<char>(b3));
      }
   else
      throw Decoding_Error("Invalid Unicode character");

   }

}

std::string ucs2_to_utf8(const uint8_t ucs2[], size_t len)
   {
   if(len % 2 != 0)
      throw Decoding_Error("Invalid length for UCS-2 string");

   const size_t chars = len / 2;

   std::string s;
   for(size_t i = 0; i != chars; ++i)
      {
      const uint16_t c = load_be<uint16_t>(ucs2, i);
      append_utf8_for(s, c);
      }

   return s;
   }

std::string ucs4_to_utf8(const uint8_t ucs4[], size_t len)
   {
   if(len % 4 != 0)
      throw Decoding_Error("Invalid length for UCS-4 string");

   const size_t chars = len / 4;

   std::string s;
   for(size_t i = 0; i != chars; ++i)
      {
      const uint32_t c = load_be<uint32_t>(ucs4, i);
      append_utf8_for(s, c);
      }

   return s;
   }

/*
* Convert from UTF-8 to ISO 8859-1
*/
std::string utf8_to_latin1(const std::string& utf8)
   {
   std::string iso8859;

   size_t position = 0;
   while(position != utf8.size())
      {
      const uint8_t c1 = static_cast<uint8_t>(utf8[position++]);

      if(c1 <= 0x7F)
         {
         iso8859 += static_cast<char>(c1);
         }
      else if(c1 >= 0xC0 && c1 <= 0xC7)
         {
         if(position == utf8.size())
            throw Decoding_Error("UTF-8: sequence truncated");

         const uint8_t c2 = static_cast<uint8_t>(utf8[position++]);
         const uint8_t iso_char = ((c1 & 0x07) << 6) | (c2 & 0x3F);

         if(iso_char <= 0x7F)
            throw Decoding_Error("UTF-8: sequence longer than needed");

         iso8859 += static_cast<char>(iso_char);
         }
      else
         throw Decoding_Error("UTF-8: Unicode chars not in Latin1 used");
      }

   return iso8859;
   }

namespace Charset {

namespace {

/*
* Convert from UCS-2 to ISO 8859-1
*/
std::string ucs2_to_latin1(const std::string& ucs2)
   {
   if(ucs2.size() % 2 == 1)
      throw Decoding_Error("UCS-2 string has an odd number of bytes");

   std::string latin1;

   for(size_t i = 0; i != ucs2.size(); i += 2)
      {
      const uint8_t c1 = ucs2[i];
      const uint8_t c2 = ucs2[i+1];

      if(c1 != 0)
         throw Decoding_Error("UCS-2 has non-Latin1 characters");

      latin1 += static_cast<char>(c2);
      }

   return latin1;
   }

/*
* Convert from ISO 8859-1 to UTF-8
*/
std::string latin1_to_utf8(const std::string& iso8859)
   {
   std::string utf8;
   for(size_t i = 0; i != iso8859.size(); ++i)
      {
      const uint8_t c = static_cast<uint8_t>(iso8859[i]);

      if(c <= 0x7F)
         utf8 += static_cast<char>(c);
      else
         {
         utf8 += static_cast<char>((0xC0 | (c >> 6)));
         utf8 += static_cast<char>((0x80 | (c & 0x3F)));
         }
      }
   return utf8;
   }

}

/*
* Perform character set transcoding
*/
std::string transcode(const std::string& str,
                      Character_Set to, Character_Set from)
   {
   if(to == LOCAL_CHARSET)
      to = LATIN1_CHARSET;
   if(from == LOCAL_CHARSET)
      from = LATIN1_CHARSET;

   if(to == from)
      return str;

   if(from == LATIN1_CHARSET && to == UTF8_CHARSET)
      return latin1_to_utf8(str);
   if(from == UTF8_CHARSET && to == LATIN1_CHARSET)
      return utf8_to_latin1(str);
   if(from == UCS2_CHARSET && to == LATIN1_CHARSET)
      return ucs2_to_latin1(str);

   throw Invalid_Argument("Unknown transcoding operation from " +
                          std::to_string(from) + " to " + std::to_string(to));
   }

/*
* Check if a character represents a digit
*/
bool is_digit(char c)
   {
   if(c == '0' || c == '1' || c == '2' || c == '3' || c == '4' ||
      c == '5' || c == '6' || c == '7' || c == '8' || c == '9')
      return true;
   return false;
   }

/*
* Check if a character represents whitespace
*/
bool is_space(char c)
   {
   if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
      return true;
   return false;
   }

/*
* Convert a character to a digit
*/
uint8_t char2digit(char c)
   {
   switch(c)
      {
      case '0': return 0;
      case '1': return 1;
      case '2': return 2;
      case '3': return 3;
      case '4': return 4;
      case '5': return 5;
      case '6': return 6;
      case '7': return 7;
      case '8': return 8;
      case '9': return 9;
      }

   throw Invalid_Argument("char2digit: Input is not a digit character");
   }

/*
* Convert a digit to a character
*/
char digit2char(uint8_t b)
   {
   switch(b)
      {
      case 0: return '0';
      case 1: return '1';
      case 2: return '2';
      case 3: return '3';
      case 4: return '4';
      case 5: return '5';
      case 6: return '6';
      case 7: return '7';
      case 8: return '8';
      case 9: return '9';
      }

   throw Invalid_Argument("digit2char: Input is not a digit");
   }

/*
* Case-insensitive character comparison
*/
bool caseless_cmp(char a, char b)
   {
   return (std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b)));
   }

}

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

namespace CT {

secure_vector<uint8_t> copy_output(CT::Mask<uint8_t> bad_input,
                                   const uint8_t input[],
                                   size_t input_length,
                                   size_t offset)
   {
   if(input_length == 0)
      return secure_vector<uint8_t>();

   /*
   * Ensure at runtime that offset <= input_length. This is an invalid input,
   * but we can't throw without using the poisoned value. Instead, if it happens,
   * set offset to be equal to the input length (so output_bytes becomes 0 and
   * the returned vector is empty)
   */
   const auto valid_offset = CT::Mask<size_t>::is_lte(offset, input_length);
   offset = valid_offset.select(offset, input_length);

   const size_t output_bytes = input_length - offset;

   secure_vector<uint8_t> output(input_length);

   /*
   Move the desired output bytes to the front using a slow (O^n)
   but constant time loop that does not leak the value of the offset
   */
   for(size_t i = 0; i != input_length; ++i)
      {
      /*
      start index from i rather than 0 since we know j must be >= i + offset
      to have any effect, and starting from i does not reveal information
      */
      for(size_t j = i; j != input_length; ++j)
         {
         const uint8_t b = input[j];
         const auto is_eq = CT::Mask<size_t>::is_equal(j, offset + i);
         output[i] |= is_eq.if_set_return(b);
         }
      }

   bad_input.if_set_zero_out(output.data(), output.size());

   CT::unpoison(output.data(), output.size());
   CT::unpoison(output_bytes);

   /*
   This is potentially not const time, depending on how std::vector is
   implemented. But since we are always reducing length, it should
   just amount to setting the member var holding the length.
   */
   output.resize(output_bytes);
   return output;
   }

secure_vector<uint8_t> strip_leading_zeros(const uint8_t in[], size_t length)
   {
   size_t leading_zeros = 0;

   auto only_zeros = Mask<uint8_t>::set();

   for(size_t i = 0; i != length; ++i)
      {
      only_zeros &= CT::Mask<uint8_t>::is_zero(in[i]);
      leading_zeros += only_zeros.if_set_return(1);
      }

   return copy_output(CT::Mask<uint8_t>::cleared(), in, length, leading_zeros);
   }

}

}
/*
* DataSource
* (C) 1999-2007 Jack Lloyd
*     2005 Matthew Gregan
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <istream>

#if defined(BOTAN_TARGET_OS_HAS_FILESYSTEM)
  #include <fstream>
#endif

namespace Botan {

/*
* Read a single byte from the DataSource
*/
size_t DataSource::read_byte(uint8_t& out)
   {
   return read(&out, 1);
   }

/*
* Peek a single byte from the DataSource
*/
size_t DataSource::peek_byte(uint8_t& out) const
   {
   return peek(&out, 1, 0);
   }

/*
* Discard the next N bytes of the data
*/
size_t DataSource::discard_next(size_t n)
   {
   uint8_t buf[64] = { 0 };
   size_t discarded = 0;

   while(n)
      {
      const size_t got = this->read(buf, std::min(n, sizeof(buf)));
      discarded += got;
      n -= got;

      if(got == 0)
         break;
      }

   return discarded;
   }

/*
* Read from a memory buffer
*/
size_t DataSource_Memory::read(uint8_t out[], size_t length)
   {
   const size_t got = std::min<size_t>(m_source.size() - m_offset, length);
   copy_mem(out, m_source.data() + m_offset, got);
   m_offset += got;
   return got;
   }

bool DataSource_Memory::check_available(size_t n)
   {
   return (n <= (m_source.size() - m_offset));
   }

/*
* Peek into a memory buffer
*/
size_t DataSource_Memory::peek(uint8_t out[], size_t length,
                               size_t peek_offset) const
   {
   const size_t bytes_left = m_source.size() - m_offset;
   if(peek_offset >= bytes_left) return 0;

   const size_t got = std::min(bytes_left - peek_offset, length);
   copy_mem(out, &m_source[m_offset + peek_offset], got);
   return got;
   }

/*
* Check if the memory buffer is empty
*/
bool DataSource_Memory::end_of_data() const
   {
   return (m_offset == m_source.size());
   }

/*
* DataSource_Memory Constructor
*/
DataSource_Memory::DataSource_Memory(const std::string& in) :
   m_source(cast_char_ptr_to_uint8(in.data()),
            cast_char_ptr_to_uint8(in.data()) + in.length()),
   m_offset(0)
   {
   }

/*
* Read from a stream
*/
size_t DataSource_Stream::read(uint8_t out[], size_t length)
   {
   m_source.read(cast_uint8_ptr_to_char(out), length);
   if(m_source.bad())
      throw Stream_IO_Error("DataSource_Stream::read: Source failure");

   const size_t got = static_cast<size_t>(m_source.gcount());
   m_total_read += got;
   return got;
   }

bool DataSource_Stream::check_available(size_t n)
   {
   const std::streampos orig_pos = m_source.tellg();
   m_source.seekg(0, std::ios::end);
   const size_t avail = static_cast<size_t>(m_source.tellg() - orig_pos);
   m_source.seekg(orig_pos);
   return (avail >= n);
   }

/*
* Peek into a stream
*/
size_t DataSource_Stream::peek(uint8_t out[], size_t length, size_t offset) const
   {
   if(end_of_data())
      throw Invalid_State("DataSource_Stream: Cannot peek when out of data");

   size_t got = 0;

   if(offset)
      {
      secure_vector<uint8_t> buf(offset);
      m_source.read(cast_uint8_ptr_to_char(buf.data()), buf.size());
      if(m_source.bad())
         throw Stream_IO_Error("DataSource_Stream::peek: Source failure");
      got = static_cast<size_t>(m_source.gcount());
      }

   if(got == offset)
      {
      m_source.read(cast_uint8_ptr_to_char(out), length);
      if(m_source.bad())
         throw Stream_IO_Error("DataSource_Stream::peek: Source failure");
      got = static_cast<size_t>(m_source.gcount());
      }

   if(m_source.eof())
      m_source.clear();
   m_source.seekg(m_total_read, std::ios::beg);

   return got;
   }

/*
* Check if the stream is empty or in error
*/
bool DataSource_Stream::end_of_data() const
   {
   return (!m_source.good());
   }

/*
* Return a human-readable ID for this stream
*/
std::string DataSource_Stream::id() const
   {
   return m_identifier;
   }

#if defined(BOTAN_TARGET_OS_HAS_FILESYSTEM)

/*
* DataSource_Stream Constructor
*/
DataSource_Stream::DataSource_Stream(const std::string& path,
                                     bool use_binary) :
   m_identifier(path),
   m_source_memory(new std::ifstream(path, use_binary ? std::ios::binary : std::ios::in)),
   m_source(*m_source_memory),
   m_total_read(0)
   {
   if(!m_source.good())
      {
      throw Stream_IO_Error("DataSource: Failure opening file " + path);
      }
   }

#endif

/*
* DataSource_Stream Constructor
*/
DataSource_Stream::DataSource_Stream(std::istream& in,
                                     const std::string& name) :
   m_identifier(name),
   m_source(in),
   m_total_read(0)
   {
   }

DataSource_Stream::~DataSource_Stream()
   {
   // for ~unique_ptr
   }

}
/*
* (C) 2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::string to_string(ErrorType type)
   {
   switch(type)
      {
      case ErrorType::Unknown:
         return "Unknown";
      case ErrorType::SystemError:
         return "SystemError";
      case ErrorType::NotImplemented:
         return "NotImplemented";
      case ErrorType::OutOfMemory:
         return "OutOfMemory";
      case ErrorType::InternalError:
         return "InternalError";
      case ErrorType::IoError:
         return "IoError";
      case ErrorType::InvalidObjectState :
         return "InvalidObjectState";
      case ErrorType::KeyNotSet:
         return "KeyNotSet";
      case ErrorType::InvalidArgument:
         return "InvalidArgument";
      case ErrorType::InvalidKeyLength:
         return "InvalidKeyLength";
      case ErrorType::InvalidNonceLength:
         return "InvalidNonceLength";
      case ErrorType::LookupError:
         return "LookupError";
      case ErrorType::EncodingFailure:
         return "EncodingFailure";
      case ErrorType::DecodingFailure:
         return "DecodingFailure";
      case ErrorType::TLSError:
         return "TLSError";
      case ErrorType::HttpError:
         return "HttpError";
      case ErrorType::InvalidTag:
         return "InvalidTag";
      case ErrorType::RoughtimeError:
         return "RoughtimeError";
      case ErrorType::OpenSSLError :
         return "OpenSSLError";
      case ErrorType::CommonCryptoError:
         return "CommonCryptoError";
      case ErrorType::Pkcs11Error:
         return "Pkcs11Error";
      case ErrorType::TPMError:
         return "TPMError";
      case ErrorType::DatabaseError:
         return "DatabaseError";
      case ErrorType::ZlibError :
         return "ZlibError";
      case ErrorType::Bzip2Error:
         return "Bzip2Error" ;
      case ErrorType::LzmaError:
         return "LzmaError";
      }

   // No default case in above switch so compiler warns
   return "Unrecognized Botan error";
   }

Exception::Exception(const std::string& msg) : m_msg(msg)
   {}

Exception::Exception(const std::string& msg, const std::exception& e) :
   m_msg(msg + " failed with " + std::string(e.what()))
   {}

Exception::Exception(const char* prefix, const std::string& msg) :
   m_msg(std::string(prefix) + " " + msg)
   {}

Invalid_Argument::Invalid_Argument(const std::string& msg) :
   Exception(msg)
   {}

Invalid_Argument::Invalid_Argument(const std::string& msg, const std::string& where) :
   Exception(msg + " in " + where)
   {}

Invalid_Argument::Invalid_Argument(const std::string& msg, const std::exception& e) :
   Exception(msg, e) {}

Lookup_Error::Lookup_Error(const std::string& type,
                           const std::string& algo,
                           const std::string& provider) :
   Exception("Unavailable " + type + " " + algo +
             (provider.empty() ? std::string("") : (" for provider " + provider)))
   {}

Internal_Error::Internal_Error(const std::string& err) :
   Exception("Internal error: " + err)
   {}

Invalid_Key_Length::Invalid_Key_Length(const std::string& name, size_t length) :
   Invalid_Argument(name + " cannot accept a key of length " +
                    std::to_string(length))
   {}

Invalid_IV_Length::Invalid_IV_Length(const std::string& mode, size_t bad_len) :
   Invalid_Argument("IV length " + std::to_string(bad_len) +
                    " is invalid for " + mode)
   {}

Key_Not_Set::Key_Not_Set(const std::string& algo) :
   Invalid_State("Key not set in " + algo)
   {}

Policy_Violation::Policy_Violation(const std::string& err) :
   Invalid_State("Policy violation: " + err) {}

PRNG_Unseeded::PRNG_Unseeded(const std::string& algo) :
   Invalid_State("PRNG not seeded: " + algo)
   {}

Algorithm_Not_Found::Algorithm_Not_Found(const std::string& name) :
   Lookup_Error("Could not find any algorithm named \"" + name + "\"")
   {}

No_Provider_Found::No_Provider_Found(const std::string& name) :
   Exception("Could not find any provider for algorithm named \"" + name + "\"")
   {}

Provider_Not_Found::Provider_Not_Found(const std::string& algo, const std::string& provider) :
   Lookup_Error("Could not find provider '" + provider + "' for " + algo)
   {}

Invalid_Algorithm_Name::Invalid_Algorithm_Name(const std::string& name):
   Invalid_Argument("Invalid algorithm name: " + name)
   {}

Encoding_Error::Encoding_Error(const std::string& name) :
   Invalid_Argument("Encoding error: " + name)
   {}

Decoding_Error::Decoding_Error(const std::string& name) :
   Invalid_Argument(name)
   {}

Decoding_Error::Decoding_Error(const std::string& msg, const std::exception& e) :
   Invalid_Argument(msg, e)
   {}

Decoding_Error::Decoding_Error(const std::string& name, const char* exception_message) :
   Invalid_Argument(name + " failed with exception " + exception_message) {}

Invalid_Authentication_Tag::Invalid_Authentication_Tag(const std::string& msg) :
   Exception("Invalid authentication tag: " + msg)
   {}

Invalid_OID::Invalid_OID(const std::string& oid) :
   Decoding_Error("Invalid ASN.1 OID: " + oid)
   {}

Stream_IO_Error::Stream_IO_Error(const std::string& err) :
   Exception("I/O error: " + err)
   {}

System_Error::System_Error(const std::string& msg, int err_code) :
   Exception(msg + " error code " + std::to_string(err_code)),
   m_error_code(err_code)
   {}

Self_Test_Failure::Self_Test_Failure(const std::string& err) :
   Internal_Error("Self test failed: " + err)
   {}

Not_Implemented::Not_Implemented(const std::string& err) :
   Exception("Not implemented", err)
   {}

}
/*
* (C) 2015,2017,2019 Jack Lloyd
* (C) 2015 Simon Warta (Kullo GmbH)
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <deque>
#include <memory>

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <dirent.h>
  #include <functional>
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
  #define NOMINMAX 1
  #define _WINSOCKAPI_ // stop windows.h including winsock.h
  #include <windows.h>
#endif

namespace Botan {

namespace {

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)

std::vector<std::string> impl_readdir(const std::string& dir_path)
   {
   std::vector<std::string> out;
   std::deque<std::string> dir_list;
   dir_list.push_back(dir_path);

   while(!dir_list.empty())
      {
      const std::string cur_path = dir_list[0];
      dir_list.pop_front();

      std::unique_ptr<DIR, std::function<int (DIR*)>> dir(::opendir(cur_path.c_str()), ::closedir);

      if(dir)
         {
         while(struct dirent* dirent = ::readdir(dir.get()))
            {
            const std::string filename = dirent->d_name;
            if(filename == "." || filename == "..")
               continue;
            const std::string full_path = cur_path + "/" + filename;

            struct stat stat_buf;

            if(::stat(full_path.c_str(), &stat_buf) == -1)
               continue;

            if(S_ISDIR(stat_buf.st_mode))
               dir_list.push_back(full_path);
            else if(S_ISREG(stat_buf.st_mode))
               out.push_back(full_path);
            }
         }
      }

   return out;
   }

#elif defined(BOTAN_TARGET_OS_HAS_WIN32)

std::vector<std::string> impl_win32(const std::string& dir_path)
   {
   std::vector<std::string> out;
   std::deque<std::string> dir_list;
   dir_list.push_back(dir_path);

   while(!dir_list.empty())
      {
      const std::string cur_path = dir_list[0];
      dir_list.pop_front();

      WIN32_FIND_DATAA find_data;
      HANDLE dir = ::FindFirstFileA((cur_path + "/*").c_str(), &find_data);

      if(dir != INVALID_HANDLE_VALUE)
         {
         do
            {
            const std::string filename = find_data.cFileName;
            if(filename == "." || filename == "..")
               continue;
            const std::string full_path = cur_path + "/" + filename;

            if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
               {
               dir_list.push_back(full_path);
               }
            else
               {
               out.push_back(full_path);
               }
            }
         while(::FindNextFileA(dir, &find_data));
         }

      ::FindClose(dir);
      }

   return out;
}
#endif

}

bool has_filesystem_impl()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   return true;
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
   return true;
#else
   return false;
#endif
   }

std::vector<std::string> get_files_recursive(const std::string& dir)
   {
   std::vector<std::string> files;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   files = impl_readdir(dir);
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
   files = impl_win32(dir);
#else
   BOTAN_UNUSED(dir);
   throw No_Filesystem_Access();
#endif

   std::sort(files.begin(), files.end());

   return files;
   }

}
/*
* (C) 2017 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <cstdlib>
#include <new>

#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
#endif

namespace Botan {

BOTAN_MALLOC_FN void* allocate_memory(size_t elems, size_t elem_size)
   {
   if(elems == 0 || elem_size == 0)
      return nullptr;

#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
   if(void* p = mlock_allocator::instance().allocate(elems, elem_size))
      return p;
#endif

   void* ptr = std::calloc(elems, elem_size);
   if(!ptr)
      throw std::bad_alloc();
   return ptr;
   }

void deallocate_memory(void* p, size_t elems, size_t elem_size)
   {
   if(p == nullptr)
      return;

   secure_scrub_memory(p, elems * elem_size);

#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
   if(mlock_allocator::instance().deallocate(p, elems, elem_size))
      return;
#endif

   std::free(p);
   }

void initialize_allocator()
   {
#if defined(BOTAN_HAS_LOCKING_ALLOCATOR)
   mlock_allocator::instance();
#endif
   }

uint8_t ct_compare_u8(const uint8_t x[],
                      const uint8_t y[],
                      size_t len)
   {
   volatile uint8_t difference = 0;

   for(size_t i = 0; i != len; ++i)
      difference |= (x[i] ^ y[i]);

   return CT::Mask<uint8_t>::is_zero(difference).value();
   }

}
/*
* OS and machine specific utility functions
* (C) 2015,2016,2017,2018 Jack Lloyd
* (C) 2016 Daniel Neus
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


#include <chrono>

#if defined(BOTAN_TARGET_OS_HAS_THREADS)
  #include <thread>
#endif

#if defined(BOTAN_TARGET_OS_HAS_EXPLICIT_BZERO)
  #include <string.h>
#endif

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
  #include <sys/types.h>
  #include <sys/resource.h>
  #include <sys/mman.h>
  #include <signal.h>
  #include <setjmp.h>
  #include <unistd.h>
  #include <errno.h>
  #include <termios.h>
  #undef B0
#endif

#if defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)
  #include <emscripten/emscripten.h>
#endif

#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL) || defined(BOTAN_TARGET_OS_IS_ANDROID) || \
  defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
  #include <sys/auxv.h>
#endif

#if defined(BOTAN_TARGET_OS_HAS_WIN32)
  #define NOMINMAX 1
  #define _WINSOCKAPI_ // stop windows.h including winsock.h
  #include <windows.h>
#endif

#if defined(BOTAN_TARGET_OS_IS_ANDROID)
  #include <elf.h>
  extern "C" char **environ;
#endif

#if defined(BOTAN_TARGET_OS_IS_IOS) || defined(BOTAN_TARGET_OS_IS_MACOS)
  #include <mach/vm_statistics.h>
#endif

namespace Botan {

// Not defined in OS namespace for historical reasons
void secure_scrub_memory(void* ptr, size_t n)
   {
#if defined(BOTAN_TARGET_OS_HAS_RTLSECUREZEROMEMORY)
   ::RtlSecureZeroMemory(ptr, n);

#elif defined(BOTAN_TARGET_OS_HAS_EXPLICIT_BZERO)
   ::explicit_bzero(ptr, n);

#elif defined(BOTAN_TARGET_OS_HAS_EXPLICIT_MEMSET)
   (void)::explicit_memset(ptr, 0, n);

#elif defined(BOTAN_USE_VOLATILE_MEMSET_FOR_ZERO) && (BOTAN_USE_VOLATILE_MEMSET_FOR_ZERO == 1)
   /*
   Call memset through a static volatile pointer, which the compiler
   should not elide. This construct should be safe in conforming
   compilers, but who knows. I did confirm that on x86-64 GCC 6.1 and
   Clang 3.8 both create code that saves the memset address in the
   data segment and unconditionally loads and jumps to that address.
   */
   static void* (*const volatile memset_ptr)(void*, int, size_t) = std::memset;
   (memset_ptr)(ptr, 0, n);
#else

   volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(ptr);

   for(size_t i = 0; i != n; ++i)
      p[i] = 0;
#endif
   }

uint32_t OS::get_process_id()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   return ::getpid();
#elif defined(BOTAN_TARGET_OS_HAS_WIN32)
   return ::GetCurrentProcessId();
#elif defined(BOTAN_TARGET_OS_IS_INCLUDEOS) || defined(BOTAN_TARGET_OS_IS_LLVM) || defined(BOTAN_TARGET_OS_IS_NONE)
   return 0; // truly no meaningful value
#else
   #error "Missing get_process_id"
#endif
   }

unsigned long OS::get_auxval(unsigned long id)
   {
#if defined(BOTAN_TARGET_OS_HAS_GETAUXVAL)
   return ::getauxval(id);
#elif defined(BOTAN_TARGET_OS_IS_ANDROID) && defined(BOTAN_TARGET_ARCH_IS_ARM32)

   if(id == 0)
      return 0;

   char **p = environ;

   while(*p++ != nullptr)
      ;

   Elf32_auxv_t *e = reinterpret_cast<Elf32_auxv_t*>(p);

   while(e != nullptr)
      {
      if(e->a_type == id)
         return e->a_un.a_val;
      e++;
      }

   return 0;
#elif defined(BOTAN_TARGET_OS_HAS_ELF_AUX_INFO)
   unsigned long auxinfo = 0;
   ::elf_aux_info(id, &auxinfo, sizeof(auxinfo));
   return auxinfo;
#else
   BOTAN_UNUSED(id);
   return 0;
#endif
   }

bool OS::running_in_privileged_state()
   {
#if defined(AT_SECURE)
   return OS::get_auxval(AT_SECURE) != 0;
#elif defined(BOTAN_TARGET_OS_HAS_POSIX1)
   return (::getuid() != ::geteuid()) || (::getgid() != ::getegid());
#else
   return false;
#endif
   }

uint64_t OS::get_cpu_cycle_counter()
   {
   uint64_t rtc = 0;

#if defined(BOTAN_TARGET_OS_HAS_WIN32)
   LARGE_INTEGER tv;
   ::QueryPerformanceCounter(&tv);
   rtc = tv.QuadPart;

#elif defined(BOTAN_USE_GCC_INLINE_ASM)

#if defined(BOTAN_TARGET_CPU_IS_X86_FAMILY)

   if(CPUID::has_rdtsc())
      {
      uint32_t rtc_low = 0, rtc_high = 0;
      asm volatile("rdtsc" : "=d" (rtc_high), "=a" (rtc_low));
      rtc = (static_cast<uint64_t>(rtc_high) << 32) | rtc_low;
      }

#elif defined(BOTAN_TARGET_ARCH_IS_PPC64)

   for(;;)
      {
      uint32_t rtc_low = 0, rtc_high = 0, rtc_high2 = 0;
      asm volatile("mftbu %0" : "=r" (rtc_high));
      asm volatile("mftb %0" : "=r" (rtc_low));
      asm volatile("mftbu %0" : "=r" (rtc_high2));

      if(rtc_high == rtc_high2)
         {
         rtc = (static_cast<uint64_t>(rtc_high) << 32) | rtc_low;
         break;
         }
      }

#elif defined(BOTAN_TARGET_ARCH_IS_ALPHA)
   asm volatile("rpcc %0" : "=r" (rtc));

   // OpenBSD does not trap access to the %tick register
#elif defined(BOTAN_TARGET_ARCH_IS_SPARC64) && !defined(BOTAN_TARGET_OS_IS_OPENBSD)
   asm volatile("rd %%tick, %0" : "=r" (rtc));

#elif defined(BOTAN_TARGET_ARCH_IS_IA64)
   asm volatile("mov %0=ar.itc" : "=r" (rtc));

#elif defined(BOTAN_TARGET_ARCH_IS_S390X)
   asm volatile("stck 0(%0)" : : "a" (&rtc) : "memory", "cc");

#elif defined(BOTAN_TARGET_ARCH_IS_HPPA)
   asm volatile("mfctl 16,%0" : "=r" (rtc)); // 64-bit only?

#else
   //#warning "OS::get_cpu_cycle_counter not implemented"
#endif

#endif

   return rtc;
   }

size_t OS::get_cpu_total()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(_SC_NPROCESSORS_CONF)
   const long res = ::sysconf(_SC_NPROCESSORS_CONF);
   if(res > 0)
      return static_cast<size_t>(res);
#endif

#if defined(BOTAN_TARGET_OS_HAS_THREADS)
   return static_cast<size_t>(std::thread::hardware_concurrency());
#else
   return 1;
#endif
   }

size_t OS::get_cpu_available()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(_SC_NPROCESSORS_ONLN)
   const long res = ::sysconf(_SC_NPROCESSORS_ONLN);
   if(res > 0)
      return static_cast<size_t>(res);
#endif

   return OS::get_cpu_total();
   }

uint64_t OS::get_high_resolution_clock()
   {
   if(uint64_t cpu_clock = OS::get_cpu_cycle_counter())
      return cpu_clock;

#if defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)
   return emscripten_get_now();
#endif

   /*
   If we got here either we either don't have an asm instruction
   above, or (for x86) RDTSC is not available at runtime. Try some
   clock_gettimes and return the first one that works, or otherwise
   fall back to std::chrono.
   */

#if defined(BOTAN_TARGET_OS_HAS_CLOCK_GETTIME)

   // The ordering here is somewhat arbitrary...
   const clockid_t clock_types[] = {
#if defined(CLOCK_MONOTONIC_HR)
      CLOCK_MONOTONIC_HR,
#endif
#if defined(CLOCK_MONOTONIC_RAW)
      CLOCK_MONOTONIC_RAW,
#endif
#if defined(CLOCK_MONOTONIC)
      CLOCK_MONOTONIC,
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
      CLOCK_PROCESS_CPUTIME_ID,
#endif
#if defined(CLOCK_THREAD_CPUTIME_ID)
      CLOCK_THREAD_CPUTIME_ID,
#endif
   };

   for(clockid_t clock : clock_types)
      {
      struct timespec ts;
      if(::clock_gettime(clock, &ts) == 0)
         {
         return (static_cast<uint64_t>(ts.tv_sec) * 1000000000) + static_cast<uint64_t>(ts.tv_nsec);
         }
      }
#endif

   // Plain C++11 fallback
   auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
   }

uint64_t OS::get_system_timestamp_ns()
   {
#if defined(BOTAN_TARGET_OS_HAS_CLOCK_GETTIME)
   struct timespec ts;
   if(::clock_gettime(CLOCK_REALTIME, &ts) == 0)
      {
      return (static_cast<uint64_t>(ts.tv_sec) * 1000000000) + static_cast<uint64_t>(ts.tv_nsec);
      }
#endif

   auto now = std::chrono::system_clock::now().time_since_epoch();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
   }

size_t OS::system_page_size()
   {
   const size_t default_page_size = 4096;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   long p = ::sysconf(_SC_PAGESIZE);
   if(p > 1)
      return static_cast<size_t>(p);
   else
      return default_page_size;
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   BOTAN_UNUSED(default_page_size);
   SYSTEM_INFO sys_info;
   ::GetSystemInfo(&sys_info);
   return sys_info.dwPageSize;
#else
   return default_page_size;
#endif
   }

size_t OS::get_memory_locking_limit()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK) && defined(RLIMIT_MEMLOCK)
   /*
   * If RLIMIT_MEMLOCK is not defined, likely the OS does not support
   * unprivileged mlock calls.
   *
   * Linux defaults to only 64 KiB of mlockable memory per process
   * (too small) but BSDs offer a small fraction of total RAM (more
   * than we need). Bound the total mlock size to 512 KiB which is
   * enough to run the entire test suite without spilling to non-mlock
   * memory (and thus presumably also enough for many useful
   * programs), but small enough that we should not cause problems
   * even if many processes are mlocking on the same machine.
   */
   const size_t user_req = read_env_variable_sz("BOTAN_MLOCK_POOL_SIZE", BOTAN_MLOCK_ALLOCATOR_MAX_LOCKED_KB);

   const size_t mlock_requested = std::min<size_t>(user_req, BOTAN_MLOCK_ALLOCATOR_MAX_LOCKED_KB);

   if(mlock_requested > 0)
      {
      struct ::rlimit limits;

      ::getrlimit(RLIMIT_MEMLOCK, &limits);

      if(limits.rlim_cur < limits.rlim_max)
         {
         limits.rlim_cur = limits.rlim_max;
         ::setrlimit(RLIMIT_MEMLOCK, &limits);
         ::getrlimit(RLIMIT_MEMLOCK, &limits);
         }

      return std::min<size_t>(limits.rlim_cur, mlock_requested * 1024);
      }

#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   SIZE_T working_min = 0, working_max = 0;
   if(!::GetProcessWorkingSetSize(::GetCurrentProcess(), &working_min, &working_max))
      {
      return 0;
      }

   // According to Microsoft MSDN:
   // The maximum number of pages that a process can lock is equal to the number of pages in its minimum working set minus a small overhead
   // In the book "Windows Internals Part 2": the maximum lockable pages are minimum working set size - 8 pages
   // But the information in the book seems to be inaccurate/outdated
   // I've tested this on Windows 8.1 x64, Windows 10 x64 and Windows 7 x86
   // On all three OS the value is 11 instead of 8
   const size_t overhead = OS::system_page_size() * 11;
   if(working_min > overhead)
      {
      const size_t lockable_bytes = working_min - overhead;
      return std::min<size_t>(lockable_bytes, BOTAN_MLOCK_ALLOCATOR_MAX_LOCKED_KB * 1024);
      }
#endif

   // Not supported on this platform
   return 0;
   }

bool OS::read_env_variable(std::string& value_out, const std::string& name)
   {
   value_out = "";

   if(running_in_privileged_state())
      return false;

#if defined(BOTAN_TARGET_OS_HAS_WIN32) && defined(BOTAN_BUILD_COMPILER_IS_MSVC)
   char val[128] = { 0 };
   size_t req_size = 0;
   if(getenv_s(&req_size, val, sizeof(val), name.c_str()) == 0)
      {
      value_out = std::string(val, req_size);
      return true;
      }
#else
   if(const char* val = std::getenv(name.c_str()))
      {
      value_out = val;
      return true;
      }
#endif

   return false;
   }

size_t OS::read_env_variable_sz(const std::string& name, size_t def)
   {
   std::string value;
   if(read_env_variable(value, name))
      {
      try
         {
         const size_t val = std::stoul(value, nullptr);
         return val;
         }
      catch(std::exception&) { /* ignore it */ }
      }

   return def;
   }

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)

namespace {

int get_locked_fd()
   {
#if defined(BOTAN_TARGET_OS_IS_IOS) || defined(BOTAN_TARGET_OS_IS_MACOS)
   // On Darwin, tagging anonymous pages allows vmmap to track these.
   // Allowed from 240 to 255 for userland applications
   static constexpr int default_locked_fd = 255;
   int locked_fd = default_locked_fd;

   if(size_t locked_fdl = OS::read_env_variable_sz("BOTAN_LOCKED_FD", default_locked_fd))
      {
      if(locked_fdl < 240 || locked_fdl > 255)
         {
         locked_fdl = default_locked_fd;
         }
      locked_fd = static_cast<int>(locked_fdl);
      }
   return VM_MAKE_TAG(locked_fd);
#else
   return -1;
#endif
   }

}

#endif

std::vector<void*> OS::allocate_locked_pages(size_t count)
   {
   std::vector<void*> result;

#if (defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)) || defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)

   result.reserve(count);

   const size_t page_size = OS::system_page_size();

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)
   static const int locked_fd = get_locked_fd();
#endif

   for(size_t i = 0; i != count; ++i)
      {
      void* ptr = nullptr;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)

#if !defined(MAP_ANONYMOUS)
   #define MAP_ANONYMOUS MAP_ANON
#endif

#if !defined(MAP_NOCORE)
#if defined(MAP_CONCEAL)
   #define MAP_NOCORE MAP_CONCEAL
#else
   #define MAP_NOCORE 0
#endif
#endif

#if !defined(PROT_MAX)
   #define PROT_MAX(p) 0
#endif
      const int pflags = PROT_READ | PROT_WRITE;

      ptr = ::mmap(nullptr, 3*page_size,
                   pflags | PROT_MAX(pflags),
                   MAP_ANONYMOUS | MAP_PRIVATE | MAP_NOCORE,
                   /*fd=*/locked_fd, /*offset=*/0);

      if(ptr == MAP_FAILED)
         {
         continue;
         }

      // lock the data page
      if(::mlock(static_cast<uint8_t*>(ptr) + page_size, page_size) != 0)
         {
         ::munmap(ptr, 3*page_size);
         continue;
         }

#if defined(MADV_DONTDUMP)
      // we ignore errors here, as DONTDUMP is just a bonus
      ::madvise(static_cast<uint8_t*>(ptr) + page_size, page_size, MADV_DONTDUMP);
#endif

#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
      ptr = ::VirtualAlloc(nullptr, 3*page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

      if(ptr == nullptr)
         continue;

      if(::VirtualLock(static_cast<uint8_t*>(ptr) + page_size, page_size) == 0)
         {
         ::VirtualFree(ptr, 0, MEM_RELEASE);
         continue;
         }
#endif

      std::memset(ptr, 0, 3*page_size); // zero data page and both guard pages

      // Make guard page preceeding the data page
      page_prohibit_access(static_cast<uint8_t*>(ptr));
      // Make guard page following the data page
      page_prohibit_access(static_cast<uint8_t*>(ptr) + 2*page_size);

      result.push_back(static_cast<uint8_t*>(ptr) + page_size);
      }
#else
   BOTAN_UNUSED(count);
#endif

   return result;
   }

void OS::page_allow_access(void* page)
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   const size_t page_size = OS::system_page_size();
   ::mprotect(page, page_size, PROT_READ | PROT_WRITE);
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   const size_t page_size = OS::system_page_size();
   DWORD old_perms = 0;
   ::VirtualProtect(page, page_size, PAGE_READWRITE, &old_perms);
   BOTAN_UNUSED(old_perms);
#else
   BOTAN_UNUSED(page);
#endif
   }

void OS::page_prohibit_access(void* page)
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   const size_t page_size = OS::system_page_size();
   ::mprotect(page, page_size, PROT_NONE);
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
   const size_t page_size = OS::system_page_size();
   DWORD old_perms = 0;
   ::VirtualProtect(page, page_size, PAGE_NOACCESS, &old_perms);
   BOTAN_UNUSED(old_perms);
#else
   BOTAN_UNUSED(page);
#endif
   }

void OS::free_locked_pages(const std::vector<void*>& pages)
   {
   const size_t page_size = OS::system_page_size();

   for(size_t i = 0; i != pages.size(); ++i)
      {
      void* ptr = pages[i];

      secure_scrub_memory(ptr, page_size);

      // ptr points to the data page, guard pages are before and after
      page_allow_access(static_cast<uint8_t*>(ptr) - page_size);
      page_allow_access(static_cast<uint8_t*>(ptr) + page_size);

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && defined(BOTAN_TARGET_OS_HAS_POSIX_MLOCK)
      ::munlock(ptr, page_size);
      ::munmap(static_cast<uint8_t*>(ptr) - page_size, 3*page_size);
#elif defined(BOTAN_TARGET_OS_HAS_VIRTUAL_LOCK)
      ::VirtualUnlock(ptr, page_size);
      ::VirtualFree(static_cast<uint8_t*>(ptr) - page_size, 0, MEM_RELEASE);
#endif
      }
   }

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && !defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)

namespace {

static ::sigjmp_buf g_sigill_jmp_buf;

void botan_sigill_handler(int)
   {
   siglongjmp(g_sigill_jmp_buf, /*non-zero return value*/1);
   }

}

#endif

int OS::run_cpu_instruction_probe(std::function<int ()> probe_fn)
   {
   volatile int probe_result = -3;

#if defined(BOTAN_TARGET_OS_HAS_POSIX1) && !defined(BOTAN_TARGET_OS_IS_EMSCRIPTEN)
   struct sigaction old_sigaction;
   struct sigaction sigaction;

   sigaction.sa_handler = botan_sigill_handler;
   sigemptyset(&sigaction.sa_mask);
   sigaction.sa_flags = 0;

   int rc = ::sigaction(SIGILL, &sigaction, &old_sigaction);

   if(rc != 0)
      throw System_Error("run_cpu_instruction_probe sigaction failed", errno);

   rc = sigsetjmp(g_sigill_jmp_buf, /*save sigs*/1);

   if(rc == 0)
      {
      // first call to sigsetjmp
      probe_result = probe_fn();
      }
   else if(rc == 1)
      {
      // non-local return from siglongjmp in signal handler: return error
      probe_result = -1;
      }

   // Restore old SIGILL handler, if any
   rc = ::sigaction(SIGILL, &old_sigaction, nullptr);
   if(rc != 0)
      throw System_Error("run_cpu_instruction_probe sigaction restore failed", errno);

#else
   BOTAN_UNUSED(probe_fn);
#endif

   return probe_result;
   }

std::unique_ptr<OS::Echo_Suppression> OS::suppress_echo_on_terminal()
   {
#if defined(BOTAN_TARGET_OS_HAS_POSIX1)
   class POSIX_Echo_Suppression : public Echo_Suppression
      {
      public:
         POSIX_Echo_Suppression()
            {
            m_stdin_fd = fileno(stdin);
            if(::tcgetattr(m_stdin_fd, &m_old_termios) != 0)
               throw System_Error("Getting terminal status failed", errno);

            struct termios noecho_flags = m_old_termios;
            noecho_flags.c_lflag &= ~ECHO;
            noecho_flags.c_lflag |= ECHONL;

            if(::tcsetattr(m_stdin_fd, TCSANOW, &noecho_flags) != 0)
               throw System_Error("Clearing terminal echo bit failed", errno);
            }

         void reenable_echo() override
            {
            if(m_stdin_fd > 0)
               {
               if(::tcsetattr(m_stdin_fd, TCSANOW, &m_old_termios) != 0)
                  throw System_Error("Restoring terminal echo bit failed", errno);
               m_stdin_fd = -1;
               }
            }

         ~POSIX_Echo_Suppression()
            {
            try
               {
               reenable_echo();
               }
            catch(...)
               {
               }
            }

      private:
         int m_stdin_fd;
         struct termios m_old_termios;
      };

   return std::unique_ptr<Echo_Suppression>(new POSIX_Echo_Suppression);

#elif defined(BOTAN_TARGET_OS_HAS_WIN32)

   class Win32_Echo_Suppression : public Echo_Suppression
      {
      public:
         Win32_Echo_Suppression()
            {
            m_input_handle = ::GetStdHandle(STD_INPUT_HANDLE);
            if(::GetConsoleMode(m_input_handle, &m_console_state) == 0)
               throw System_Error("Getting console mode failed", ::GetLastError());

            DWORD new_mode = ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
            if(::SetConsoleMode(m_input_handle, new_mode) == 0)
               throw System_Error("Setting console mode failed", ::GetLastError());
            }

         void reenable_echo() override
            {
            if(m_input_handle != INVALID_HANDLE_VALUE)
               {
               if(::SetConsoleMode(m_input_handle, m_console_state) == 0)
                  throw System_Error("Setting console mode failed", ::GetLastError());
               m_input_handle = INVALID_HANDLE_VALUE;
               }
            }

         ~Win32_Echo_Suppression()
            {
            try
               {
               reenable_echo();
               }
            catch(...)
               {
               }
            }

      private:
         HANDLE m_input_handle;
         DWORD m_console_state;
      };

   return std::unique_ptr<Echo_Suppression>(new Win32_Echo_Suppression);

#else

   // Not supported on this platform, return null
   return std::unique_ptr<Echo_Suppression>();
#endif
   }

}
/*
* Various string utils and parsing functions
* (C) 1999-2007,2013,2014,2015,2018 Jack Lloyd
* (C) 2015 Simon Warta (Kullo GmbH)
* (C) 2017 René Korthaus, Rohde & Schwarz Cybersecurity
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <limits>
#include <set>

#if defined(BOTAN_HAS_ASN1)
#endif

namespace Botan {

uint16_t to_uint16(const std::string& str)
   {
   const uint32_t x = to_u32bit(str);

   if(x >> 16)
      throw Invalid_Argument("Integer value exceeds 16 bit range");

   return static_cast<uint16_t>(x);
   }

uint32_t to_u32bit(const std::string& str)
   {
   // std::stoul is not strict enough. Ensure that str is digit only [0-9]*
   for(const char chr : str)
      {
      if(chr < '0' || chr > '9')
         {
         std::string chrAsString(1, chr);
         throw Invalid_Argument("String contains non-digit char: " + chrAsString);
         }
      }

   const unsigned long int x = std::stoul(str);

   if(sizeof(unsigned long int) > 4)
      {
      // x might be uint64
      if (x > std::numeric_limits<uint32_t>::max())
         {
         throw Invalid_Argument("Integer value of " + str + " exceeds 32 bit range");
         }
      }

   return static_cast<uint32_t>(x);
   }

/*
* Convert a string into a time duration
*/
uint32_t timespec_to_u32bit(const std::string& timespec)
   {
   if(timespec.empty())
      return 0;

   const char suffix = timespec[timespec.size()-1];
   std::string value = timespec.substr(0, timespec.size()-1);

   uint32_t scale = 1;

   if(Charset::is_digit(suffix))
      value += suffix;
   else if(suffix == 's')
      scale = 1;
   else if(suffix == 'm')
      scale = 60;
   else if(suffix == 'h')
      scale = 60 * 60;
   else if(suffix == 'd')
      scale = 24 * 60 * 60;
   else if(suffix == 'y')
      scale = 365 * 24 * 60 * 60;
   else
      throw Decoding_Error("timespec_to_u32bit: Bad input " + timespec);

   return scale * to_u32bit(value);
   }

/*
* Parse a SCAN-style algorithm name
*/
std::vector<std::string> parse_algorithm_name(const std::string& namex)
   {
   if(namex.find('(') == std::string::npos &&
      namex.find(')') == std::string::npos)
      return std::vector<std::string>(1, namex);

   std::string name = namex, substring;
   std::vector<std::string> elems;
   size_t level = 0;

   elems.push_back(name.substr(0, name.find('(')));
   name = name.substr(name.find('('));

   for(auto i = name.begin(); i != name.end(); ++i)
      {
      char c = *i;

      if(c == '(')
         ++level;
      if(c == ')')
         {
         if(level == 1 && i == name.end() - 1)
            {
            if(elems.size() == 1)
               elems.push_back(substring.substr(1));
            else
               elems.push_back(substring);
            return elems;
            }

         if(level == 0 || (level == 1 && i != name.end() - 1))
            throw Invalid_Algorithm_Name(namex);
         --level;
         }

      if(c == ',' && level == 1)
         {
         if(elems.size() == 1)
            elems.push_back(substring.substr(1));
         else
            elems.push_back(substring);
         substring.clear();
         }
      else
         substring += c;
      }

   if(!substring.empty())
      throw Invalid_Algorithm_Name(namex);

   return elems;
   }

std::vector<std::string> split_on(const std::string& str, char delim)
   {
   return split_on_pred(str, [delim](char c) { return c == delim; });
   }

std::vector<std::string> split_on_pred(const std::string& str,
                                       std::function<bool (char)> pred)
   {
   std::vector<std::string> elems;
   if(str.empty()) return elems;

   std::string substr;
   for(auto i = str.begin(); i != str.end(); ++i)
      {
      if(pred(*i))
         {
         if(!substr.empty())
            elems.push_back(substr);
         substr.clear();
         }
      else
         substr += *i;
      }

   if(substr.empty())
      throw Invalid_Argument("Unable to split string: " + str);
   elems.push_back(substr);

   return elems;
   }

/*
* Join a string
*/
std::string string_join(const std::vector<std::string>& strs, char delim)
   {
   std::string out = "";

   for(size_t i = 0; i != strs.size(); ++i)
      {
      if(i != 0)
         out += delim;
      out += strs[i];
      }

   return out;
   }

/*
* Parse an ASN.1 OID string
*/
std::vector<uint32_t> parse_asn1_oid(const std::string& oid)
   {
#if defined(BOTAN_HAS_ASN1)
   return OID(oid).get_components();
#else
   BOTAN_UNUSED(oid);
   throw Not_Implemented("ASN1 support not available");
#endif
   }

/*
* X.500 String Comparison
*/
bool x500_name_cmp(const std::string& name1, const std::string& name2)
   {
   auto p1 = name1.begin();
   auto p2 = name2.begin();

   while((p1 != name1.end()) && Charset::is_space(*p1)) ++p1;
   while((p2 != name2.end()) && Charset::is_space(*p2)) ++p2;

   while(p1 != name1.end() && p2 != name2.end())
      {
      if(Charset::is_space(*p1))
         {
         if(!Charset::is_space(*p2))
            return false;

         while((p1 != name1.end()) && Charset::is_space(*p1)) ++p1;
         while((p2 != name2.end()) && Charset::is_space(*p2)) ++p2;

         if(p1 == name1.end() && p2 == name2.end())
            return true;
         if(p1 == name1.end() || p2 == name2.end())
            return false;
         }

      if(!Charset::caseless_cmp(*p1, *p2))
         return false;
      ++p1;
      ++p2;
      }

   while((p1 != name1.end()) && Charset::is_space(*p1)) ++p1;
   while((p2 != name2.end()) && Charset::is_space(*p2)) ++p2;

   if((p1 != name1.end()) || (p2 != name2.end()))
      return false;
   return true;
   }

/*
* Convert a decimal-dotted string to binary IP
*/
uint32_t string_to_ipv4(const std::string& str)
   {
   std::vector<std::string> parts = split_on(str, '.');

   if(parts.size() != 4)
      throw Decoding_Error("Invalid IP string " + str);

   uint32_t ip = 0;

   for(auto part = parts.begin(); part != parts.end(); ++part)
      {
      uint32_t octet = to_u32bit(*part);

      if(octet > 255)
         throw Decoding_Error("Invalid IP string " + str);

      ip = (ip << 8) | (octet & 0xFF);
      }

   return ip;
   }

/*
* Convert an IP address to decimal-dotted string
*/
std::string ipv4_to_string(uint32_t ip)
   {
   std::string str;

   for(size_t i = 0; i != sizeof(ip); ++i)
      {
      if(i)
         str += ".";
      str += std::to_string(get_byte(i, ip));
      }

   return str;
   }

std::string erase_chars(const std::string& str, const std::set<char>& chars)
   {
   std::string out;

   for(auto c: str)
      if(chars.count(c) == 0)
         out += c;

   return out;
   }

std::string replace_chars(const std::string& str,
                          const std::set<char>& chars,
                          char to_char)
   {
   std::string out = str;

   for(size_t i = 0; i != out.size(); ++i)
      if(chars.count(out[i]))
         out[i] = to_char;

   return out;
   }

std::string replace_char(const std::string& str, char from_char, char to_char)
   {
   std::string out = str;

   for(size_t i = 0; i != out.size(); ++i)
      if(out[i] == from_char)
         out[i] = to_char;

   return out;
   }

std::string tolower_string(const std::string& in)
   {
   std::string s = in;
   for(size_t i = 0; i != s.size(); ++i)
      {
      const int cu = static_cast<unsigned char>(s[i]);
      if(std::isalpha(cu))
         s[i] = static_cast<char>(std::tolower(cu));
      }
   return s;
   }

bool host_wildcard_match(const std::string& issued_, const std::string& host_)
   {
   const std::string issued = tolower_string(issued_);
   const std::string host = tolower_string(host_);

   if(host.empty() || issued.empty())
      return false;

   /*
   If there are embedded nulls in your issued name
   Well I feel bad for you son
   */
   if(std::count(issued.begin(), issued.end(), char(0)) > 0)
      return false;

   // If more than one wildcard, then issued name is invalid
   const size_t stars = std::count(issued.begin(), issued.end(), '*');
   if(stars > 1)
      return false;

   // '*' is not a valid character in DNS names so should not appear on the host side
   if(std::count(host.begin(), host.end(), '*') != 0)
      return false;

   // Similarly a DNS name can't end in .
   if(host[host.size() - 1] == '.')
      return false;

   // And a host can't have an empty name component, so reject that
   if(host.find("..") != std::string::npos)
      return false;

   // Exact match: accept
   if(issued == host)
      {
      return true;
      }

   /*
   Otherwise it might be a wildcard

   If the issued size is strictly longer than the hostname size it
   couldn't possibly be a match, even if the issued value is a
   wildcard. The only exception is when the wildcard ends up empty
   (eg www.example.com matches www*.example.com)
   */
   if(issued.size() > host.size() + 1)
      {
      return false;
      }

   // If no * at all then not a wildcard, and so not a match
   if(stars != 1)
      {
      return false;
      }

   /*
   Now walk through the issued string, making sure every character
   matches. When we come to the (singular) '*', jump forward in the
   hostname by the corresponding amount. We know exactly how much
   space the wildcard takes because it must be exactly `len(host) -
   len(issued) + 1 chars`.

   We also verify that the '*' comes in the leftmost component, and
   doesn't skip over any '.' in the hostname.
   */
   size_t dots_seen = 0;
   size_t host_idx = 0;

   for(size_t i = 0; i != issued.size(); ++i)
      {
      dots_seen += (issued[i] == '.');

      if(issued[i] == '*')
         {
         // Fail: wildcard can only come in leftmost component
         if(dots_seen > 0)
            {
            return false;
            }

         /*
         Since there is only one * we know the tail of the issued and
         hostname must be an exact match. In this case advance host_idx
         to match.
         */
         const size_t advance = (host.size() - issued.size() + 1);

         if(host_idx + advance > host.size()) // shouldn't happen
            return false;

         // Can't be any intervening .s that we would have skipped
         if(std::count(host.begin() + host_idx,
                       host.begin() + host_idx + advance, '.') != 0)
            return false;

         host_idx += advance;
         }
      else
         {
         if(issued[i] != host[host_idx])
            {
            return false;
            }

         host_idx += 1;
         }
      }

   // Wildcard issued name must have at least 3 components
   if(dots_seen < 2)
      {
      return false;
      }

   return true;
   }

}
/*
* Simple config/test file reader
* (C) 2013,2014,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::string clean_ws(const std::string& s)
   {
   const char* ws = " \t\n";
   auto start = s.find_first_not_of(ws);
   auto end = s.find_last_not_of(ws);

   if(start == std::string::npos)
      return "";

   if(end == std::string::npos)
      return s.substr(start, end);
   else
      return s.substr(start, start + end + 1);
   }

std::map<std::string, std::string> read_cfg(std::istream& is)
   {
   std::map<std::string, std::string> kv;
   size_t line = 0;

   while(is.good())
      {
      std::string s;

      std::getline(is, s);

      ++line;

      if(s.empty() || s[0] == '#')
         continue;

      s = clean_ws(s.substr(0, s.find('#')));

      if(s.empty())
         continue;

      auto eq = s.find("=");

      if(eq == std::string::npos || eq == 0 || eq == s.size() - 1)
         throw Decoding_Error("Bad read_cfg input '" + s + "' on line " + std::to_string(line));

      const std::string key = clean_ws(s.substr(0, eq));
      const std::string val = clean_ws(s.substr(eq + 1, std::string::npos));

      kv[key] = val;
      }

   return kv;
   }

}
/*
* (C) 2018 Ribose Inc
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::map<std::string, std::string> read_kv(const std::string& kv)
   {
   std::map<std::string, std::string> m;
   if(kv == "")
      return m;

   std::vector<std::string> parts;

   try
      {
      parts = split_on(kv, ',');
      }
   catch(std::exception&)
      {
      throw Invalid_Argument("Bad KV spec");
      }

   bool escaped = false;
   bool reading_key = true;
   std::string cur_key;
   std::string cur_val;

   for(char c : kv)
      {
      if(c == '\\' && !escaped)
         {
         escaped = true;
         }
      else if(c == ',' && !escaped)
         {
         if(cur_key.empty())
            throw Invalid_Argument("Bad KV spec empty key");

         if(m.find(cur_key) != m.end())
            throw Invalid_Argument("Bad KV spec duplicated key");
         m[cur_key] = cur_val;
         cur_key = "";
         cur_val = "";
         reading_key = true;
         }
      else if(c == '=' && !escaped)
         {
         if(reading_key == false)
            throw Invalid_Argument("Bad KV spec unexpected equals sign");
         reading_key = false;
         }
      else
         {
         if(reading_key)
            cur_key += c;
         else
            cur_val += c;

         if(escaped)
            escaped = false;
         }
      }

   if(!cur_key.empty())
      {
      if(reading_key == false)
         {
         if(m.find(cur_key) != m.end())
            throw Invalid_Argument("Bad KV spec duplicated key");
         m[cur_key] = cur_val;
         }
      else
         throw Invalid_Argument("Bad KV spec incomplete string");
      }

   return m;
   }

}
/*
* (C) 2018 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

void Timer::start()
   {
   stop();
   m_timer_start = OS::get_system_timestamp_ns();
   m_cpu_cycles_start = OS::get_cpu_cycle_counter();
   }

void Timer::stop()
   {
   if(m_timer_start)
      {
      if(m_cpu_cycles_start != 0)
         {
         const uint64_t cycles_taken = OS::get_cpu_cycle_counter() - m_cpu_cycles_start;
         if(cycles_taken > 0)
            {
            m_cpu_cycles_used += static_cast<size_t>(cycles_taken * m_clock_cycle_ratio);
            }
         }

      const uint64_t now = OS::get_system_timestamp_ns();

      if(now > m_timer_start)
         {
         const uint64_t dur = now - m_timer_start;

         m_time_used += dur;

         if(m_event_count == 0)
            {
            m_min_time = m_max_time = dur;
            }
         else
            {
            m_max_time = std::max(m_max_time, dur);
            m_min_time = std::min(m_min_time, dur);
            }
         }

      m_timer_start = 0;
      ++m_event_count;
      }
   }

bool Timer::operator<(const Timer& other) const
   {
   if(this->doing() != other.doing())
      return (this->doing() < other.doing());

   return (this->get_name() < other.get_name());
   }

std::string Timer::to_string() const
   {
   if(m_custom_msg.size() > 0)
      {
      return m_custom_msg;
      }
   else if(this->buf_size() == 0)
      {
      return result_string_ops();
      }
   else
      {
      return result_string_bps();
      }
   }

std::string Timer::result_string_bps() const
   {
   const size_t MiB = 1024 * 1024;

   const double MiB_total = static_cast<double>(events()) / MiB;
   const double MiB_per_sec = MiB_total / seconds();

   std::ostringstream oss;
   oss << get_name();

   if(!doing().empty())
      {
      oss << " " << doing();
      }

   if(buf_size() > 0)
      {
      oss << " buffer size " << buf_size() << " bytes:";
      }

   if(events() == 0)
      oss << " " << "N/A";
   else
      oss << " " << std::fixed << std::setprecision(3) << MiB_per_sec << " MiB/sec";

   if(cycles_consumed() != 0)
      {
      const double cycles_per_byte = static_cast<double>(cycles_consumed()) / events();
      oss << " " << std::fixed << std::setprecision(2) << cycles_per_byte << " cycles/byte";
      }

   oss << " (" << MiB_total << " MiB in " << milliseconds() << " ms)\n";

   return oss.str();
   }

std::string Timer::result_string_ops() const
   {
   std::ostringstream oss;

   oss << get_name() << " ";

   if(events() == 0)
      {
      oss << "no events\n";
      }
   else
      {
      oss << static_cast<uint64_t>(events_per_second())
          << ' ' << doing() << "/sec; "
          << std::setprecision(2) << std::fixed
          << ms_per_event() << " ms/op";

      if(cycles_consumed() != 0)
         {
         const double cycles_per_op = static_cast<double>(cycles_consumed()) / events();
         const int precision = (cycles_per_op < 10000) ? 2 : 0;
         oss << " " << std::fixed << std::setprecision(precision) << cycles_per_op << " cycles/op";
         }

      oss << " (" << events() << " " << (events() == 1 ? "op" : "ops")
          << " in " << milliseconds() << " ms)\n";
      }

   return oss.str();
   }

}
/*
* Version Information
* (C) 1999-2013,2015 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

/*
  These are intentionally compiled rather than inlined, so an
  application running against a shared library can test the true
  version they are running against.
*/

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

const char* short_version_cstr()
   {
   return STR(BOTAN_VERSION_MAJOR) "."
          STR(BOTAN_VERSION_MINOR) "."
          STR(BOTAN_VERSION_PATCH)
#if defined(BOTAN_VERSION_SUFFIX)
          STR(BOTAN_VERSION_SUFFIX)
#endif
      ;
   }

const char* version_cstr()
   {

   /*
   It is intentional that this string is a compile-time constant;
   it makes it much easier to find in binaries.
   */

   return "Botan " STR(BOTAN_VERSION_MAJOR) "."
                   STR(BOTAN_VERSION_MINOR) "."
                   STR(BOTAN_VERSION_PATCH)
#if defined(BOTAN_VERSION_SUFFIX)
                   STR(BOTAN_VERSION_SUFFIX)
#endif
                   " ("
#if defined(BOTAN_UNSAFE_FUZZER_MODE)
                   "UNSAFE FUZZER MODE BUILD "
#endif
                   BOTAN_VERSION_RELEASE_TYPE
#if (BOTAN_VERSION_DATESTAMP != 0)
                   ", dated " STR(BOTAN_VERSION_DATESTAMP)
#endif
                   ", revision " BOTAN_VERSION_VC_REVISION
                   ", distribution " BOTAN_DISTRIBUTION_INFO ")";
   }

#undef STR
#undef QUOTE

/*
* Return the version as a string
*/
std::string version_string()
   {
   return std::string(version_cstr());
   }

std::string short_version_string()
   {
   return std::string(short_version_cstr());
   }

uint32_t version_datestamp() { return BOTAN_VERSION_DATESTAMP; }

/*
* Return parts of the version as integers
*/
uint32_t version_major() { return BOTAN_VERSION_MAJOR; }
uint32_t version_minor() { return BOTAN_VERSION_MINOR; }
uint32_t version_patch() { return BOTAN_VERSION_PATCH; }

std::string runtime_version_check(uint32_t major,
                                  uint32_t minor,
                                  uint32_t patch)
   {
   if(major != version_major() || minor != version_minor() || patch != version_patch())
      {
      std::ostringstream oss;
      oss << "Warning: linked version (" << short_version_string() << ")"
          << " does not match version built against "
          << "(" << major << '.' << minor << '.' << patch << ")\n";
      return oss.str();
      }

   return "";
   }

}
/*
* Whirlpool
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

std::unique_ptr<HashFunction> Whirlpool::copy_state() const
   {
   return std::unique_ptr<HashFunction>(new Whirlpool(*this));
   }

/*
* Whirlpool Compression Function
*/
void Whirlpool::compress_n(const uint8_t in[], size_t blocks)
   {
   static const uint64_t RC[10] = {
      0x1823C6E887B8014F, 0x36A6D2F5796F9152,
      0x60BC9B8EA30C7B35, 0x1DE0D7C22E4BFE57,
      0x157737E59FF04ADA, 0x58C9290AB1A06B85,
      0xBD5D10F4CB3E0567, 0xE427418BA77D95D8,
      0xFBEE7C66DD17479E, 0xCA2DBF07AD5A8333
   };

   for(size_t i = 0; i != blocks; ++i)
      {
      load_be(m_M.data(), in, m_M.size());

      uint64_t K0, K1, K2, K3, K4, K5, K6, K7;
      K0 = m_digest[0]; K1 = m_digest[1]; K2 = m_digest[2]; K3 = m_digest[3];
      K4 = m_digest[4]; K5 = m_digest[5]; K6 = m_digest[6]; K7 = m_digest[7];

      uint64_t B0, B1, B2, B3, B4, B5, B6, B7;
      B0 = K0 ^ m_M[0]; B1 = K1 ^ m_M[1]; B2 = K2 ^ m_M[2]; B3 = K3 ^ m_M[3];
      B4 = K4 ^ m_M[4]; B5 = K5 ^ m_M[5]; B6 = K6 ^ m_M[6]; B7 = K7 ^ m_M[7];

      for(size_t j = 0; j != 10; ++j)
         {
         uint64_t T0, T1, T2, T3, T4, T5, T6, T7;
         T0 = C0[get_byte(0, K0)] ^ C1[get_byte(1, K7)] ^
              C2[get_byte(2, K6)] ^ C3[get_byte(3, K5)] ^
              C4[get_byte(4, K4)] ^ C5[get_byte(5, K3)] ^
              C6[get_byte(6, K2)] ^ C7[get_byte(7, K1)] ^ RC[j];
         T1 = C0[get_byte(0, K1)] ^ C1[get_byte(1, K0)] ^
              C2[get_byte(2, K7)] ^ C3[get_byte(3, K6)] ^
              C4[get_byte(4, K5)] ^ C5[get_byte(5, K4)] ^
              C6[get_byte(6, K3)] ^ C7[get_byte(7, K2)];
         T2 = C0[get_byte(0, K2)] ^ C1[get_byte(1, K1)] ^
              C2[get_byte(2, K0)] ^ C3[get_byte(3, K7)] ^
              C4[get_byte(4, K6)] ^ C5[get_byte(5, K5)] ^
              C6[get_byte(6, K4)] ^ C7[get_byte(7, K3)];
         T3 = C0[get_byte(0, K3)] ^ C1[get_byte(1, K2)] ^
              C2[get_byte(2, K1)] ^ C3[get_byte(3, K0)] ^
              C4[get_byte(4, K7)] ^ C5[get_byte(5, K6)] ^
              C6[get_byte(6, K5)] ^ C7[get_byte(7, K4)];
         T4 = C0[get_byte(0, K4)] ^ C1[get_byte(1, K3)] ^
              C2[get_byte(2, K2)] ^ C3[get_byte(3, K1)] ^
              C4[get_byte(4, K0)] ^ C5[get_byte(5, K7)] ^
              C6[get_byte(6, K6)] ^ C7[get_byte(7, K5)];
         T5 = C0[get_byte(0, K5)] ^ C1[get_byte(1, K4)] ^
              C2[get_byte(2, K3)] ^ C3[get_byte(3, K2)] ^
              C4[get_byte(4, K1)] ^ C5[get_byte(5, K0)] ^
              C6[get_byte(6, K7)] ^ C7[get_byte(7, K6)];
         T6 = C0[get_byte(0, K6)] ^ C1[get_byte(1, K5)] ^
              C2[get_byte(2, K4)] ^ C3[get_byte(3, K3)] ^
              C4[get_byte(4, K2)] ^ C5[get_byte(5, K1)] ^
              C6[get_byte(6, K0)] ^ C7[get_byte(7, K7)];
         T7 = C0[get_byte(0, K7)] ^ C1[get_byte(1, K6)] ^
              C2[get_byte(2, K5)] ^ C3[get_byte(3, K4)] ^
              C4[get_byte(4, K3)] ^ C5[get_byte(5, K2)] ^
              C6[get_byte(6, K1)] ^ C7[get_byte(7, K0)];

         K0 = T0; K1 = T1; K2 = T2; K3 = T3;
         K4 = T4; K5 = T5; K6 = T6; K7 = T7;

         T0 = C0[get_byte(0, B0)] ^ C1[get_byte(1, B7)] ^
              C2[get_byte(2, B6)] ^ C3[get_byte(3, B5)] ^
              C4[get_byte(4, B4)] ^ C5[get_byte(5, B3)] ^
              C6[get_byte(6, B2)] ^ C7[get_byte(7, B1)] ^ K0;
         T1 = C0[get_byte(0, B1)] ^ C1[get_byte(1, B0)] ^
              C2[get_byte(2, B7)] ^ C3[get_byte(3, B6)] ^
              C4[get_byte(4, B5)] ^ C5[get_byte(5, B4)] ^
              C6[get_byte(6, B3)] ^ C7[get_byte(7, B2)] ^ K1;
         T2 = C0[get_byte(0, B2)] ^ C1[get_byte(1, B1)] ^
              C2[get_byte(2, B0)] ^ C3[get_byte(3, B7)] ^
              C4[get_byte(4, B6)] ^ C5[get_byte(5, B5)] ^
              C6[get_byte(6, B4)] ^ C7[get_byte(7, B3)] ^ K2;
         T3 = C0[get_byte(0, B3)] ^ C1[get_byte(1, B2)] ^
              C2[get_byte(2, B1)] ^ C3[get_byte(3, B0)] ^
              C4[get_byte(4, B7)] ^ C5[get_byte(5, B6)] ^
              C6[get_byte(6, B5)] ^ C7[get_byte(7, B4)] ^ K3;
         T4 = C0[get_byte(0, B4)] ^ C1[get_byte(1, B3)] ^
              C2[get_byte(2, B2)] ^ C3[get_byte(3, B1)] ^
              C4[get_byte(4, B0)] ^ C5[get_byte(5, B7)] ^
              C6[get_byte(6, B6)] ^ C7[get_byte(7, B5)] ^ K4;
         T5 = C0[get_byte(0, B5)] ^ C1[get_byte(1, B4)] ^
              C2[get_byte(2, B3)] ^ C3[get_byte(3, B2)] ^
              C4[get_byte(4, B1)] ^ C5[get_byte(5, B0)] ^
              C6[get_byte(6, B7)] ^ C7[get_byte(7, B6)] ^ K5;
         T6 = C0[get_byte(0, B6)] ^ C1[get_byte(1, B5)] ^
              C2[get_byte(2, B4)] ^ C3[get_byte(3, B3)] ^
              C4[get_byte(4, B2)] ^ C5[get_byte(5, B1)] ^
              C6[get_byte(6, B0)] ^ C7[get_byte(7, B7)] ^ K6;
         T7 = C0[get_byte(0, B7)] ^ C1[get_byte(1, B6)] ^
              C2[get_byte(2, B5)] ^ C3[get_byte(3, B4)] ^
              C4[get_byte(4, B3)] ^ C5[get_byte(5, B2)] ^
              C6[get_byte(6, B1)] ^ C7[get_byte(7, B0)] ^ K7;

         B0 = T0; B1 = T1; B2 = T2; B3 = T3;
         B4 = T4; B5 = T5; B6 = T6; B7 = T7;
         }

      m_digest[0] ^= B0 ^ m_M[0];
      m_digest[1] ^= B1 ^ m_M[1];
      m_digest[2] ^= B2 ^ m_M[2];
      m_digest[3] ^= B3 ^ m_M[3];
      m_digest[4] ^= B4 ^ m_M[4];
      m_digest[5] ^= B5 ^ m_M[5];
      m_digest[6] ^= B6 ^ m_M[6];
      m_digest[7] ^= B7 ^ m_M[7];

      in += hash_block_size();
      }
   }

/*
* Copy out the digest
*/
void Whirlpool::copy_out(uint8_t output[])
   {
   copy_out_vec_be(output, output_length(), m_digest);
   }

/*
* Clear memory of sensitive data
*/
void Whirlpool::clear()
   {
   MDx_HashFunction::clear();
   zeroise(m_M);
   zeroise(m_digest);
   }

}
/*
* Diffusion Tables for Whirlpool
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/


namespace Botan {

alignas(64) const uint64_t Whirlpool::C0[256] = {
0x18186018C07830D8, 0x23238C2305AF4626, 0xC6C63FC67EF991B8, 0xE8E887E8136FCDFB,
0x878726874CA113CB, 0xB8B8DAB8A9626D11, 0x0101040108050209, 0x4F4F214F426E9E0D,
0x3636D836ADEE6C9B, 0xA6A6A2A6590451FF, 0xD2D26FD2DEBDB90C, 0xF5F5F3F5FB06F70E,
0x7979F979EF80F296, 0x6F6FA16F5FCEDE30, 0x91917E91FCEF3F6D, 0x52525552AA07A4F8,
0x60609D6027FDC047, 0xBCBCCABC89766535, 0x9B9B569BACCD2B37, 0x8E8E028E048C018A,
0xA3A3B6A371155BD2, 0x0C0C300C603C186C, 0x7B7BF17BFF8AF684, 0x3535D435B5E16A80,
0x1D1D741DE8693AF5, 0xE0E0A7E05347DDB3, 0xD7D77BD7F6ACB321, 0xC2C22FC25EED999C,
0x2E2EB82E6D965C43, 0x4B4B314B627A9629, 0xFEFEDFFEA321E15D, 0x575741578216AED5,
0x15155415A8412ABD, 0x7777C1779FB6EEE8, 0x3737DC37A5EB6E92, 0xE5E5B3E57B56D79E,
0x9F9F469F8CD92313, 0xF0F0E7F0D317FD23, 0x4A4A354A6A7F9420, 0xDADA4FDA9E95A944,
0x58587D58FA25B0A2, 0xC9C903C906CA8FCF, 0x2929A429558D527C, 0x0A0A280A5022145A,
0xB1B1FEB1E14F7F50, 0xA0A0BAA0691A5DC9, 0x6B6BB16B7FDAD614, 0x85852E855CAB17D9,
0xBDBDCEBD8173673C, 0x5D5D695DD234BA8F, 0x1010401080502090, 0xF4F4F7F4F303F507,
0xCBCB0BCB16C08BDD, 0x3E3EF83EEDC67CD3, 0x0505140528110A2D, 0x676781671FE6CE78,
0xE4E4B7E47353D597, 0x27279C2725BB4E02, 0x4141194132588273, 0x8B8B168B2C9D0BA7,
0xA7A7A6A7510153F6, 0x7D7DE97DCF94FAB2, 0x95956E95DCFB3749, 0xD8D847D88E9FAD56,
0xFBFBCBFB8B30EB70, 0xEEEE9FEE2371C1CD, 0x7C7CED7CC791F8BB, 0x6666856617E3CC71,
0xDDDD53DDA68EA77B, 0x17175C17B84B2EAF, 0x4747014702468E45, 0x9E9E429E84DC211A,
0xCACA0FCA1EC589D4, 0x2D2DB42D75995A58, 0xBFBFC6BF9179632E, 0x07071C07381B0E3F,
0xADAD8EAD012347AC, 0x5A5A755AEA2FB4B0, 0x838336836CB51BEF, 0x3333CC3385FF66B6,
0x636391633FF2C65C, 0x02020802100A0412, 0xAAAA92AA39384993, 0x7171D971AFA8E2DE,
0xC8C807C80ECF8DC6, 0x19196419C87D32D1, 0x494939497270923B, 0xD9D943D9869AAF5F,
0xF2F2EFF2C31DF931, 0xE3E3ABE34B48DBA8, 0x5B5B715BE22AB6B9, 0x88881A8834920DBC,
0x9A9A529AA4C8293E, 0x262698262DBE4C0B, 0x3232C8328DFA64BF, 0xB0B0FAB0E94A7D59,
0xE9E983E91B6ACFF2, 0x0F0F3C0F78331E77, 0xD5D573D5E6A6B733, 0x80803A8074BA1DF4,
0xBEBEC2BE997C6127, 0xCDCD13CD26DE87EB, 0x3434D034BDE46889, 0x48483D487A759032,
0xFFFFDBFFAB24E354, 0x7A7AF57AF78FF48D, 0x90907A90F4EA3D64, 0x5F5F615FC23EBE9D,
0x202080201DA0403D, 0x6868BD6867D5D00F, 0x1A1A681AD07234CA, 0xAEAE82AE192C41B7,
0xB4B4EAB4C95E757D, 0x54544D549A19A8CE, 0x93937693ECE53B7F, 0x222288220DAA442F,
0x64648D6407E9C863, 0xF1F1E3F1DB12FF2A, 0x7373D173BFA2E6CC, 0x12124812905A2482,
0x40401D403A5D807A, 0x0808200840281048, 0xC3C32BC356E89B95, 0xECEC97EC337BC5DF,
0xDBDB4BDB9690AB4D, 0xA1A1BEA1611F5FC0, 0x8D8D0E8D1C830791, 0x3D3DF43DF5C97AC8,
0x97976697CCF1335B, 0x0000000000000000, 0xCFCF1BCF36D483F9, 0x2B2BAC2B4587566E,
0x7676C57697B3ECE1, 0x8282328264B019E6, 0xD6D67FD6FEA9B128, 0x1B1B6C1BD87736C3,
0xB5B5EEB5C15B7774, 0xAFAF86AF112943BE, 0x6A6AB56A77DFD41D, 0x50505D50BA0DA0EA,
0x45450945124C8A57, 0xF3F3EBF3CB18FB38, 0x3030C0309DF060AD, 0xEFEF9BEF2B74C3C4,
0x3F3FFC3FE5C37EDA, 0x55554955921CAAC7, 0xA2A2B2A2791059DB, 0xEAEA8FEA0365C9E9,
0x656589650FECCA6A, 0xBABAD2BAB9686903, 0x2F2FBC2F65935E4A, 0xC0C027C04EE79D8E,
0xDEDE5FDEBE81A160, 0x1C1C701CE06C38FC, 0xFDFDD3FDBB2EE746, 0x4D4D294D52649A1F,
0x92927292E4E03976, 0x7575C9758FBCEAFA, 0x06061806301E0C36, 0x8A8A128A249809AE,
0xB2B2F2B2F940794B, 0xE6E6BFE66359D185, 0x0E0E380E70361C7E, 0x1F1F7C1FF8633EE7,
0x6262956237F7C455, 0xD4D477D4EEA3B53A, 0xA8A89AA829324D81, 0x96966296C4F43152,
0xF9F9C3F99B3AEF62, 0xC5C533C566F697A3, 0x2525942535B14A10, 0x59597959F220B2AB,
0x84842A8454AE15D0, 0x7272D572B7A7E4C5, 0x3939E439D5DD72EC, 0x4C4C2D4C5A619816,
0x5E5E655ECA3BBC94, 0x7878FD78E785F09F, 0x3838E038DDD870E5, 0x8C8C0A8C14860598,
0xD1D163D1C6B2BF17, 0xA5A5AEA5410B57E4, 0xE2E2AFE2434DD9A1, 0x616199612FF8C24E,
0xB3B3F6B3F1457B42, 0x2121842115A54234, 0x9C9C4A9C94D62508, 0x1E1E781EF0663CEE,
0x4343114322528661, 0xC7C73BC776FC93B1, 0xFCFCD7FCB32BE54F, 0x0404100420140824,
0x51515951B208A2E3, 0x99995E99BCC72F25, 0x6D6DA96D4FC4DA22, 0x0D0D340D68391A65,
0xFAFACFFA8335E979, 0xDFDF5BDFB684A369, 0x7E7EE57ED79BFCA9, 0x242490243DB44819,
0x3B3BEC3BC5D776FE, 0xABAB96AB313D4B9A, 0xCECE1FCE3ED181F0, 0x1111441188552299,
0x8F8F068F0C890383, 0x4E4E254E4A6B9C04, 0xB7B7E6B7D1517366, 0xEBEB8BEB0B60CBE0,
0x3C3CF03CFDCC78C1, 0x81813E817CBF1FFD, 0x94946A94D4FE3540, 0xF7F7FBF7EB0CF31C,
0xB9B9DEB9A1676F18, 0x13134C13985F268B, 0x2C2CB02C7D9C5851, 0xD3D36BD3D6B8BB05,
0xE7E7BBE76B5CD38C, 0x6E6EA56E57CBDC39, 0xC4C437C46EF395AA, 0x03030C03180F061B,
0x565645568A13ACDC, 0x44440D441A49885E, 0x7F7FE17FDF9EFEA0, 0xA9A99EA921374F88,
0x2A2AA82A4D825467, 0xBBBBD6BBB16D6B0A, 0xC1C123C146E29F87, 0x53535153A202A6F1,
0xDCDC57DCAE8BA572, 0x0B0B2C0B58271653, 0x9D9D4E9D9CD32701, 0x6C6CAD6C47C1D82B,
0x3131C43195F562A4, 0x7474CD7487B9E8F3, 0xF6F6FFF6E309F115, 0x464605460A438C4C,
0xACAC8AAC092645A5, 0x89891E893C970FB5, 0x14145014A04428B4, 0xE1E1A3E15B42DFBA,
0x16165816B04E2CA6, 0x3A3AE83ACDD274F7, 0x6969B9696FD0D206, 0x09092409482D1241,
0x7070DD70A7ADE0D7, 0xB6B6E2B6D954716F, 0xD0D067D0CEB7BD1E, 0xEDED93ED3B7EC7D6,
0xCCCC17CC2EDB85E2, 0x424215422A578468, 0x98985A98B4C22D2C, 0xA4A4AAA4490E55ED,
0x2828A0285D885075, 0x5C5C6D5CDA31B886, 0xF8F8C7F8933FED6B, 0x8686228644A411C2 };

alignas(64) const uint64_t Whirlpool::C1[256] = {
0xD818186018C07830, 0x2623238C2305AF46, 0xB8C6C63FC67EF991, 0xFBE8E887E8136FCD,
0xCB878726874CA113, 0x11B8B8DAB8A9626D, 0x0901010401080502, 0x0D4F4F214F426E9E,
0x9B3636D836ADEE6C, 0xFFA6A6A2A6590451, 0x0CD2D26FD2DEBDB9, 0x0EF5F5F3F5FB06F7,
0x967979F979EF80F2, 0x306F6FA16F5FCEDE, 0x6D91917E91FCEF3F, 0xF852525552AA07A4,
0x4760609D6027FDC0, 0x35BCBCCABC897665, 0x379B9B569BACCD2B, 0x8A8E8E028E048C01,
0xD2A3A3B6A371155B, 0x6C0C0C300C603C18, 0x847B7BF17BFF8AF6, 0x803535D435B5E16A,
0xF51D1D741DE8693A, 0xB3E0E0A7E05347DD, 0x21D7D77BD7F6ACB3, 0x9CC2C22FC25EED99,
0x432E2EB82E6D965C, 0x294B4B314B627A96, 0x5DFEFEDFFEA321E1, 0xD5575741578216AE,
0xBD15155415A8412A, 0xE87777C1779FB6EE, 0x923737DC37A5EB6E, 0x9EE5E5B3E57B56D7,
0x139F9F469F8CD923, 0x23F0F0E7F0D317FD, 0x204A4A354A6A7F94, 0x44DADA4FDA9E95A9,
0xA258587D58FA25B0, 0xCFC9C903C906CA8F, 0x7C2929A429558D52, 0x5A0A0A280A502214,
0x50B1B1FEB1E14F7F, 0xC9A0A0BAA0691A5D, 0x146B6BB16B7FDAD6, 0xD985852E855CAB17,
0x3CBDBDCEBD817367, 0x8F5D5D695DD234BA, 0x9010104010805020, 0x07F4F4F7F4F303F5,
0xDDCBCB0BCB16C08B, 0xD33E3EF83EEDC67C, 0x2D0505140528110A, 0x78676781671FE6CE,
0x97E4E4B7E47353D5, 0x0227279C2725BB4E, 0x7341411941325882, 0xA78B8B168B2C9D0B,
0xF6A7A7A6A7510153, 0xB27D7DE97DCF94FA, 0x4995956E95DCFB37, 0x56D8D847D88E9FAD,
0x70FBFBCBFB8B30EB, 0xCDEEEE9FEE2371C1, 0xBB7C7CED7CC791F8, 0x716666856617E3CC,
0x7BDDDD53DDA68EA7, 0xAF17175C17B84B2E, 0x454747014702468E, 0x1A9E9E429E84DC21,
0xD4CACA0FCA1EC589, 0x582D2DB42D75995A, 0x2EBFBFC6BF917963, 0x3F07071C07381B0E,
0xACADAD8EAD012347, 0xB05A5A755AEA2FB4, 0xEF838336836CB51B, 0xB63333CC3385FF66,
0x5C636391633FF2C6, 0x1202020802100A04, 0x93AAAA92AA393849, 0xDE7171D971AFA8E2,
0xC6C8C807C80ECF8D, 0xD119196419C87D32, 0x3B49493949727092, 0x5FD9D943D9869AAF,
0x31F2F2EFF2C31DF9, 0xA8E3E3ABE34B48DB, 0xB95B5B715BE22AB6, 0xBC88881A8834920D,
0x3E9A9A529AA4C829, 0x0B262698262DBE4C, 0xBF3232C8328DFA64, 0x59B0B0FAB0E94A7D,
0xF2E9E983E91B6ACF, 0x770F0F3C0F78331E, 0x33D5D573D5E6A6B7, 0xF480803A8074BA1D,
0x27BEBEC2BE997C61, 0xEBCDCD13CD26DE87, 0x893434D034BDE468, 0x3248483D487A7590,
0x54FFFFDBFFAB24E3, 0x8D7A7AF57AF78FF4, 0x6490907A90F4EA3D, 0x9D5F5F615FC23EBE,
0x3D202080201DA040, 0x0F6868BD6867D5D0, 0xCA1A1A681AD07234, 0xB7AEAE82AE192C41,
0x7DB4B4EAB4C95E75, 0xCE54544D549A19A8, 0x7F93937693ECE53B, 0x2F222288220DAA44,
0x6364648D6407E9C8, 0x2AF1F1E3F1DB12FF, 0xCC7373D173BFA2E6, 0x8212124812905A24,
0x7A40401D403A5D80, 0x4808082008402810, 0x95C3C32BC356E89B, 0xDFECEC97EC337BC5,
0x4DDBDB4BDB9690AB, 0xC0A1A1BEA1611F5F, 0x918D8D0E8D1C8307, 0xC83D3DF43DF5C97A,
0x5B97976697CCF133, 0x0000000000000000, 0xF9CFCF1BCF36D483, 0x6E2B2BAC2B458756,
0xE17676C57697B3EC, 0xE68282328264B019, 0x28D6D67FD6FEA9B1, 0xC31B1B6C1BD87736,
0x74B5B5EEB5C15B77, 0xBEAFAF86AF112943, 0x1D6A6AB56A77DFD4, 0xEA50505D50BA0DA0,
0x5745450945124C8A, 0x38F3F3EBF3CB18FB, 0xAD3030C0309DF060, 0xC4EFEF9BEF2B74C3,
0xDA3F3FFC3FE5C37E, 0xC755554955921CAA, 0xDBA2A2B2A2791059, 0xE9EAEA8FEA0365C9,
0x6A656589650FECCA, 0x03BABAD2BAB96869, 0x4A2F2FBC2F65935E, 0x8EC0C027C04EE79D,
0x60DEDE5FDEBE81A1, 0xFC1C1C701CE06C38, 0x46FDFDD3FDBB2EE7, 0x1F4D4D294D52649A,
0x7692927292E4E039, 0xFA7575C9758FBCEA, 0x3606061806301E0C, 0xAE8A8A128A249809,
0x4BB2B2F2B2F94079, 0x85E6E6BFE66359D1, 0x7E0E0E380E70361C, 0xE71F1F7C1FF8633E,
0x556262956237F7C4, 0x3AD4D477D4EEA3B5, 0x81A8A89AA829324D, 0x5296966296C4F431,
0x62F9F9C3F99B3AEF, 0xA3C5C533C566F697, 0x102525942535B14A, 0xAB59597959F220B2,
0xD084842A8454AE15, 0xC57272D572B7A7E4, 0xEC3939E439D5DD72, 0x164C4C2D4C5A6198,
0x945E5E655ECA3BBC, 0x9F7878FD78E785F0, 0xE53838E038DDD870, 0x988C8C0A8C148605,
0x17D1D163D1C6B2BF, 0xE4A5A5AEA5410B57, 0xA1E2E2AFE2434DD9, 0x4E616199612FF8C2,
0x42B3B3F6B3F1457B, 0x342121842115A542, 0x089C9C4A9C94D625, 0xEE1E1E781EF0663C,
0x6143431143225286, 0xB1C7C73BC776FC93, 0x4FFCFCD7FCB32BE5, 0x2404041004201408,
0xE351515951B208A2, 0x2599995E99BCC72F, 0x226D6DA96D4FC4DA, 0x650D0D340D68391A,
0x79FAFACFFA8335E9, 0x69DFDF5BDFB684A3, 0xA97E7EE57ED79BFC, 0x19242490243DB448,
0xFE3B3BEC3BC5D776, 0x9AABAB96AB313D4B, 0xF0CECE1FCE3ED181, 0x9911114411885522,
0x838F8F068F0C8903, 0x044E4E254E4A6B9C, 0x66B7B7E6B7D15173, 0xE0EBEB8BEB0B60CB,
0xC13C3CF03CFDCC78, 0xFD81813E817CBF1F, 0x4094946A94D4FE35, 0x1CF7F7FBF7EB0CF3,
0x18B9B9DEB9A1676F, 0x8B13134C13985F26, 0x512C2CB02C7D9C58, 0x05D3D36BD3D6B8BB,
0x8CE7E7BBE76B5CD3, 0x396E6EA56E57CBDC, 0xAAC4C437C46EF395, 0x1B03030C03180F06,
0xDC565645568A13AC, 0x5E44440D441A4988, 0xA07F7FE17FDF9EFE, 0x88A9A99EA921374F,
0x672A2AA82A4D8254, 0x0ABBBBD6BBB16D6B, 0x87C1C123C146E29F, 0xF153535153A202A6,
0x72DCDC57DCAE8BA5, 0x530B0B2C0B582716, 0x019D9D4E9D9CD327, 0x2B6C6CAD6C47C1D8,
0xA43131C43195F562, 0xF37474CD7487B9E8, 0x15F6F6FFF6E309F1, 0x4C464605460A438C,
0xA5ACAC8AAC092645, 0xB589891E893C970F, 0xB414145014A04428, 0xBAE1E1A3E15B42DF,
0xA616165816B04E2C, 0xF73A3AE83ACDD274, 0x066969B9696FD0D2, 0x4109092409482D12,
0xD77070DD70A7ADE0, 0x6FB6B6E2B6D95471, 0x1ED0D067D0CEB7BD, 0xD6EDED93ED3B7EC7,
0xE2CCCC17CC2EDB85, 0x68424215422A5784, 0x2C98985A98B4C22D, 0xEDA4A4AAA4490E55,
0x752828A0285D8850, 0x865C5C6D5CDA31B8, 0x6BF8F8C7F8933FED, 0xC28686228644A411 };

alignas(64) const uint64_t Whirlpool::C2[256] = {
0x30D818186018C078, 0x462623238C2305AF, 0x91B8C6C63FC67EF9, 0xCDFBE8E887E8136F,
0x13CB878726874CA1, 0x6D11B8B8DAB8A962, 0x0209010104010805, 0x9E0D4F4F214F426E,
0x6C9B3636D836ADEE, 0x51FFA6A6A2A65904, 0xB90CD2D26FD2DEBD, 0xF70EF5F5F3F5FB06,
0xF2967979F979EF80, 0xDE306F6FA16F5FCE, 0x3F6D91917E91FCEF, 0xA4F852525552AA07,
0xC04760609D6027FD, 0x6535BCBCCABC8976, 0x2B379B9B569BACCD, 0x018A8E8E028E048C,
0x5BD2A3A3B6A37115, 0x186C0C0C300C603C, 0xF6847B7BF17BFF8A, 0x6A803535D435B5E1,
0x3AF51D1D741DE869, 0xDDB3E0E0A7E05347, 0xB321D7D77BD7F6AC, 0x999CC2C22FC25EED,
0x5C432E2EB82E6D96, 0x96294B4B314B627A, 0xE15DFEFEDFFEA321, 0xAED5575741578216,
0x2ABD15155415A841, 0xEEE87777C1779FB6, 0x6E923737DC37A5EB, 0xD79EE5E5B3E57B56,
0x23139F9F469F8CD9, 0xFD23F0F0E7F0D317, 0x94204A4A354A6A7F, 0xA944DADA4FDA9E95,
0xB0A258587D58FA25, 0x8FCFC9C903C906CA, 0x527C2929A429558D, 0x145A0A0A280A5022,
0x7F50B1B1FEB1E14F, 0x5DC9A0A0BAA0691A, 0xD6146B6BB16B7FDA, 0x17D985852E855CAB,
0x673CBDBDCEBD8173, 0xBA8F5D5D695DD234, 0x2090101040108050, 0xF507F4F4F7F4F303,
0x8BDDCBCB0BCB16C0, 0x7CD33E3EF83EEDC6, 0x0A2D050514052811, 0xCE78676781671FE6,
0xD597E4E4B7E47353, 0x4E0227279C2725BB, 0x8273414119413258, 0x0BA78B8B168B2C9D,
0x53F6A7A7A6A75101, 0xFAB27D7DE97DCF94, 0x374995956E95DCFB, 0xAD56D8D847D88E9F,
0xEB70FBFBCBFB8B30, 0xC1CDEEEE9FEE2371, 0xF8BB7C7CED7CC791, 0xCC716666856617E3,
0xA77BDDDD53DDA68E, 0x2EAF17175C17B84B, 0x8E45474701470246, 0x211A9E9E429E84DC,
0x89D4CACA0FCA1EC5, 0x5A582D2DB42D7599, 0x632EBFBFC6BF9179, 0x0E3F07071C07381B,
0x47ACADAD8EAD0123, 0xB4B05A5A755AEA2F, 0x1BEF838336836CB5, 0x66B63333CC3385FF,
0xC65C636391633FF2, 0x041202020802100A, 0x4993AAAA92AA3938, 0xE2DE7171D971AFA8,
0x8DC6C8C807C80ECF, 0x32D119196419C87D, 0x923B494939497270, 0xAF5FD9D943D9869A,
0xF931F2F2EFF2C31D, 0xDBA8E3E3ABE34B48, 0xB6B95B5B715BE22A, 0x0DBC88881A883492,
0x293E9A9A529AA4C8, 0x4C0B262698262DBE, 0x64BF3232C8328DFA, 0x7D59B0B0FAB0E94A,
0xCFF2E9E983E91B6A, 0x1E770F0F3C0F7833, 0xB733D5D573D5E6A6, 0x1DF480803A8074BA,
0x6127BEBEC2BE997C, 0x87EBCDCD13CD26DE, 0x68893434D034BDE4, 0x903248483D487A75,
0xE354FFFFDBFFAB24, 0xF48D7A7AF57AF78F, 0x3D6490907A90F4EA, 0xBE9D5F5F615FC23E,
0x403D202080201DA0, 0xD00F6868BD6867D5, 0x34CA1A1A681AD072, 0x41B7AEAE82AE192C,
0x757DB4B4EAB4C95E, 0xA8CE54544D549A19, 0x3B7F93937693ECE5, 0x442F222288220DAA,
0xC86364648D6407E9, 0xFF2AF1F1E3F1DB12, 0xE6CC7373D173BFA2, 0x248212124812905A,
0x807A40401D403A5D, 0x1048080820084028, 0x9B95C3C32BC356E8, 0xC5DFECEC97EC337B,
0xAB4DDBDB4BDB9690, 0x5FC0A1A1BEA1611F, 0x07918D8D0E8D1C83, 0x7AC83D3DF43DF5C9,
0x335B97976697CCF1, 0x0000000000000000, 0x83F9CFCF1BCF36D4, 0x566E2B2BAC2B4587,
0xECE17676C57697B3, 0x19E68282328264B0, 0xB128D6D67FD6FEA9, 0x36C31B1B6C1BD877,
0x7774B5B5EEB5C15B, 0x43BEAFAF86AF1129, 0xD41D6A6AB56A77DF, 0xA0EA50505D50BA0D,
0x8A5745450945124C, 0xFB38F3F3EBF3CB18, 0x60AD3030C0309DF0, 0xC3C4EFEF9BEF2B74,
0x7EDA3F3FFC3FE5C3, 0xAAC755554955921C, 0x59DBA2A2B2A27910, 0xC9E9EAEA8FEA0365,
0xCA6A656589650FEC, 0x6903BABAD2BAB968, 0x5E4A2F2FBC2F6593, 0x9D8EC0C027C04EE7,
0xA160DEDE5FDEBE81, 0x38FC1C1C701CE06C, 0xE746FDFDD3FDBB2E, 0x9A1F4D4D294D5264,
0x397692927292E4E0, 0xEAFA7575C9758FBC, 0x0C3606061806301E, 0x09AE8A8A128A2498,
0x794BB2B2F2B2F940, 0xD185E6E6BFE66359, 0x1C7E0E0E380E7036, 0x3EE71F1F7C1FF863,
0xC4556262956237F7, 0xB53AD4D477D4EEA3, 0x4D81A8A89AA82932, 0x315296966296C4F4,
0xEF62F9F9C3F99B3A, 0x97A3C5C533C566F6, 0x4A102525942535B1, 0xB2AB59597959F220,
0x15D084842A8454AE, 0xE4C57272D572B7A7, 0x72EC3939E439D5DD, 0x98164C4C2D4C5A61,
0xBC945E5E655ECA3B, 0xF09F7878FD78E785, 0x70E53838E038DDD8, 0x05988C8C0A8C1486,
0xBF17D1D163D1C6B2, 0x57E4A5A5AEA5410B, 0xD9A1E2E2AFE2434D, 0xC24E616199612FF8,
0x7B42B3B3F6B3F145, 0x42342121842115A5, 0x25089C9C4A9C94D6, 0x3CEE1E1E781EF066,
0x8661434311432252, 0x93B1C7C73BC776FC, 0xE54FFCFCD7FCB32B, 0x0824040410042014,
0xA2E351515951B208, 0x2F2599995E99BCC7, 0xDA226D6DA96D4FC4, 0x1A650D0D340D6839,
0xE979FAFACFFA8335, 0xA369DFDF5BDFB684, 0xFCA97E7EE57ED79B, 0x4819242490243DB4,
0x76FE3B3BEC3BC5D7, 0x4B9AABAB96AB313D, 0x81F0CECE1FCE3ED1, 0x2299111144118855,
0x03838F8F068F0C89, 0x9C044E4E254E4A6B, 0x7366B7B7E6B7D151, 0xCBE0EBEB8BEB0B60,
0x78C13C3CF03CFDCC, 0x1FFD81813E817CBF, 0x354094946A94D4FE, 0xF31CF7F7FBF7EB0C,
0x6F18B9B9DEB9A167, 0x268B13134C13985F, 0x58512C2CB02C7D9C, 0xBB05D3D36BD3D6B8,
0xD38CE7E7BBE76B5C, 0xDC396E6EA56E57CB, 0x95AAC4C437C46EF3, 0x061B03030C03180F,
0xACDC565645568A13, 0x885E44440D441A49, 0xFEA07F7FE17FDF9E, 0x4F88A9A99EA92137,
0x54672A2AA82A4D82, 0x6B0ABBBBD6BBB16D, 0x9F87C1C123C146E2, 0xA6F153535153A202,
0xA572DCDC57DCAE8B, 0x16530B0B2C0B5827, 0x27019D9D4E9D9CD3, 0xD82B6C6CAD6C47C1,
0x62A43131C43195F5, 0xE8F37474CD7487B9, 0xF115F6F6FFF6E309, 0x8C4C464605460A43,
0x45A5ACAC8AAC0926, 0x0FB589891E893C97, 0x28B414145014A044, 0xDFBAE1E1A3E15B42,
0x2CA616165816B04E, 0x74F73A3AE83ACDD2, 0xD2066969B9696FD0, 0x124109092409482D,
0xE0D77070DD70A7AD, 0x716FB6B6E2B6D954, 0xBD1ED0D067D0CEB7, 0xC7D6EDED93ED3B7E,
0x85E2CCCC17CC2EDB, 0x8468424215422A57, 0x2D2C98985A98B4C2, 0x55EDA4A4AAA4490E,
0x50752828A0285D88, 0xB8865C5C6D5CDA31, 0xED6BF8F8C7F8933F, 0x11C28686228644A4 };

alignas(64) const uint64_t Whirlpool::C3[256] = {
0x7830D818186018C0, 0xAF462623238C2305, 0xF991B8C6C63FC67E, 0x6FCDFBE8E887E813,
0xA113CB878726874C, 0x626D11B8B8DAB8A9, 0x0502090101040108, 0x6E9E0D4F4F214F42,
0xEE6C9B3636D836AD, 0x0451FFA6A6A2A659, 0xBDB90CD2D26FD2DE, 0x06F70EF5F5F3F5FB,
0x80F2967979F979EF, 0xCEDE306F6FA16F5F, 0xEF3F6D91917E91FC, 0x07A4F852525552AA,
0xFDC04760609D6027, 0x766535BCBCCABC89, 0xCD2B379B9B569BAC, 0x8C018A8E8E028E04,
0x155BD2A3A3B6A371, 0x3C186C0C0C300C60, 0x8AF6847B7BF17BFF, 0xE16A803535D435B5,
0x693AF51D1D741DE8, 0x47DDB3E0E0A7E053, 0xACB321D7D77BD7F6, 0xED999CC2C22FC25E,
0x965C432E2EB82E6D, 0x7A96294B4B314B62, 0x21E15DFEFEDFFEA3, 0x16AED55757415782,
0x412ABD15155415A8, 0xB6EEE87777C1779F, 0xEB6E923737DC37A5, 0x56D79EE5E5B3E57B,
0xD923139F9F469F8C, 0x17FD23F0F0E7F0D3, 0x7F94204A4A354A6A, 0x95A944DADA4FDA9E,
0x25B0A258587D58FA, 0xCA8FCFC9C903C906, 0x8D527C2929A42955, 0x22145A0A0A280A50,
0x4F7F50B1B1FEB1E1, 0x1A5DC9A0A0BAA069, 0xDAD6146B6BB16B7F, 0xAB17D985852E855C,
0x73673CBDBDCEBD81, 0x34BA8F5D5D695DD2, 0x5020901010401080, 0x03F507F4F4F7F4F3,
0xC08BDDCBCB0BCB16, 0xC67CD33E3EF83EED, 0x110A2D0505140528, 0xE6CE78676781671F,
0x53D597E4E4B7E473, 0xBB4E0227279C2725, 0x5882734141194132, 0x9D0BA78B8B168B2C,
0x0153F6A7A7A6A751, 0x94FAB27D7DE97DCF, 0xFB374995956E95DC, 0x9FAD56D8D847D88E,
0x30EB70FBFBCBFB8B, 0x71C1CDEEEE9FEE23, 0x91F8BB7C7CED7CC7, 0xE3CC716666856617,
0x8EA77BDDDD53DDA6, 0x4B2EAF17175C17B8, 0x468E454747014702, 0xDC211A9E9E429E84,
0xC589D4CACA0FCA1E, 0x995A582D2DB42D75, 0x79632EBFBFC6BF91, 0x1B0E3F07071C0738,
0x2347ACADAD8EAD01, 0x2FB4B05A5A755AEA, 0xB51BEF838336836C, 0xFF66B63333CC3385,
0xF2C65C636391633F, 0x0A04120202080210, 0x384993AAAA92AA39, 0xA8E2DE7171D971AF,
0xCF8DC6C8C807C80E, 0x7D32D119196419C8, 0x70923B4949394972, 0x9AAF5FD9D943D986,
0x1DF931F2F2EFF2C3, 0x48DBA8E3E3ABE34B, 0x2AB6B95B5B715BE2, 0x920DBC88881A8834,
0xC8293E9A9A529AA4, 0xBE4C0B262698262D, 0xFA64BF3232C8328D, 0x4A7D59B0B0FAB0E9,
0x6ACFF2E9E983E91B, 0x331E770F0F3C0F78, 0xA6B733D5D573D5E6, 0xBA1DF480803A8074,
0x7C6127BEBEC2BE99, 0xDE87EBCDCD13CD26, 0xE468893434D034BD, 0x75903248483D487A,
0x24E354FFFFDBFFAB, 0x8FF48D7A7AF57AF7, 0xEA3D6490907A90F4, 0x3EBE9D5F5F615FC2,
0xA0403D202080201D, 0xD5D00F6868BD6867, 0x7234CA1A1A681AD0, 0x2C41B7AEAE82AE19,
0x5E757DB4B4EAB4C9, 0x19A8CE54544D549A, 0xE53B7F93937693EC, 0xAA442F222288220D,
0xE9C86364648D6407, 0x12FF2AF1F1E3F1DB, 0xA2E6CC7373D173BF, 0x5A24821212481290,
0x5D807A40401D403A, 0x2810480808200840, 0xE89B95C3C32BC356, 0x7BC5DFECEC97EC33,
0x90AB4DDBDB4BDB96, 0x1F5FC0A1A1BEA161, 0x8307918D8D0E8D1C, 0xC97AC83D3DF43DF5,
0xF1335B97976697CC, 0x0000000000000000, 0xD483F9CFCF1BCF36, 0x87566E2B2BAC2B45,
0xB3ECE17676C57697, 0xB019E68282328264, 0xA9B128D6D67FD6FE, 0x7736C31B1B6C1BD8,
0x5B7774B5B5EEB5C1, 0x2943BEAFAF86AF11, 0xDFD41D6A6AB56A77, 0x0DA0EA50505D50BA,
0x4C8A574545094512, 0x18FB38F3F3EBF3CB, 0xF060AD3030C0309D, 0x74C3C4EFEF9BEF2B,
0xC37EDA3F3FFC3FE5, 0x1CAAC75555495592, 0x1059DBA2A2B2A279, 0x65C9E9EAEA8FEA03,
0xECCA6A656589650F, 0x686903BABAD2BAB9, 0x935E4A2F2FBC2F65, 0xE79D8EC0C027C04E,
0x81A160DEDE5FDEBE, 0x6C38FC1C1C701CE0, 0x2EE746FDFDD3FDBB, 0x649A1F4D4D294D52,
0xE0397692927292E4, 0xBCEAFA7575C9758F, 0x1E0C360606180630, 0x9809AE8A8A128A24,
0x40794BB2B2F2B2F9, 0x59D185E6E6BFE663, 0x361C7E0E0E380E70, 0x633EE71F1F7C1FF8,
0xF7C4556262956237, 0xA3B53AD4D477D4EE, 0x324D81A8A89AA829, 0xF4315296966296C4,
0x3AEF62F9F9C3F99B, 0xF697A3C5C533C566, 0xB14A102525942535, 0x20B2AB59597959F2,
0xAE15D084842A8454, 0xA7E4C57272D572B7, 0xDD72EC3939E439D5, 0x6198164C4C2D4C5A,
0x3BBC945E5E655ECA, 0x85F09F7878FD78E7, 0xD870E53838E038DD, 0x8605988C8C0A8C14,
0xB2BF17D1D163D1C6, 0x0B57E4A5A5AEA541, 0x4DD9A1E2E2AFE243, 0xF8C24E616199612F,
0x457B42B3B3F6B3F1, 0xA542342121842115, 0xD625089C9C4A9C94, 0x663CEE1E1E781EF0,
0x5286614343114322, 0xFC93B1C7C73BC776, 0x2BE54FFCFCD7FCB3, 0x1408240404100420,
0x08A2E351515951B2, 0xC72F2599995E99BC, 0xC4DA226D6DA96D4F, 0x391A650D0D340D68,
0x35E979FAFACFFA83, 0x84A369DFDF5BDFB6, 0x9BFCA97E7EE57ED7, 0xB44819242490243D,
0xD776FE3B3BEC3BC5, 0x3D4B9AABAB96AB31, 0xD181F0CECE1FCE3E, 0x5522991111441188,
0x8903838F8F068F0C, 0x6B9C044E4E254E4A, 0x517366B7B7E6B7D1, 0x60CBE0EBEB8BEB0B,
0xCC78C13C3CF03CFD, 0xBF1FFD81813E817C, 0xFE354094946A94D4, 0x0CF31CF7F7FBF7EB,
0x676F18B9B9DEB9A1, 0x5F268B13134C1398, 0x9C58512C2CB02C7D, 0xB8BB05D3D36BD3D6,
0x5CD38CE7E7BBE76B, 0xCBDC396E6EA56E57, 0xF395AAC4C437C46E, 0x0F061B03030C0318,
0x13ACDC565645568A, 0x49885E44440D441A, 0x9EFEA07F7FE17FDF, 0x374F88A9A99EA921,
0x8254672A2AA82A4D, 0x6D6B0ABBBBD6BBB1, 0xE29F87C1C123C146, 0x02A6F153535153A2,
0x8BA572DCDC57DCAE, 0x2716530B0B2C0B58, 0xD327019D9D4E9D9C, 0xC1D82B6C6CAD6C47,
0xF562A43131C43195, 0xB9E8F37474CD7487, 0x09F115F6F6FFF6E3, 0x438C4C464605460A,
0x2645A5ACAC8AAC09, 0x970FB589891E893C, 0x4428B414145014A0, 0x42DFBAE1E1A3E15B,
0x4E2CA616165816B0, 0xD274F73A3AE83ACD, 0xD0D2066969B9696F, 0x2D12410909240948,
0xADE0D77070DD70A7, 0x54716FB6B6E2B6D9, 0xB7BD1ED0D067D0CE, 0x7EC7D6EDED93ED3B,
0xDB85E2CCCC17CC2E, 0x578468424215422A, 0xC22D2C98985A98B4, 0x0E55EDA4A4AAA449,
0x8850752828A0285D, 0x31B8865C5C6D5CDA, 0x3FED6BF8F8C7F893, 0xA411C28686228644 };

alignas(64) const uint64_t Whirlpool::C4[256] = {
0xC07830D818186018, 0x05AF462623238C23, 0x7EF991B8C6C63FC6, 0x136FCDFBE8E887E8,
0x4CA113CB87872687, 0xA9626D11B8B8DAB8, 0x0805020901010401, 0x426E9E0D4F4F214F,
0xADEE6C9B3636D836, 0x590451FFA6A6A2A6, 0xDEBDB90CD2D26FD2, 0xFB06F70EF5F5F3F5,
0xEF80F2967979F979, 0x5FCEDE306F6FA16F, 0xFCEF3F6D91917E91, 0xAA07A4F852525552,
0x27FDC04760609D60, 0x89766535BCBCCABC, 0xACCD2B379B9B569B, 0x048C018A8E8E028E,
0x71155BD2A3A3B6A3, 0x603C186C0C0C300C, 0xFF8AF6847B7BF17B, 0xB5E16A803535D435,
0xE8693AF51D1D741D, 0x5347DDB3E0E0A7E0, 0xF6ACB321D7D77BD7, 0x5EED999CC2C22FC2,
0x6D965C432E2EB82E, 0x627A96294B4B314B, 0xA321E15DFEFEDFFE, 0x8216AED557574157,
0xA8412ABD15155415, 0x9FB6EEE87777C177, 0xA5EB6E923737DC37, 0x7B56D79EE5E5B3E5,
0x8CD923139F9F469F, 0xD317FD23F0F0E7F0, 0x6A7F94204A4A354A, 0x9E95A944DADA4FDA,
0xFA25B0A258587D58, 0x06CA8FCFC9C903C9, 0x558D527C2929A429, 0x5022145A0A0A280A,
0xE14F7F50B1B1FEB1, 0x691A5DC9A0A0BAA0, 0x7FDAD6146B6BB16B, 0x5CAB17D985852E85,
0x8173673CBDBDCEBD, 0xD234BA8F5D5D695D, 0x8050209010104010, 0xF303F507F4F4F7F4,
0x16C08BDDCBCB0BCB, 0xEDC67CD33E3EF83E, 0x28110A2D05051405, 0x1FE6CE7867678167,
0x7353D597E4E4B7E4, 0x25BB4E0227279C27, 0x3258827341411941, 0x2C9D0BA78B8B168B,
0x510153F6A7A7A6A7, 0xCF94FAB27D7DE97D, 0xDCFB374995956E95, 0x8E9FAD56D8D847D8,
0x8B30EB70FBFBCBFB, 0x2371C1CDEEEE9FEE, 0xC791F8BB7C7CED7C, 0x17E3CC7166668566,
0xA68EA77BDDDD53DD, 0xB84B2EAF17175C17, 0x02468E4547470147, 0x84DC211A9E9E429E,
0x1EC589D4CACA0FCA, 0x75995A582D2DB42D, 0x9179632EBFBFC6BF, 0x381B0E3F07071C07,
0x012347ACADAD8EAD, 0xEA2FB4B05A5A755A, 0x6CB51BEF83833683, 0x85FF66B63333CC33,
0x3FF2C65C63639163, 0x100A041202020802, 0x39384993AAAA92AA, 0xAFA8E2DE7171D971,
0x0ECF8DC6C8C807C8, 0xC87D32D119196419, 0x7270923B49493949, 0x869AAF5FD9D943D9,
0xC31DF931F2F2EFF2, 0x4B48DBA8E3E3ABE3, 0xE22AB6B95B5B715B, 0x34920DBC88881A88,
0xA4C8293E9A9A529A, 0x2DBE4C0B26269826, 0x8DFA64BF3232C832, 0xE94A7D59B0B0FAB0,
0x1B6ACFF2E9E983E9, 0x78331E770F0F3C0F, 0xE6A6B733D5D573D5, 0x74BA1DF480803A80,
0x997C6127BEBEC2BE, 0x26DE87EBCDCD13CD, 0xBDE468893434D034, 0x7A75903248483D48,
0xAB24E354FFFFDBFF, 0xF78FF48D7A7AF57A, 0xF4EA3D6490907A90, 0xC23EBE9D5F5F615F,
0x1DA0403D20208020, 0x67D5D00F6868BD68, 0xD07234CA1A1A681A, 0x192C41B7AEAE82AE,
0xC95E757DB4B4EAB4, 0x9A19A8CE54544D54, 0xECE53B7F93937693, 0x0DAA442F22228822,
0x07E9C86364648D64, 0xDB12FF2AF1F1E3F1, 0xBFA2E6CC7373D173, 0x905A248212124812,
0x3A5D807A40401D40, 0x4028104808082008, 0x56E89B95C3C32BC3, 0x337BC5DFECEC97EC,
0x9690AB4DDBDB4BDB, 0x611F5FC0A1A1BEA1, 0x1C8307918D8D0E8D, 0xF5C97AC83D3DF43D,
0xCCF1335B97976697, 0x0000000000000000, 0x36D483F9CFCF1BCF, 0x4587566E2B2BAC2B,
0x97B3ECE17676C576, 0x64B019E682823282, 0xFEA9B128D6D67FD6, 0xD87736C31B1B6C1B,
0xC15B7774B5B5EEB5, 0x112943BEAFAF86AF, 0x77DFD41D6A6AB56A, 0xBA0DA0EA50505D50,
0x124C8A5745450945, 0xCB18FB38F3F3EBF3, 0x9DF060AD3030C030, 0x2B74C3C4EFEF9BEF,
0xE5C37EDA3F3FFC3F, 0x921CAAC755554955, 0x791059DBA2A2B2A2, 0x0365C9E9EAEA8FEA,
0x0FECCA6A65658965, 0xB9686903BABAD2BA, 0x65935E4A2F2FBC2F, 0x4EE79D8EC0C027C0,
0xBE81A160DEDE5FDE, 0xE06C38FC1C1C701C, 0xBB2EE746FDFDD3FD, 0x52649A1F4D4D294D,
0xE4E0397692927292, 0x8FBCEAFA7575C975, 0x301E0C3606061806, 0x249809AE8A8A128A,
0xF940794BB2B2F2B2, 0x6359D185E6E6BFE6, 0x70361C7E0E0E380E, 0xF8633EE71F1F7C1F,
0x37F7C45562629562, 0xEEA3B53AD4D477D4, 0x29324D81A8A89AA8, 0xC4F4315296966296,
0x9B3AEF62F9F9C3F9, 0x66F697A3C5C533C5, 0x35B14A1025259425, 0xF220B2AB59597959,
0x54AE15D084842A84, 0xB7A7E4C57272D572, 0xD5DD72EC3939E439, 0x5A6198164C4C2D4C,
0xCA3BBC945E5E655E, 0xE785F09F7878FD78, 0xDDD870E53838E038, 0x148605988C8C0A8C,
0xC6B2BF17D1D163D1, 0x410B57E4A5A5AEA5, 0x434DD9A1E2E2AFE2, 0x2FF8C24E61619961,
0xF1457B42B3B3F6B3, 0x15A5423421218421, 0x94D625089C9C4A9C, 0xF0663CEE1E1E781E,
0x2252866143431143, 0x76FC93B1C7C73BC7, 0xB32BE54FFCFCD7FC, 0x2014082404041004,
0xB208A2E351515951, 0xBCC72F2599995E99, 0x4FC4DA226D6DA96D, 0x68391A650D0D340D,
0x8335E979FAFACFFA, 0xB684A369DFDF5BDF, 0xD79BFCA97E7EE57E, 0x3DB4481924249024,
0xC5D776FE3B3BEC3B, 0x313D4B9AABAB96AB, 0x3ED181F0CECE1FCE, 0x8855229911114411,
0x0C8903838F8F068F, 0x4A6B9C044E4E254E, 0xD1517366B7B7E6B7, 0x0B60CBE0EBEB8BEB,
0xFDCC78C13C3CF03C, 0x7CBF1FFD81813E81, 0xD4FE354094946A94, 0xEB0CF31CF7F7FBF7,
0xA1676F18B9B9DEB9, 0x985F268B13134C13, 0x7D9C58512C2CB02C, 0xD6B8BB05D3D36BD3,
0x6B5CD38CE7E7BBE7, 0x57CBDC396E6EA56E, 0x6EF395AAC4C437C4, 0x180F061B03030C03,
0x8A13ACDC56564556, 0x1A49885E44440D44, 0xDF9EFEA07F7FE17F, 0x21374F88A9A99EA9,
0x4D8254672A2AA82A, 0xB16D6B0ABBBBD6BB, 0x46E29F87C1C123C1, 0xA202A6F153535153,
0xAE8BA572DCDC57DC, 0x582716530B0B2C0B, 0x9CD327019D9D4E9D, 0x47C1D82B6C6CAD6C,
0x95F562A43131C431, 0x87B9E8F37474CD74, 0xE309F115F6F6FFF6, 0x0A438C4C46460546,
0x092645A5ACAC8AAC, 0x3C970FB589891E89, 0xA04428B414145014, 0x5B42DFBAE1E1A3E1,
0xB04E2CA616165816, 0xCDD274F73A3AE83A, 0x6FD0D2066969B969, 0x482D124109092409,
0xA7ADE0D77070DD70, 0xD954716FB6B6E2B6, 0xCEB7BD1ED0D067D0, 0x3B7EC7D6EDED93ED,
0x2EDB85E2CCCC17CC, 0x2A57846842421542, 0xB4C22D2C98985A98, 0x490E55EDA4A4AAA4,
0x5D8850752828A028, 0xDA31B8865C5C6D5C, 0x933FED6BF8F8C7F8, 0x44A411C286862286 };

alignas(64) const uint64_t Whirlpool::C5[256] = {
0x18C07830D8181860, 0x2305AF462623238C, 0xC67EF991B8C6C63F, 0xE8136FCDFBE8E887,
0x874CA113CB878726, 0xB8A9626D11B8B8DA, 0x0108050209010104, 0x4F426E9E0D4F4F21,
0x36ADEE6C9B3636D8, 0xA6590451FFA6A6A2, 0xD2DEBDB90CD2D26F, 0xF5FB06F70EF5F5F3,
0x79EF80F2967979F9, 0x6F5FCEDE306F6FA1, 0x91FCEF3F6D91917E, 0x52AA07A4F8525255,
0x6027FDC04760609D, 0xBC89766535BCBCCA, 0x9BACCD2B379B9B56, 0x8E048C018A8E8E02,
0xA371155BD2A3A3B6, 0x0C603C186C0C0C30, 0x7BFF8AF6847B7BF1, 0x35B5E16A803535D4,
0x1DE8693AF51D1D74, 0xE05347DDB3E0E0A7, 0xD7F6ACB321D7D77B, 0xC25EED999CC2C22F,
0x2E6D965C432E2EB8, 0x4B627A96294B4B31, 0xFEA321E15DFEFEDF, 0x578216AED5575741,
0x15A8412ABD151554, 0x779FB6EEE87777C1, 0x37A5EB6E923737DC, 0xE57B56D79EE5E5B3,
0x9F8CD923139F9F46, 0xF0D317FD23F0F0E7, 0x4A6A7F94204A4A35, 0xDA9E95A944DADA4F,
0x58FA25B0A258587D, 0xC906CA8FCFC9C903, 0x29558D527C2929A4, 0x0A5022145A0A0A28,
0xB1E14F7F50B1B1FE, 0xA0691A5DC9A0A0BA, 0x6B7FDAD6146B6BB1, 0x855CAB17D985852E,
0xBD8173673CBDBDCE, 0x5DD234BA8F5D5D69, 0x1080502090101040, 0xF4F303F507F4F4F7,
0xCB16C08BDDCBCB0B, 0x3EEDC67CD33E3EF8, 0x0528110A2D050514, 0x671FE6CE78676781,
0xE47353D597E4E4B7, 0x2725BB4E0227279C, 0x4132588273414119, 0x8B2C9D0BA78B8B16,
0xA7510153F6A7A7A6, 0x7DCF94FAB27D7DE9, 0x95DCFB374995956E, 0xD88E9FAD56D8D847,
0xFB8B30EB70FBFBCB, 0xEE2371C1CDEEEE9F, 0x7CC791F8BB7C7CED, 0x6617E3CC71666685,
0xDDA68EA77BDDDD53, 0x17B84B2EAF17175C, 0x4702468E45474701, 0x9E84DC211A9E9E42,
0xCA1EC589D4CACA0F, 0x2D75995A582D2DB4, 0xBF9179632EBFBFC6, 0x07381B0E3F07071C,
0xAD012347ACADAD8E, 0x5AEA2FB4B05A5A75, 0x836CB51BEF838336, 0x3385FF66B63333CC,
0x633FF2C65C636391, 0x02100A0412020208, 0xAA39384993AAAA92, 0x71AFA8E2DE7171D9,
0xC80ECF8DC6C8C807, 0x19C87D32D1191964, 0x497270923B494939, 0xD9869AAF5FD9D943,
0xF2C31DF931F2F2EF, 0xE34B48DBA8E3E3AB, 0x5BE22AB6B95B5B71, 0x8834920DBC88881A,
0x9AA4C8293E9A9A52, 0x262DBE4C0B262698, 0x328DFA64BF3232C8, 0xB0E94A7D59B0B0FA,
0xE91B6ACFF2E9E983, 0x0F78331E770F0F3C, 0xD5E6A6B733D5D573, 0x8074BA1DF480803A,
0xBE997C6127BEBEC2, 0xCD26DE87EBCDCD13, 0x34BDE468893434D0, 0x487A75903248483D,
0xFFAB24E354FFFFDB, 0x7AF78FF48D7A7AF5, 0x90F4EA3D6490907A, 0x5FC23EBE9D5F5F61,
0x201DA0403D202080, 0x6867D5D00F6868BD, 0x1AD07234CA1A1A68, 0xAE192C41B7AEAE82,
0xB4C95E757DB4B4EA, 0x549A19A8CE54544D, 0x93ECE53B7F939376, 0x220DAA442F222288,
0x6407E9C86364648D, 0xF1DB12FF2AF1F1E3, 0x73BFA2E6CC7373D1, 0x12905A2482121248,
0x403A5D807A40401D, 0x0840281048080820, 0xC356E89B95C3C32B, 0xEC337BC5DFECEC97,
0xDB9690AB4DDBDB4B, 0xA1611F5FC0A1A1BE, 0x8D1C8307918D8D0E, 0x3DF5C97AC83D3DF4,
0x97CCF1335B979766, 0x0000000000000000, 0xCF36D483F9CFCF1B, 0x2B4587566E2B2BAC,
0x7697B3ECE17676C5, 0x8264B019E6828232, 0xD6FEA9B128D6D67F, 0x1BD87736C31B1B6C,
0xB5C15B7774B5B5EE, 0xAF112943BEAFAF86, 0x6A77DFD41D6A6AB5, 0x50BA0DA0EA50505D,
0x45124C8A57454509, 0xF3CB18FB38F3F3EB, 0x309DF060AD3030C0, 0xEF2B74C3C4EFEF9B,
0x3FE5C37EDA3F3FFC, 0x55921CAAC7555549, 0xA2791059DBA2A2B2, 0xEA0365C9E9EAEA8F,
0x650FECCA6A656589, 0xBAB9686903BABAD2, 0x2F65935E4A2F2FBC, 0xC04EE79D8EC0C027,
0xDEBE81A160DEDE5F, 0x1CE06C38FC1C1C70, 0xFDBB2EE746FDFDD3, 0x4D52649A1F4D4D29,
0x92E4E03976929272, 0x758FBCEAFA7575C9, 0x06301E0C36060618, 0x8A249809AE8A8A12,
0xB2F940794BB2B2F2, 0xE66359D185E6E6BF, 0x0E70361C7E0E0E38, 0x1FF8633EE71F1F7C,
0x6237F7C455626295, 0xD4EEA3B53AD4D477, 0xA829324D81A8A89A, 0x96C4F43152969662,
0xF99B3AEF62F9F9C3, 0xC566F697A3C5C533, 0x2535B14A10252594, 0x59F220B2AB595979,
0x8454AE15D084842A, 0x72B7A7E4C57272D5, 0x39D5DD72EC3939E4, 0x4C5A6198164C4C2D,
0x5ECA3BBC945E5E65, 0x78E785F09F7878FD, 0x38DDD870E53838E0, 0x8C148605988C8C0A,
0xD1C6B2BF17D1D163, 0xA5410B57E4A5A5AE, 0xE2434DD9A1E2E2AF, 0x612FF8C24E616199,
0xB3F1457B42B3B3F6, 0x2115A54234212184, 0x9C94D625089C9C4A, 0x1EF0663CEE1E1E78,
0x4322528661434311, 0xC776FC93B1C7C73B, 0xFCB32BE54FFCFCD7, 0x0420140824040410,
0x51B208A2E3515159, 0x99BCC72F2599995E, 0x6D4FC4DA226D6DA9, 0x0D68391A650D0D34,
0xFA8335E979FAFACF, 0xDFB684A369DFDF5B, 0x7ED79BFCA97E7EE5, 0x243DB44819242490,
0x3BC5D776FE3B3BEC, 0xAB313D4B9AABAB96, 0xCE3ED181F0CECE1F, 0x1188552299111144,
0x8F0C8903838F8F06, 0x4E4A6B9C044E4E25, 0xB7D1517366B7B7E6, 0xEB0B60CBE0EBEB8B,
0x3CFDCC78C13C3CF0, 0x817CBF1FFD81813E, 0x94D4FE354094946A, 0xF7EB0CF31CF7F7FB,
0xB9A1676F18B9B9DE, 0x13985F268B13134C, 0x2C7D9C58512C2CB0, 0xD3D6B8BB05D3D36B,
0xE76B5CD38CE7E7BB, 0x6E57CBDC396E6EA5, 0xC46EF395AAC4C437, 0x03180F061B03030C,
0x568A13ACDC565645, 0x441A49885E44440D, 0x7FDF9EFEA07F7FE1, 0xA921374F88A9A99E,
0x2A4D8254672A2AA8, 0xBBB16D6B0ABBBBD6, 0xC146E29F87C1C123, 0x53A202A6F1535351,
0xDCAE8BA572DCDC57, 0x0B582716530B0B2C, 0x9D9CD327019D9D4E, 0x6C47C1D82B6C6CAD,
0x3195F562A43131C4, 0x7487B9E8F37474CD, 0xF6E309F115F6F6FF, 0x460A438C4C464605,
0xAC092645A5ACAC8A, 0x893C970FB589891E, 0x14A04428B4141450, 0xE15B42DFBAE1E1A3,
0x16B04E2CA6161658, 0x3ACDD274F73A3AE8, 0x696FD0D2066969B9, 0x09482D1241090924,
0x70A7ADE0D77070DD, 0xB6D954716FB6B6E2, 0xD0CEB7BD1ED0D067, 0xED3B7EC7D6EDED93,
0xCC2EDB85E2CCCC17, 0x422A578468424215, 0x98B4C22D2C98985A, 0xA4490E55EDA4A4AA,
0x285D8850752828A0, 0x5CDA31B8865C5C6D, 0xF8933FED6BF8F8C7, 0x8644A411C2868622 };

alignas(64) const uint64_t Whirlpool::C6[256] = {
0x6018C07830D81818, 0x8C2305AF46262323, 0x3FC67EF991B8C6C6, 0x87E8136FCDFBE8E8,
0x26874CA113CB8787, 0xDAB8A9626D11B8B8, 0x0401080502090101, 0x214F426E9E0D4F4F,
0xD836ADEE6C9B3636, 0xA2A6590451FFA6A6, 0x6FD2DEBDB90CD2D2, 0xF3F5FB06F70EF5F5,
0xF979EF80F2967979, 0xA16F5FCEDE306F6F, 0x7E91FCEF3F6D9191, 0x5552AA07A4F85252,
0x9D6027FDC0476060, 0xCABC89766535BCBC, 0x569BACCD2B379B9B, 0x028E048C018A8E8E,
0xB6A371155BD2A3A3, 0x300C603C186C0C0C, 0xF17BFF8AF6847B7B, 0xD435B5E16A803535,
0x741DE8693AF51D1D, 0xA7E05347DDB3E0E0, 0x7BD7F6ACB321D7D7, 0x2FC25EED999CC2C2,
0xB82E6D965C432E2E, 0x314B627A96294B4B, 0xDFFEA321E15DFEFE, 0x41578216AED55757,
0x5415A8412ABD1515, 0xC1779FB6EEE87777, 0xDC37A5EB6E923737, 0xB3E57B56D79EE5E5,
0x469F8CD923139F9F, 0xE7F0D317FD23F0F0, 0x354A6A7F94204A4A, 0x4FDA9E95A944DADA,
0x7D58FA25B0A25858, 0x03C906CA8FCFC9C9, 0xA429558D527C2929, 0x280A5022145A0A0A,
0xFEB1E14F7F50B1B1, 0xBAA0691A5DC9A0A0, 0xB16B7FDAD6146B6B, 0x2E855CAB17D98585,
0xCEBD8173673CBDBD, 0x695DD234BA8F5D5D, 0x4010805020901010, 0xF7F4F303F507F4F4,
0x0BCB16C08BDDCBCB, 0xF83EEDC67CD33E3E, 0x140528110A2D0505, 0x81671FE6CE786767,
0xB7E47353D597E4E4, 0x9C2725BB4E022727, 0x1941325882734141, 0x168B2C9D0BA78B8B,
0xA6A7510153F6A7A7, 0xE97DCF94FAB27D7D, 0x6E95DCFB37499595, 0x47D88E9FAD56D8D8,
0xCBFB8B30EB70FBFB, 0x9FEE2371C1CDEEEE, 0xED7CC791F8BB7C7C, 0x856617E3CC716666,
0x53DDA68EA77BDDDD, 0x5C17B84B2EAF1717, 0x014702468E454747, 0x429E84DC211A9E9E,
0x0FCA1EC589D4CACA, 0xB42D75995A582D2D, 0xC6BF9179632EBFBF, 0x1C07381B0E3F0707,
0x8EAD012347ACADAD, 0x755AEA2FB4B05A5A, 0x36836CB51BEF8383, 0xCC3385FF66B63333,
0x91633FF2C65C6363, 0x0802100A04120202, 0x92AA39384993AAAA, 0xD971AFA8E2DE7171,
0x07C80ECF8DC6C8C8, 0x6419C87D32D11919, 0x39497270923B4949, 0x43D9869AAF5FD9D9,
0xEFF2C31DF931F2F2, 0xABE34B48DBA8E3E3, 0x715BE22AB6B95B5B, 0x1A8834920DBC8888,
0x529AA4C8293E9A9A, 0x98262DBE4C0B2626, 0xC8328DFA64BF3232, 0xFAB0E94A7D59B0B0,
0x83E91B6ACFF2E9E9, 0x3C0F78331E770F0F, 0x73D5E6A6B733D5D5, 0x3A8074BA1DF48080,
0xC2BE997C6127BEBE, 0x13CD26DE87EBCDCD, 0xD034BDE468893434, 0x3D487A7590324848,
0xDBFFAB24E354FFFF, 0xF57AF78FF48D7A7A, 0x7A90F4EA3D649090, 0x615FC23EBE9D5F5F,
0x80201DA0403D2020, 0xBD6867D5D00F6868, 0x681AD07234CA1A1A, 0x82AE192C41B7AEAE,
0xEAB4C95E757DB4B4, 0x4D549A19A8CE5454, 0x7693ECE53B7F9393, 0x88220DAA442F2222,
0x8D6407E9C8636464, 0xE3F1DB12FF2AF1F1, 0xD173BFA2E6CC7373, 0x4812905A24821212,
0x1D403A5D807A4040, 0x2008402810480808, 0x2BC356E89B95C3C3, 0x97EC337BC5DFECEC,
0x4BDB9690AB4DDBDB, 0xBEA1611F5FC0A1A1, 0x0E8D1C8307918D8D, 0xF43DF5C97AC83D3D,
0x6697CCF1335B9797, 0x0000000000000000, 0x1BCF36D483F9CFCF, 0xAC2B4587566E2B2B,
0xC57697B3ECE17676, 0x328264B019E68282, 0x7FD6FEA9B128D6D6, 0x6C1BD87736C31B1B,
0xEEB5C15B7774B5B5, 0x86AF112943BEAFAF, 0xB56A77DFD41D6A6A, 0x5D50BA0DA0EA5050,
0x0945124C8A574545, 0xEBF3CB18FB38F3F3, 0xC0309DF060AD3030, 0x9BEF2B74C3C4EFEF,
0xFC3FE5C37EDA3F3F, 0x4955921CAAC75555, 0xB2A2791059DBA2A2, 0x8FEA0365C9E9EAEA,
0x89650FECCA6A6565, 0xD2BAB9686903BABA, 0xBC2F65935E4A2F2F, 0x27C04EE79D8EC0C0,
0x5FDEBE81A160DEDE, 0x701CE06C38FC1C1C, 0xD3FDBB2EE746FDFD, 0x294D52649A1F4D4D,
0x7292E4E039769292, 0xC9758FBCEAFA7575, 0x1806301E0C360606, 0x128A249809AE8A8A,
0xF2B2F940794BB2B2, 0xBFE66359D185E6E6, 0x380E70361C7E0E0E, 0x7C1FF8633EE71F1F,
0x956237F7C4556262, 0x77D4EEA3B53AD4D4, 0x9AA829324D81A8A8, 0x6296C4F431529696,
0xC3F99B3AEF62F9F9, 0x33C566F697A3C5C5, 0x942535B14A102525, 0x7959F220B2AB5959,
0x2A8454AE15D08484, 0xD572B7A7E4C57272, 0xE439D5DD72EC3939, 0x2D4C5A6198164C4C,
0x655ECA3BBC945E5E, 0xFD78E785F09F7878, 0xE038DDD870E53838, 0x0A8C148605988C8C,
0x63D1C6B2BF17D1D1, 0xAEA5410B57E4A5A5, 0xAFE2434DD9A1E2E2, 0x99612FF8C24E6161,
0xF6B3F1457B42B3B3, 0x842115A542342121, 0x4A9C94D625089C9C, 0x781EF0663CEE1E1E,
0x1143225286614343, 0x3BC776FC93B1C7C7, 0xD7FCB32BE54FFCFC, 0x1004201408240404,
0x5951B208A2E35151, 0x5E99BCC72F259999, 0xA96D4FC4DA226D6D, 0x340D68391A650D0D,
0xCFFA8335E979FAFA, 0x5BDFB684A369DFDF, 0xE57ED79BFCA97E7E, 0x90243DB448192424,
0xEC3BC5D776FE3B3B, 0x96AB313D4B9AABAB, 0x1FCE3ED181F0CECE, 0x4411885522991111,
0x068F0C8903838F8F, 0x254E4A6B9C044E4E, 0xE6B7D1517366B7B7, 0x8BEB0B60CBE0EBEB,
0xF03CFDCC78C13C3C, 0x3E817CBF1FFD8181, 0x6A94D4FE35409494, 0xFBF7EB0CF31CF7F7,
0xDEB9A1676F18B9B9, 0x4C13985F268B1313, 0xB02C7D9C58512C2C, 0x6BD3D6B8BB05D3D3,
0xBBE76B5CD38CE7E7, 0xA56E57CBDC396E6E, 0x37C46EF395AAC4C4, 0x0C03180F061B0303,
0x45568A13ACDC5656, 0x0D441A49885E4444, 0xE17FDF9EFEA07F7F, 0x9EA921374F88A9A9,
0xA82A4D8254672A2A, 0xD6BBB16D6B0ABBBB, 0x23C146E29F87C1C1, 0x5153A202A6F15353,
0x57DCAE8BA572DCDC, 0x2C0B582716530B0B, 0x4E9D9CD327019D9D, 0xAD6C47C1D82B6C6C,
0xC43195F562A43131, 0xCD7487B9E8F37474, 0xFFF6E309F115F6F6, 0x05460A438C4C4646,
0x8AAC092645A5ACAC, 0x1E893C970FB58989, 0x5014A04428B41414, 0xA3E15B42DFBAE1E1,
0x5816B04E2CA61616, 0xE83ACDD274F73A3A, 0xB9696FD0D2066969, 0x2409482D12410909,
0xDD70A7ADE0D77070, 0xE2B6D954716FB6B6, 0x67D0CEB7BD1ED0D0, 0x93ED3B7EC7D6EDED,
0x17CC2EDB85E2CCCC, 0x15422A5784684242, 0x5A98B4C22D2C9898, 0xAAA4490E55EDA4A4,
0xA0285D8850752828, 0x6D5CDA31B8865C5C, 0xC7F8933FED6BF8F8, 0x228644A411C28686 };

alignas(64) const uint64_t Whirlpool::C7[256] = {
0x186018C07830D818, 0x238C2305AF462623, 0xC63FC67EF991B8C6, 0xE887E8136FCDFBE8,
0x8726874CA113CB87, 0xB8DAB8A9626D11B8, 0x0104010805020901, 0x4F214F426E9E0D4F,
0x36D836ADEE6C9B36, 0xA6A2A6590451FFA6, 0xD26FD2DEBDB90CD2, 0xF5F3F5FB06F70EF5,
0x79F979EF80F29679, 0x6FA16F5FCEDE306F, 0x917E91FCEF3F6D91, 0x525552AA07A4F852,
0x609D6027FDC04760, 0xBCCABC89766535BC, 0x9B569BACCD2B379B, 0x8E028E048C018A8E,
0xA3B6A371155BD2A3, 0x0C300C603C186C0C, 0x7BF17BFF8AF6847B, 0x35D435B5E16A8035,
0x1D741DE8693AF51D, 0xE0A7E05347DDB3E0, 0xD77BD7F6ACB321D7, 0xC22FC25EED999CC2,
0x2EB82E6D965C432E, 0x4B314B627A96294B, 0xFEDFFEA321E15DFE, 0x5741578216AED557,
0x155415A8412ABD15, 0x77C1779FB6EEE877, 0x37DC37A5EB6E9237, 0xE5B3E57B56D79EE5,
0x9F469F8CD923139F, 0xF0E7F0D317FD23F0, 0x4A354A6A7F94204A, 0xDA4FDA9E95A944DA,
0x587D58FA25B0A258, 0xC903C906CA8FCFC9, 0x29A429558D527C29, 0x0A280A5022145A0A,
0xB1FEB1E14F7F50B1, 0xA0BAA0691A5DC9A0, 0x6BB16B7FDAD6146B, 0x852E855CAB17D985,
0xBDCEBD8173673CBD, 0x5D695DD234BA8F5D, 0x1040108050209010, 0xF4F7F4F303F507F4,
0xCB0BCB16C08BDDCB, 0x3EF83EEDC67CD33E, 0x05140528110A2D05, 0x6781671FE6CE7867,
0xE4B7E47353D597E4, 0x279C2725BB4E0227, 0x4119413258827341, 0x8B168B2C9D0BA78B,
0xA7A6A7510153F6A7, 0x7DE97DCF94FAB27D, 0x956E95DCFB374995, 0xD847D88E9FAD56D8,
0xFBCBFB8B30EB70FB, 0xEE9FEE2371C1CDEE, 0x7CED7CC791F8BB7C, 0x66856617E3CC7166,
0xDD53DDA68EA77BDD, 0x175C17B84B2EAF17, 0x47014702468E4547, 0x9E429E84DC211A9E,
0xCA0FCA1EC589D4CA, 0x2DB42D75995A582D, 0xBFC6BF9179632EBF, 0x071C07381B0E3F07,
0xAD8EAD012347ACAD, 0x5A755AEA2FB4B05A, 0x8336836CB51BEF83, 0x33CC3385FF66B633,
0x6391633FF2C65C63, 0x020802100A041202, 0xAA92AA39384993AA, 0x71D971AFA8E2DE71,
0xC807C80ECF8DC6C8, 0x196419C87D32D119, 0x4939497270923B49, 0xD943D9869AAF5FD9,
0xF2EFF2C31DF931F2, 0xE3ABE34B48DBA8E3, 0x5B715BE22AB6B95B, 0x881A8834920DBC88,
0x9A529AA4C8293E9A, 0x2698262DBE4C0B26, 0x32C8328DFA64BF32, 0xB0FAB0E94A7D59B0,
0xE983E91B6ACFF2E9, 0x0F3C0F78331E770F, 0xD573D5E6A6B733D5, 0x803A8074BA1DF480,
0xBEC2BE997C6127BE, 0xCD13CD26DE87EBCD, 0x34D034BDE4688934, 0x483D487A75903248,
0xFFDBFFAB24E354FF, 0x7AF57AF78FF48D7A, 0x907A90F4EA3D6490, 0x5F615FC23EBE9D5F,
0x2080201DA0403D20, 0x68BD6867D5D00F68, 0x1A681AD07234CA1A, 0xAE82AE192C41B7AE,
0xB4EAB4C95E757DB4, 0x544D549A19A8CE54, 0x937693ECE53B7F93, 0x2288220DAA442F22,
0x648D6407E9C86364, 0xF1E3F1DB12FF2AF1, 0x73D173BFA2E6CC73, 0x124812905A248212,
0x401D403A5D807A40, 0x0820084028104808, 0xC32BC356E89B95C3, 0xEC97EC337BC5DFEC,
0xDB4BDB9690AB4DDB, 0xA1BEA1611F5FC0A1, 0x8D0E8D1C8307918D, 0x3DF43DF5C97AC83D,
0x976697CCF1335B97, 0x0000000000000000, 0xCF1BCF36D483F9CF, 0x2BAC2B4587566E2B,
0x76C57697B3ECE176, 0x82328264B019E682, 0xD67FD6FEA9B128D6, 0x1B6C1BD87736C31B,
0xB5EEB5C15B7774B5, 0xAF86AF112943BEAF, 0x6AB56A77DFD41D6A, 0x505D50BA0DA0EA50,
0x450945124C8A5745, 0xF3EBF3CB18FB38F3, 0x30C0309DF060AD30, 0xEF9BEF2B74C3C4EF,
0x3FFC3FE5C37EDA3F, 0x554955921CAAC755, 0xA2B2A2791059DBA2, 0xEA8FEA0365C9E9EA,
0x6589650FECCA6A65, 0xBAD2BAB9686903BA, 0x2FBC2F65935E4A2F, 0xC027C04EE79D8EC0,
0xDE5FDEBE81A160DE, 0x1C701CE06C38FC1C, 0xFDD3FDBB2EE746FD, 0x4D294D52649A1F4D,
0x927292E4E0397692, 0x75C9758FBCEAFA75, 0x061806301E0C3606, 0x8A128A249809AE8A,
0xB2F2B2F940794BB2, 0xE6BFE66359D185E6, 0x0E380E70361C7E0E, 0x1F7C1FF8633EE71F,
0x62956237F7C45562, 0xD477D4EEA3B53AD4, 0xA89AA829324D81A8, 0x966296C4F4315296,
0xF9C3F99B3AEF62F9, 0xC533C566F697A3C5, 0x25942535B14A1025, 0x597959F220B2AB59,
0x842A8454AE15D084, 0x72D572B7A7E4C572, 0x39E439D5DD72EC39, 0x4C2D4C5A6198164C,
0x5E655ECA3BBC945E, 0x78FD78E785F09F78, 0x38E038DDD870E538, 0x8C0A8C148605988C,
0xD163D1C6B2BF17D1, 0xA5AEA5410B57E4A5, 0xE2AFE2434DD9A1E2, 0x6199612FF8C24E61,
0xB3F6B3F1457B42B3, 0x21842115A5423421, 0x9C4A9C94D625089C, 0x1E781EF0663CEE1E,
0x4311432252866143, 0xC73BC776FC93B1C7, 0xFCD7FCB32BE54FFC, 0x0410042014082404,
0x515951B208A2E351, 0x995E99BCC72F2599, 0x6DA96D4FC4DA226D, 0x0D340D68391A650D,
0xFACFFA8335E979FA, 0xDF5BDFB684A369DF, 0x7EE57ED79BFCA97E, 0x2490243DB4481924,
0x3BEC3BC5D776FE3B, 0xAB96AB313D4B9AAB, 0xCE1FCE3ED181F0CE, 0x1144118855229911,
0x8F068F0C8903838F, 0x4E254E4A6B9C044E, 0xB7E6B7D1517366B7, 0xEB8BEB0B60CBE0EB,
0x3CF03CFDCC78C13C, 0x813E817CBF1FFD81, 0x946A94D4FE354094, 0xF7FBF7EB0CF31CF7,
0xB9DEB9A1676F18B9, 0x134C13985F268B13, 0x2CB02C7D9C58512C, 0xD36BD3D6B8BB05D3,
0xE7BBE76B5CD38CE7, 0x6EA56E57CBDC396E, 0xC437C46EF395AAC4, 0x030C03180F061B03,
0x5645568A13ACDC56, 0x440D441A49885E44, 0x7FE17FDF9EFEA07F, 0xA99EA921374F88A9,
0x2AA82A4D8254672A, 0xBBD6BBB16D6B0ABB, 0xC123C146E29F87C1, 0x535153A202A6F153,
0xDC57DCAE8BA572DC, 0x0B2C0B582716530B, 0x9D4E9D9CD327019D, 0x6CAD6C47C1D82B6C,
0x31C43195F562A431, 0x74CD7487B9E8F374, 0xF6FFF6E309F115F6, 0x4605460A438C4C46,
0xAC8AAC092645A5AC, 0x891E893C970FB589, 0x145014A04428B414, 0xE1A3E15B42DFBAE1,
0x165816B04E2CA616, 0x3AE83ACDD274F73A, 0x69B9696FD0D20669, 0x092409482D124109,
0x70DD70A7ADE0D770, 0xB6E2B6D954716FB6, 0xD067D0CEB7BD1ED0, 0xED93ED3B7EC7D6ED,
0xCC17CC2EDB85E2CC, 0x4215422A57846842, 0x985A98B4C22D2C98, 0xA4AAA4490E55EDA4,
0x28A0285D88507528, 0x5C6D5CDA31B8865C, 0xF8C7F8933FED6BF8, 0x86228644A411C286 };

}
