#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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
                     const SpotlightState& spotlight,
                     float inspectorOpacity = 0.0f);

private:
    bool compileShaders();
    bool updateInspectorTexture(const CameraState& camera,
                                const SpotlightState& spotlight,
                                float opacity);
    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int tex_ = 0;
    unsigned int statusTex_ = 0;
    unsigned int inspectorTex_ = 0;
    int imageW_ = 0;
    int imageH_ = 0;
    int statusW_ = 0;
    int statusH_ = 0;
    int inspectorW_ = 0;
    int inspectorH_ = 0;
    bool uniformImage_ = false;
    std::vector<uint8_t> imagePixels_;
    std::string inspectorText_;
};

}  // namespace coomer
