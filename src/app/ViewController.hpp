#pragma once

#include "app/Overview.hpp"
#include "window/IWindow.hpp"

namespace coomer {

// Navigation state is independent of the window backend and renderer. Camera
// coordinates use buffer pixels with the origin at the bottom left.
class ViewController {
public:
    explicit ViewController(const CameraState& home) : home_(home) {}

    void update(CameraState& camera, const InputState& input, int imageW,
                int imageH, float dt);
    float spotlightRadiusMultiplier() const {
        return spotlightRadiusMultiplier_;
    }

private:
    void stopMomentum();

    CameraState home_;
    Overview overview_;
    float panVelX_ = 0.0f;
    float panVelY_ = 0.0f;
    float zoomVel_ = 0.0f;
    float spotlightRadiusMultiplier_ = 1.0f;
    bool prevLeft_ = false;
    bool prevSpace_ = false;
    bool prevOne_ = false;
};

}  // namespace coomer
