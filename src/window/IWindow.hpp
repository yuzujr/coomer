#pragma once

#include <string>

namespace coomer {

struct InputState {
    double mouseX = 0.0;
    double mouseY = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    double wheelDelta = 0.0;
    bool mouseLeft = false;
    bool mouseRight = false;
    bool keyCtrl = false;
    bool keyShift = false;
    bool keyQ = false;
    bool keyA = false;
};

struct WindowConfig {
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    bool fullscreen = true;
    bool overlay = false;
    std::string title = "coomer";
};

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual InputState input() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void swap() = 0;
    virtual void* glGetProcAddress(const char* name) = 0;
};

}  // namespace coomer
