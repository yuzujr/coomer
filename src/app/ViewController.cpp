#include "app/ViewController.hpp"

#include <algorithm>
#include <cmath>

namespace coomer {

void ViewController::stopMomentum() {
    panVelX_ = panVelY_ = zoomVel_ = 0.0f;
}

void ViewController::update(CameraState& camera, const InputState& input,
                            int imageW, int imageH, float dt) {
    const float cursorX = static_cast<float>(input.mouseX);
    const float cursorY = static_cast<float>(camera.screenH - input.mouseY);
    const float deltaX = static_cast<float>(input.deltaX);
    const float deltaY = static_cast<float>(-input.deltaY);
    const bool spacePressed = input.keySpace && !prevSpace_;
    const bool spaceReleased = !input.keySpace && prevSpace_;
    const bool resetPressed = input.keyOne && !prevOne_;
    const bool leftPressed = input.mouseLeft && !prevLeft_;
    prevSpace_ = input.keySpace;
    prevOne_ = input.keyOne;

    if (spacePressed) {
        overview_.begin(camera);
        stopMomentum();
    } else if (spaceReleased) {
        overview_.release();
    }

    if (resetPressed) {
        camera.zoom = home_.zoom;
        camera.panX = home_.panX;
        camera.panY = home_.panY;
        stopMomentum();
        overview_.cancel();
    }

    if (leftPressed) {
        overview_.locate(camera, cursorX, cursorY, imageW, imageH,
                         camera.screenW, camera.screenH);
    }
    const bool cameraTransition = overview_.active();

    if (!cameraTransition && input.mouseLeft) {
        camera.panX += deltaX;
        camera.panY += deltaY;
        if (dt > 0.0f) {
            panVelX_ = deltaX / dt;
            panVelY_ = deltaY / dt;
        }
    } else if (!cameraTransition && !prevLeft_) {
        camera.panX += panVelX_ * dt;
        camera.panY += panVelY_ * dt;
        float decay = std::exp(-6.0f * dt);
        panVelX_ *= decay;
        panVelY_ *= decay;
        // Stop momentum when velocity becomes imperceptible to prevent
        // subpixel jitter
        const float minVel = 1.0f;  // px/s
        if (std::abs(panVelX_) < minVel && std::abs(panVelY_) < minVel) {
            panVelX_ = 0.0f;
            panVelY_ = 0.0f;
        }
    }
    prevLeft_ = input.mouseLeft;

    if (!cameraTransition && input.wheelDelta != 0.0) {
        float wheel = static_cast<float>(-input.wheelDelta);
        if (input.keyCtrl) {
            spotlightRadiusMultiplier_ += wheel * 0.35f;
            spotlightRadiusMultiplier_ =
                std::clamp(spotlightRadiusMultiplier_, 0.3f, 10.0f);
        } else {
            zoomVel_ += wheel * 2.0f;
        }
    }

    if (!cameraTransition && std::abs(zoomVel_) > 0.0001f) {
        float oldZoom = camera.zoom;
        float factor = std::exp(zoomVel_ * dt);
        camera.zoom = std::clamp(camera.zoom * factor, 0.05f, 32.0f);
        if (camera.zoom != oldZoom) {
            float ratio = camera.zoom / oldZoom;
            camera.panX = cursorX - (cursorX - camera.panX) * ratio;
            camera.panY = cursorY - (cursorY - camera.panY) * ratio;
        } else {
            zoomVel_ = 0.0f;
        }
        zoomVel_ *= std::exp(-6.0f * dt);
        // Stop zoom momentum when velocity becomes imperceptible
        if (std::abs(zoomVel_) < 0.01f) {
            zoomVel_ = 0.0f;
        }
    }

    overview_.update(camera, camera.screenW, camera.screenH, imageW, imageH,
                     dt);
}

}  // namespace coomer
