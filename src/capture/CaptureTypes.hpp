#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace coomer {

struct ImageRGBA {
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> rgba;
};

struct MonitorInfo {
    std::string name;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float scale = 1.0f;
    bool primary = false;
};

struct CaptureResult {
    ImageRGBA image;
    std::vector<MonitorInfo> monitors;
    int selectedMonitorIndex = -1;
};

}  // namespace coomer
