#include "window/WaylandInput.hpp"

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#include "platform/Log.hpp"

#if __has_include(<linux/input-event-codes.h>)
#include <linux/input-event-codes.h>
#else
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#endif

namespace coomer {

WaylandInput::~WaylandInput() {
    reset();
}

void WaylandInput::bind(wl_registry* registry, uint32_t name,
                        uint32_t version) {
    if (seat_) return;
    xkbContext_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkbContext_)
        LOG_ERROR("failed to initialize Wayland keyboard context");
    seatName_ = name;
    seat_ = static_cast<wl_seat*>(wl_registry_bind(
        registry, name, &wl_seat_interface, std::min(version, 5u)));
    wl_seat_add_listener(seat_, &seatListener_, this);
}

void WaylandInput::removeSeat(uint32_t name) {
    if (seat_ && seatName_ == name) reset();
}

void WaylandInput::clearKeyboard() {
    input_.keyCtrl = input_.keyShift = input_.keyQ = input_.keyEscape = false;
    input_.keySpace = input_.keyOne = input_.keyP = input_.keyC = false;
    pressedKeys_.clear();
}

void WaylandInput::clearPointer() {
    input_.mouseLeft = input_.mouseRight = false;
    input_.deltaX = input_.deltaY = input_.wheelDelta = 0.0;
    hasLastMouse_ = false;
}

void WaylandInput::reset() {
    clearKeyboard();
    clearPointer();
    if (keyboard_) wl_keyboard_destroy(keyboard_);
    if (pointer_) wl_pointer_destroy(pointer_);
    if (seat_) wl_seat_destroy(seat_);
    if (xkbState_) xkb_state_unref(xkbState_);
    if (xkbKeymap_) xkb_keymap_unref(xkbKeymap_);
    if (xkbContext_) xkb_context_unref(xkbContext_);
    keyboard_ = nullptr;
    pointer_ = nullptr;
    seat_ = nullptr;
    xkbState_ = nullptr;
    xkbKeymap_ = nullptr;
    xkbContext_ = nullptr;
    seatName_ = 0;
}

void WaylandInput::beginFrame() {
    input_.deltaX = input_.deltaY = input_.wheelDelta = 0.0;
}

double WaylandInput::surfaceToBufferX(double x) const {
    return x * width_ / surfaceWidth_;
}

double WaylandInput::surfaceToBufferY(double y) const {
    return y * height_ / surfaceHeight_;
}

void WaylandInput::setGeometry(int surfaceWidth, int surfaceHeight, int width,
                               int height) {
    const double surfaceX = input_.mouseX * surfaceWidth_ / width_;
    const double surfaceY = input_.mouseY * surfaceHeight_ / height_;
    surfaceWidth_ = std::max(1, surfaceWidth);
    surfaceHeight_ = std::max(1, surfaceHeight);
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    input_.mouseX = surfaceToBufferX(surfaceX);
    input_.mouseY = surfaceToBufferY(surfaceY);
    // Geometry changes must not become a synthetic drag on the next motion.
    hasLastMouse_ = false;
    input_.deltaX = input_.deltaY = 0.0;
}

void WaylandInput::updateKey(uint32_t key, bool pressed) {
    if (!pressed) {
        const auto found = pressedKeys_.find(key);
        if (found == pressedKeys_.end()) return;
        const auto field = found->second;
        pressedKeys_.erase(found);
        input_.*field = std::any_of(pressedKeys_.begin(), pressedKeys_.end(),
                                    [field](const auto& entry) {
                                        return entry.second == field;
                                    });
        return;
    }
    if (!xkbState_) return;
    bool InputState::* field = nullptr;
    switch (xkb_state_key_get_one_sym(xkbState_, key + 8)) {
        case XKB_KEY_q:
        case XKB_KEY_Q:
            field = &InputState::keyQ;
            break;
        case XKB_KEY_Escape:
            field = &InputState::keyEscape;
            break;
        case XKB_KEY_space:
            field = &InputState::keySpace;
            break;
        case XKB_KEY_1:
            field = &InputState::keyOne;
            break;
        case XKB_KEY_p:
        case XKB_KEY_P:
            field = &InputState::keyP;
            break;
        case XKB_KEY_c:
        case XKB_KEY_C:
            field = &InputState::keyC;
            break;
        default:
            return;
    }
    pressedKeys_[key] = field;
    input_.*field = true;
}

void WaylandInput::handleSeatCapabilities(void* data, wl_seat* seat,
                                          uint32_t caps) {
    auto* self = static_cast<WaylandInput*>(data);
    if (caps & WL_SEAT_CAPABILITY_POINTER) {
        if (!self->pointer_) {
            self->pointer_ = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(self->pointer_, &pointerListener_, self);
        }
    } else if (self->pointer_) {
        wl_pointer_destroy(self->pointer_);
        self->pointer_ = nullptr;
        self->clearPointer();
    }

    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        if (!self->keyboard_) {
            self->keyboard_ = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(self->keyboard_, &keyboardListener_, self);
        }
    } else if (self->keyboard_) {
        wl_keyboard_destroy(self->keyboard_);
        self->keyboard_ = nullptr;
        self->clearKeyboard();
    }
}

void WaylandInput::handleSeatName(void*, wl_seat*, const char*) {}

void WaylandInput::handlePointerEnter(void* data, wl_pointer*, uint32_t,
                                      wl_surface*, wl_fixed_t sx,
                                      wl_fixed_t sy) {
    auto* self = static_cast<WaylandInput*>(data);
    double x = self->surfaceToBufferX(wl_fixed_to_double(sx));
    double y = self->surfaceToBufferY(wl_fixed_to_double(sy));
    self->input_.mouseX = x;
    self->input_.mouseY = y;
    self->lastMouseX_ = x;
    self->lastMouseY_ = y;
    self->hasLastMouse_ = true;
}

void WaylandInput::handlePointerLeave(void* data, wl_pointer*, uint32_t,
                                      wl_surface*) {
    auto* self = static_cast<WaylandInput*>(data);
    self->clearPointer();
}

void WaylandInput::handlePointerMotion(void* data, wl_pointer*, uint32_t,
                                       wl_fixed_t sx, wl_fixed_t sy) {
    auto* self = static_cast<WaylandInput*>(data);
    double x = self->surfaceToBufferX(wl_fixed_to_double(sx));
    double y = self->surfaceToBufferY(wl_fixed_to_double(sy));
    if (!self->hasLastMouse_) {
        self->lastMouseX_ = x;
        self->lastMouseY_ = y;
        self->hasLastMouse_ = true;
    }
    self->input_.deltaX += x - self->lastMouseX_;
    self->input_.deltaY += y - self->lastMouseY_;
    self->lastMouseX_ = x;
    self->lastMouseY_ = y;
    self->input_.mouseX = x;
    self->input_.mouseY = y;
}

void WaylandInput::handlePointerButton(void* data, wl_pointer*, uint32_t,
                                       uint32_t, uint32_t button,
                                       uint32_t state) {
    auto* self = static_cast<WaylandInput*>(data);
    bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    if (button == BTN_LEFT) {
        self->input_.mouseLeft = pressed;
    } else if (button == BTN_RIGHT) {
        self->input_.mouseRight = pressed;
    }
}

void WaylandInput::handlePointerAxis(void* data, wl_pointer*, uint32_t,
                                     uint32_t axis, wl_fixed_t value) {
    auto* self = static_cast<WaylandInput*>(data);
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        double v = wl_fixed_to_double(value);
        self->input_.wheelDelta += v / 120.0;
    }
}

void WaylandInput::handlePointerFrame(void*, wl_pointer*) {}
void WaylandInput::handlePointerAxisSource(void*, wl_pointer*, uint32_t) {}
void WaylandInput::handlePointerAxisStop(void*, wl_pointer*, uint32_t,
                                         uint32_t) {}
void WaylandInput::handlePointerAxisDiscrete(void*, wl_pointer*, uint32_t,
                                             int32_t) {}

void WaylandInput::handleKeyboardKeymap(void* data, wl_keyboard*,
                                        uint32_t format, int fd,
                                        uint32_t size) {
    auto* self = static_cast<WaylandInput*>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size == 0 ||
        !self->xkbContext_) {
        close(fd);
        return;
    }
    const char* map = static_cast<const char*>(
        mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (map == MAP_FAILED) {
        LOG_WARN("failed to map Wayland keyboard layout");
        return;
    }
    xkb_keymap* keymap = nullptr;
    if (std::memchr(map, '\0', size)) {
        keymap = xkb_keymap_new_from_string(self->xkbContext_, map,
                                            XKB_KEYMAP_FORMAT_TEXT_V1,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
    }
    munmap(const_cast<char*>(map), size);
    xkb_state* state = keymap ? xkb_state_new(keymap) : nullptr;
    if (!state) {
        if (keymap) xkb_keymap_unref(keymap);
        LOG_WARN("failed to load Wayland keyboard layout");
        return;
    }
    if (self->xkbState_) xkb_state_unref(self->xkbState_);
    if (self->xkbKeymap_) xkb_keymap_unref(self->xkbKeymap_);
    self->xkbKeymap_ = keymap;
    self->xkbState_ = state;
    self->clearKeyboard();
}

void WaylandInput::handleKeyboardEnter(void* data, wl_keyboard*, uint32_t,
                                       wl_surface*, wl_array* keys) {
    auto* self = static_cast<WaylandInput*>(data);
    self->clearKeyboard();
    const auto* held = static_cast<const uint32_t*>(keys->data);
    for (size_t i = 0; i < keys->size / sizeof(uint32_t); ++i) {
        self->updateKey(held[i], true);
    }
}

void WaylandInput::handleKeyboardLeave(void* data, wl_keyboard*, uint32_t,
                                       wl_surface*) {
    static_cast<WaylandInput*>(data)->clearKeyboard();
}

void WaylandInput::handleKeyboardKey(void* data, wl_keyboard*, uint32_t,
                                     uint32_t, uint32_t key, uint32_t state) {
    auto* self = static_cast<WaylandInput*>(data);
    self->updateKey(key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

void WaylandInput::handleKeyboardModifiers(void* data, wl_keyboard*, uint32_t,
                                           uint32_t depressed, uint32_t latched,
                                           uint32_t locked, uint32_t group) {
    auto* self = static_cast<WaylandInput*>(data);
    if (!self->xkbState_) {
        return;
    }
    xkb_state_update_mask(self->xkbState_, depressed, latched, locked, 0, 0,
                          group);
    self->input_.keyCtrl =
        xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_CTRL,
                                     XKB_STATE_MODS_EFFECTIVE) > 0;
    self->input_.keyShift =
        xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_SHIFT,
                                     XKB_STATE_MODS_EFFECTIVE) > 0;
}

void WaylandInput::handleKeyboardRepeatInfo(void*, wl_keyboard*, int32_t,
                                            int32_t) {}

const wl_seat_listener WaylandInput::seatListener_ = {handleSeatCapabilities,
                                                      handleSeatName};
const wl_pointer_listener WaylandInput::pointerListener_ = {
    handlePointerEnter,      handlePointerLeave,    handlePointerMotion,
    handlePointerButton,     handlePointerAxis,     handlePointerFrame,
    handlePointerAxisSource, handlePointerAxisStop, handlePointerAxisDiscrete};
const wl_keyboard_listener WaylandInput::keyboardListener_ = {
    handleKeyboardKeymap, handleKeyboardEnter,     handleKeyboardLeave,
    handleKeyboardKey,    handleKeyboardModifiers, handleKeyboardRepeatInfo};

}  // namespace coomer
