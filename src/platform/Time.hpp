#pragma once

#include <chrono>

namespace coomer {

inline double nowSeconds() {
    using clock = std::chrono::steady_clock;
    auto now = clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

}  // namespace coomer
