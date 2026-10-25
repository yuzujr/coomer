#include <cassert>
#include <cmath>
#include "app/Overview.hpp"

using namespace coomer;

void settle(Overview& overview, CameraState& camera) {
    for (int i = 0; i < 300; ++i)
        overview.update(camera, 1920, 1080, 1920, 1080, 1.0f / 60);
}

int main() {
    CameraState camera;
    camera.zoom = 12;
    camera.panX = -300;
    camera.panY = -500;
    const CameraState original = camera;
    Overview overview;
    // Repeatedly interrupt both animations before either has finished.
    for (int i = 0; i < 40; ++i) {
        overview.begin(camera);
        overview.update(camera, 1920, 1080, 1920, 1080, 0.016f);
        overview.release();
        overview.update(camera, 1920, 1080, 1920, 1080, 0.016f);
    }
    settle(overview, camera);
    assert(!overview.active());
    assert(camera.zoom == original.zoom);
    assert(camera.panX == original.panX && camera.panY == original.panY);

    overview.begin(camera);
    settle(overview, camera);
    // Clicking outside the screenshot leaves overview open.
    assert(!overview.locate(camera, 0, 0, 1920, 1080, 1920, 1080));
    const float clickX = camera.panX + 720 * camera.zoom;
    const float clickY = camera.panY + 300 * camera.zoom;
    assert(overview.locate(camera, clickX, clickY, 1920, 1080, 1920, 1080));
    // The subsequent Space release must preserve the selected destination.
    overview.release();
    settle(overview, camera);
    assert(camera.zoom == original.zoom);
    assert(std::abs((960 - camera.panX) / camera.zoom - 720) < 0.001f);
    assert(std::abs((540 - camera.panY) / camera.zoom - 300) < 0.001f);

    // Selecting while the overview animation is still in flight uses the
    // screenshot position actually displayed under the pointer.
    overview.begin(camera);
    overview.update(camera, 1920, 1080, 1920, 1080, 0.016f);
    const float imageX = (960 - camera.panX) / camera.zoom;
    const float imageY = (540 - camera.panY) / camera.zoom;
    assert(overview.locate(camera, 960, 540, 1920, 1080, 1920, 1080));
    overview.begin(camera); // Interrupt return after selection.
    overview.release();
    settle(overview, camera);
    assert(camera.zoom == original.zoom);
    assert(std::abs((960 - camera.panX) / camera.zoom - imageX) < 0.001f);
    assert(std::abs((540 - camera.panY) / camera.zoom - imageY) < 0.001f);

    overview.begin(camera);
    overview.cancel();
    const CameraState cancelled = camera;
    settle(overview, camera);
    assert(!overview.active());
    assert(camera.zoom == cancelled.zoom && camera.panX == cancelled.panX);
}
