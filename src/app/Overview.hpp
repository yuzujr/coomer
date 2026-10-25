#pragma once

#include <algorithm>
#include <cmath>

#include "render/RendererGL.hpp"

namespace coomer {

class Overview {
public:
    void begin(const CameraState& camera) {
        // A second press during the return animation keeps the original target.
        if (!returning_) {
            target_ = camera;
        }
        holding_ = true;
        returning_ = false;
    }

    void release() {
        if (holding_) {
            holding_ = false;
            returning_ = true;
        }
    }

    bool locate(const CameraState& camera, float x, float y,
                int imageW, int imageH, int screenW, int screenH) {
        if (!holding_) return false;
        const float imageX = (x - camera.panX) / camera.zoom;
        const float imageY = (y - camera.panY) / camera.zoom;
        if (imageX < 0 || imageY < 0 || imageX >= imageW || imageY >= imageH) {
            return false;
        }
        target_.panX = screenW * 0.5f - imageX * target_.zoom;
        target_.panY = screenH * 0.5f - imageY * target_.zoom;
        release();
        return true;
    }

    void cancel() { holding_ = returning_ = false; }
    bool active() const { return holding_ || returning_; }

    void update(CameraState& camera, int screenW, int screenH,
                int imageW, int imageH, float dt) {
        if (!active()) return;
        CameraState destination = target_;
        if (holding_) {
            const float fit = std::min(float(screenW) / imageW,
                                       float(screenH) / imageH);
            destination.zoom = std::clamp(fit * 0.78f, 0.05f, 1.0f);
            destination.panX = (screenW - imageW * destination.zoom) * 0.5f;
            destination.panY = (screenH - imageH * destination.zoom) * 0.5f;
        }
        const float blend = 1.0f - std::exp(-16.0f * dt);
        camera.zoom += (destination.zoom - camera.zoom) * blend;
        camera.panX += (destination.panX - camera.panX) * blend;
        camera.panY += (destination.panY - camera.panY) * blend;

        if (returning_ && std::abs(camera.zoom - target_.zoom) < 0.001f &&
            std::abs(camera.panX - target_.panX) < 0.5f &&
            std::abs(camera.panY - target_.panY) < 0.5f) {
            camera.zoom = target_.zoom;
            camera.panX = target_.panX;
            camera.panY = target_.panY;
            returning_ = false;
        }
    }

private:
    bool holding_ = false;
    bool returning_ = false;
    CameraState target_;
};

}  // namespace coomer
