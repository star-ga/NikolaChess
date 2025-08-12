
#include "cpu_features.h"
#ifdef __x86_64__
#include <cpuid.h>
#endif
#include <sstream>

static void cpuid(unsigned int leaf, unsigned int subleaf, unsigned int& a, unsigned int& b, unsigned int& c, unsigned int& d) {
#ifdef __x86_64__
    __cpuid_count(leaf, subleaf, a, b, c, d);
#else
    a=b=c=d=0;
#endif
}

CPUFeatures detect_cpu_features() {
    CPUFeatures f;
#ifdef __x86_64__
    unsigned int a,b,c,d;
    // Leaf 1 for POPCNT (ECX bit 23) actually POPCNT is SSE4.2? We'll check leaf 1 ECX bit 23 = POPCNT is actually bit 23 for POPCNT in ECX of leaf 1? POPCNT is ECX bit 23.
    __cpuid(1, a,b,c,d);
    f.popcnt = (c & (1u<<23)) != 0;
#endif
    return f;
}

std::string cpu_features_string(const CPUFeatures& f) {
    std::ostringstream oss;
    oss << (f.avx2 ? "AVX2 " : "")
        << (f.avx512f ? "AVX-512F " : "")
        << (f.bmi2 ? "BMI2 " : "")
        << (f.popcnt ? "POPCNT " : "");
    auto s = oss.str();
    if (s.empty()) s = "(none)";
    return s;
}
