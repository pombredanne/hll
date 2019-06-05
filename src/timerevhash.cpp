#define DUMMY_INVERSE
#include "hash.h"
#include "common.h"
using namespace sketch;
using namespace hash;

// from Facebook's Folly
template <typename T>
void doNotOptimizeAway(const T& datum) {
  asm volatile("" ::"r"(datum));
}


// Geoffrey Irving
// https://naml.us/blog/tag/thomas-wang
INLINE uint64_t irving_inv_hash(uint64_t key) {
  uint64_t tmp;
  // Invert key = key + (key << 31)
  tmp = key-(key<<31);
  key = key-(tmp<<31);
  // Invert key = key ^ (key >> 28)
  tmp = key^key>>28;
  key = key^tmp>>28;
  // Invert key *= 21
  key *= 14933078535860113213u;
  // Invert key = key ^ (key >> 14)
  tmp = key^key>>14;
  tmp = key^tmp>>14;
  tmp = key^tmp>>14;
  key = key^tmp>>14;
  // Invert key *= 265
  key *= 15244667743933553977u;
  // Invert key = key ^ (key >> 24)
  tmp = key^key>>24;
  key = key^tmp>>24;
  // Invert key = (~key) + (key << 21)
  tmp = ~key;
  tmp = ~(key-(tmp<<21));
  tmp = ~(key-(tmp<<21));
  key = ~(key-(tmp<<21));
  return key;
}

int main() {
    WangHash hash;
    uint64_t seed1 = hash(uint64_t(1337));
    hash::MultiplyAddXoRot<33> gen1(seed1, hash(seed1));
    hash::MultiplyAddXor gen2(seed1);
    hash::XorMultiply gen3(seed1, hash(seed1));
    hash::FusedReversible3<InvMul, RotL33, MultiplyAddXoRot<16>> gen4(seed1, hash(seed1));
    XorMultiply gen5(seed1, hash(seed1));
    hash::KWiseIndependentPolynomialHash<4> fivewise_gamgee;
    MurFinHash mfh;
    std::array<size_t, 8> arr{0};
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t accum = 0;
    uint64_t nelem = 1000000000;
    for(uint64_t i = 0; i < nelem; ++i) {
        doNotOptimizeAway(irving_inv_hash(hash(uint64_t(i))));
    }
    auto end = std::chrono::high_resolution_clock::now();
    arr[0] = size_t(std::chrono::nanoseconds(end - start).count());
    std::fprintf(stderr, "diff: %zu. accum %zu\n", size_t(std::chrono::nanoseconds(end - start).count()), size_t(accum));
#define DO_THING(hasher, namer, ind) \
    start = std::chrono::high_resolution_clock::now();\
    accum = 0;\
    for(uint64_t i = 0; i < nelem; ++i) {\
       doNotOptimizeAway(hasher.inverse(hasher(i)));\
    }\
    end = std::chrono::high_resolution_clock::now();\
    arr[ind] = size_t(std::chrono::nanoseconds(end - start).count());\
    std::fprintf(stderr, "for %s diff: %zu. accum %zu\n", namer, size_t(std::chrono::nanoseconds(end - start).count()), size_t(accum));

    DO_THING(gen1, "multiplyaddxorot33", 1)
    DO_THING(gen2, "mulyaddor", 2)
    DO_THING(gen3, "xormult", 3)
    DO_THING(gen4, "rotxorrot", 4)
    DO_THING(fivewise_gamgee, "fivewise", 5)
    DO_THING(gen5, "mul-bf-rrot31", 6)
    DO_THING(mfh, "murfinhash", 7)
    for(size_t i = 1; i < arr.size(); ++i)
        std::fprintf(stderr, "%zu is %lf as fast as WangHash\n", i, double(arr[0]) / arr[i]);
}
