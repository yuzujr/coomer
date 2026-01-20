#pragma once

#include <memory>

#include "capture/ICaptureBackend.hpp"

namespace coomer {

std::unique_ptr<ICaptureBackend> CreateBackendPortalScreenshot();

}  // namespace coomer
