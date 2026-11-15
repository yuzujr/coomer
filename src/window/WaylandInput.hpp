#pragma once

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <unordered_map>

#include "window/IWindow.hpp"

namespace coomer {

// Shared seat handling for fullscreen and overlay windows. Pointer coordinates
// are converted from surface coordinates to the pixels used by the renderer.
class WaylandInput {
public:
    WaylandInput() = default;
    ~WaylandInput();
    WaylandInput(const WaylandInput&) = delete;
    WaylandInput& operator=(const WaylandInput&) = delete;

    void bind(wl_registry* registry, uint32_t name, uint32_t version);
    void removeSeat(uint32_t name);
    void reset();
    void beginFrame();
    void setGeometry(int surfaceWidth, int surfaceHeight, int width,
                     int height);
    const InputState& state() const {
        return input_;
    }

private:
    void clearKeyboard();
    void clearPointer();
    void updateKey(uint32_t key, bool pressed);
    double surfaceToBufferX(double x) const;
    double surfaceToBufferY(double y) const;
    static void handleSeatCapabilities(void* data, wl_seat* seat,
                                       uint32_t caps);
    static void handleSeatName(void*, wl_seat*, const char*);
    static void handlePointerEnter(void* data, wl_pointer*, uint32_t,
                                   wl_surface*, wl_fixed_t sx, wl_fixed_t sy);
    static void handlePointerLeave(void* data, wl_pointer*, uint32_t,
                                   wl_surface*);
    static void handlePointerMotion(void* data, wl_pointer*, uint32_t,
                                    wl_fixed_t sx, wl_fixed_t sy);
    static void handlePointerButton(void* data, wl_pointer*, uint32_t, uint32_t,
                                    uint32_t button, uint32_t state);
    static void handlePointerAxis(void* data, wl_pointer*, uint32_t,
                                  uint32_t axis, wl_fixed_t value);
    static void handlePointerFrame(void*, wl_pointer*);
    static void handlePointerAxisSource(void*, wl_pointer*, uint32_t);
    static void handlePointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t);
    static void handlePointerAxisDiscrete(void*, wl_pointer*, uint32_t,
                                          int32_t);
    static void handleKeyboardKeymap(void* data, wl_keyboard*, uint32_t format,
                                     int fd, uint32_t size);
    static void handleKeyboardEnter(void*, wl_keyboard*, uint32_t, wl_surface*,
                                    wl_array*);
    static void handleKeyboardLeave(void*, wl_keyboard*, uint32_t, wl_surface*);
    static void handleKeyboardKey(void* data, wl_keyboard*, uint32_t, uint32_t,
                                  uint32_t key, uint32_t state);
    static void handleKeyboardModifiers(void* data, wl_keyboard*, uint32_t,
                                        uint32_t depressed, uint32_t latched,
                                        uint32_t locked, uint32_t group);
    static void handleKeyboardRepeatInfo(void*, wl_keyboard*, int32_t, int32_t);

    static const wl_seat_listener seatListener_;
    static const wl_pointer_listener pointerListener_;
    static const wl_keyboard_listener keyboardListener_;

    wl_seat* seat_ = nullptr;
    wl_pointer* pointer_ = nullptr;
    wl_keyboard* keyboard_ = nullptr;
    uint32_t seatName_ = 0;
    xkb_context* xkbContext_ = nullptr;
    xkb_keymap* xkbKeymap_ = nullptr;
    xkb_state* xkbState_ = nullptr;
    std::unordered_map<uint32_t, bool InputState::*> pressedKeys_;
    InputState input_{};
    int surfaceWidth_ = 1;
    int surfaceHeight_ = 1;
    int width_ = 1;
    int height_ = 1;
    bool hasLastMouse_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

}  // namespace coomer
