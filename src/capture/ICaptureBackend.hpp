#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "capture/CaptureTypes.hpp"

namespace coomer {

class ICaptureBackend {
public:
    virtual ~ICaptureBackend() = default;
    virtual std::string name() const = 0;
    virtual bool isAvailable() const = 0;
    virtual std::vector<MonitorInfo> listMonitors() = 0;
    virtual CaptureResult captureOnce(
        std::optional<std::string> monitorNameHint) = 0;
};

}  // namespace coomer
