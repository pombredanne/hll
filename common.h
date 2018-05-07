#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "sseutil.h"
#include "util.h"
#include "math.h"
#include "unistd.h"
#include "x86intrin.h"
#include "kthread.h"
#if ZWRAP_USE_ZSTD
#  include "zstd_zlibwrapper.h"
#else
#  include <zlib.h>
#endif
#include "libpopcnt/libpopcnt.h"

#ifndef _VEC_H__
#  define NO_SLEEF
#  define NO_BLAZE
#  include "vec.h" // Import vec.h, but disable blaze and sleef.
#endif

#ifndef HAS_AVX_512
#  define HAS_AVX_512 (_FEATURE_AVX512F || _FEATURE_AVX512ER || _FEATURE_AVX512PF || _FEATURE_AVX512CD || __AVX512BW__ || __AVX512CD__ || __AVX512F__)
#endif

#ifndef INLINE
#  if __GNUC__ || __clang__
#    define INLINE __attribute__((always_inline)) inline
#  else
#    define INLINE inline
#  endif
#endif

#ifdef INCLUDE_CLHASH_H_
#  define ENABLE_CLHASH 1
#elif ENABLE_CLHASH
#  include "clhash.h"
#endif

#if defined(NDEBUG)
#  if NDEBUG == 0
#    undef NDEBUG
#  endif
#endif

namespace common {
using namespace std::literals;

using std::uint64_t;
using std::uint32_t;
using std::uint16_t;
using std::uint8_t;
using std::size_t;
using Space = vec::SIMDTypes<uint64_t>;
using Type  = typename vec::SIMDTypes<uint64_t>::Type;
using VType = typename vec::SIMDTypes<uint64_t>::VType;

static INLINE uint64_t finalize(uint64_t key) {
    // Murmur3 finalizer
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccd;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53;
    key ^= key >> 33;
    return key;
}

// Thomas Wang hash
// Original site down, available at https://naml.us/blog/tag/thomas-wang
// This is our core 64-bit hash.
// It a bijection within [0,1<<64)
// and can be inverted with irving_inv_hash.
struct WangHash {
    INLINE auto operator()(uint64_t key) const {
          key = (~key) + (key << 21); // key = (key << 21) - key - 1;
          key = key ^ (key >> 24);
          key = (key + (key << 3)) + (key << 8); // key * 265
          key = key ^ (key >> 14);
          key = (key + (key << 2)) + (key << 4); // key * 21
          key = key ^ (key >> 28);
          key = key + (key << 31);
          return key;
    }
    INLINE Type operator()(Type element) const {
        VType key = Space::add(Space::slli(element, 21), ~element); // key = (~key) + (key << 21);
        key = Space::srli(key.simd_, 24) ^ key.simd_; //key ^ (key >> 24)
        key = Space::add(Space::add(Space::slli(key.simd_, 3), Space::slli(key.simd_, 8)), key.simd_); // (key + (key << 3)) + (key << 8);
        key = key.simd_ ^ Space::srli(key.simd_, 14);  // key ^ (key >> 14);
        key = Space::add(Space::add(Space::slli(key.simd_, 2), Space::slli(key.simd_, 4)), key.simd_); // (key + (key << 2)) + (key << 4); // key * 21
        key = key.simd_ ^ Space::srli(key.simd_, 28); // key ^ (key >> 28);
        key = Space::add(Space::slli(key.simd_, 31), key.simd_);    // key + (key << 31);
        return key.simd_;
    }
};

struct MurFinHash {
    INLINE uint64_t operator()(uint64_t key) const {
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccd;
        key ^= key >> 33;
        key *= 0xc4ceb9fe1a85ec53;
        key ^= key >> 33;
        return key;
    }
    INLINE Type operator()(Type key) const {
        return this->operator()(*((VType *)&key));
    }
    INLINE Type operator()(VType key) const {
#if (HAS_AVX_512)
        static const Type mul1 = Space::set1(0xff51afd7ed558ccd);
        static const Type mul2 = Space::set1(0xc4ceb9fe1a85ec53);
#endif

#if !NDEBUG
        auto save = key.arr_[0];
#endif
        key = Space::srli(key.simd_, 33) ^ key.simd_;  // h ^= h >> 33;
#if (HAS_AVX_512) == 0
        key.for_each([](uint64_t &x) {x *= 0xff51afd7ed558ccd;});
#  else
        key = Space::mullo(key.simd_, mul1); // h *= 0xff51afd7ed558ccd;
#endif
        key = Space::srli(key.simd_, 33) ^ key.simd_;  // h ^= h >> 33;
#if (HAS_AVX_512) == 0
        key.for_each([](uint64_t &x) {x *= 0xc4ceb9fe1a85ec53;});
#  else
        key = Space::mullo(key.simd_, mul2); // h *= 0xc4ceb9fe1a85ec53;
#endif
        key = Space::srli(key.simd_, 33) ^ key.simd_;  // h ^= h >> 33;
        assert(this->operator()(save) == key.arr_[0]);
        return key.simd_;
    }
};

template<typename T>
static constexpr inline bool is_pow2(T val) {
    return val && (val & (val - 1)) == 0;
}
} // namespace common
