#pragma once

#include <cstdint>
#include <functional>

#include "capture/CaptureTypes.hpp"

namespace coomer {

struct CameraState {
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    int screenW = 0;
    int screenH = 0;
};

struct SpotlightState {
    bool enabled = false;
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    float radiusPx = 160.0f;
    float tintR = 0.0f;
    float tintG = 0.0f;
    float tintB = 0.0f;
    float tintA = 0.75f;
};

class RendererGL {
public:
    bool initGL(std::function<void*(const char*)> loaderProc);
    bool uploadScreenshotTexture(const ImageRGBA& image);
    void renderFrame(const CameraState& camera,
                     const SpotlightState& spotlight);

private:
    bool compileShaders();
    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int tex_ = 0;
    int imageW_ = 0;
    int imageH_ = 0;
};

}  // namespace coomer
