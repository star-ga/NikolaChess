
#pragma once
#include <string>

struct CPUFeatures {
    bool avx2{false};
    bool avx512f{false};
    bool bmi2{false};
    bool popcnt{false};
};

CPUFeatures detect_cpu_features();
std::string cpu_features_string(const CPUFeatures& f);
