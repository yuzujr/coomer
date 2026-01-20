#pragma once

#include <memory>

#include "capture/ICaptureBackend.hpp"

namespace coomer {

std::unique_ptr<ICaptureBackend> CreateBackendX11();

}  // namespace coomer
