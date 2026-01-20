#pragma once

#include <memory>

#include "window/IWindow.hpp"

namespace coomer {

std::unique_ptr<IWindow> CreateX11WindowGlx(const WindowConfig& config);

}  // namespace coomer
