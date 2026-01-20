#include "window/X11WindowGlx.hpp"

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>

#include <cstring>
#include <memory>

#include "platform/Log.hpp"

namespace coomer {

class X11WindowGlx final : public IWindow {
public:
    explicit X11WindowGlx(const WindowConfig& config) {
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            LOG_ERROR("failed to open X11 display");
            return;
        }
        int screen = DefaultScreen(display_);

        static int fbAttribs[] = {GLX_X_RENDERABLE,
                                  True,
                                  GLX_DRAWABLE_TYPE,
                                  GLX_WINDOW_BIT,
                                  GLX_RENDER_TYPE,
                                  GLX_RGBA_BIT,
                                  GLX_X_VISUAL_TYPE,
                                  GLX_TRUE_COLOR,
                                  GLX_RED_SIZE,
                                  8,
                                  GLX_GREEN_SIZE,
                                  8,
                                  GLX_BLUE_SIZE,
                                  8,
                                  GLX_ALPHA_SIZE,
                                  8,
                                  GLX_DEPTH_SIZE,
                                  24,
                                  GLX_STENCIL_SIZE,
                                  8,
                                  GLX_DOUBLEBUFFER,
                                  True,
                                  None};

        int fbCount = 0;
        GLXFBConfig* fbc =
            glXChooseFBConfig(display_, screen, fbAttribs, &fbCount);
        if (!fbc || fbCount == 0) {
            LOG_ERROR("failed to choose GLX framebuffer config");
            return;
        }

        GLXFBConfig fbConfig = fbc[0];
        XFree(fbc);

        XVisualInfo* vi = glXGetVisualFromFBConfig(display_, fbConfig);
        if (!vi) {
            LOG_ERROR("failed to get X visual");
            return;
        }

        Colormap cmap = XCreateColormap(display_, RootWindow(display_, screen),
                                        vi->visual, AllocNone);
        XSetWindowAttributes swa{};
        swa.colormap = cmap;
        swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                         ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask | StructureNotifyMask;

        width_ =
            config.width > 0 ? config.width : DisplayWidth(display_, screen);
        height_ =
            config.height > 0 ? config.height : DisplayHeight(display_, screen);

        window_ = XCreateWindow(
            display_, RootWindow(display_, screen), config.x, config.y,
            static_cast<unsigned int>(width_),
            static_cast<unsigned int>(height_), 0, vi->depth, InputOutput,
            vi->visual, CWColormap | CWEventMask, &swa);
        XFree(vi);

        if (!window_) {
            LOG_ERROR("failed to create X11 window");
            return;
        }

        XStoreName(display_, window_, config.title.c_str());

        wmDelete_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display_, window_, &wmDelete_, 1);

        Atom wmState = XInternAtom(display_, "_NET_WM_STATE", False);
        Atom wmFullscreen =
            XInternAtom(display_, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(display_, window_, wmState, XA_ATOM, 32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char*>(&wmFullscreen), 1);

        XMapRaised(display_, window_);
        XFlush(display_);

        // Wait for window to be mapped before setting focus to avoid BadMatch
        XSync(display_, False);

        // SetInputFocus may fail if window is not yet viewable, ignore errors
        XSetErrorHandler([](Display*, XErrorEvent*) {
            return 0;
        });
        XSetInputFocus(display_, window_, RevertToParent, CurrentTime);
        XSync(display_, False);
        XSetErrorHandler(nullptr);

        auto glXCreateContextAttribsARB = reinterpret_cast<GLXContext (*)(
            Display*, GLXFBConfig, GLXContext, Bool, const int*)>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(
                "glXCreateContextAttribsARB")));

        if (glXCreateContextAttribsARB) {
            int contextAttribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                    3,
                                    GLX_CONTEXT_MINOR_VERSION_ARB,
                                    3,
                                    GLX_CONTEXT_PROFILE_MASK_ARB,
                                    GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                                    None};
            context_ = glXCreateContextAttribsARB(display_, fbConfig, 0, True,
                                                  contextAttribs);
        }

        if (!context_) {
            context_ =
                glXCreateNewContext(display_, fbConfig, GLX_RGBA_TYPE, 0, True);
        }

        if (!context_) {
            LOG_ERROR("failed to create GLX context");
            return;
        }

        glXMakeCurrent(display_, window_, context_);
        valid_ = true;
    }

    ~X11WindowGlx() override {
        if (display_) {
            if (context_) {
                glXMakeCurrent(display_, None, nullptr);
                glXDestroyContext(display_, context_);
            }
            if (window_) {
                XDestroyWindow(display_, window_);
            }
            XCloseDisplay(display_);
        }
    }

    bool isValid() const {
        return valid_;
    }

    void pollEvents() override {
        input_.deltaX = 0.0;
        input_.deltaY = 0.0;
        input_.wheelDelta = 0.0;

        while (display_ && XPending(display_)) {
            XEvent ev{};
            XNextEvent(display_, &ev);
            switch (ev.type) {
                case MotionNotify: {
                    double x = ev.xmotion.x;
                    double y = ev.xmotion.y;
                    if (!hasLastMouse_) {
                        lastMouseX_ = x;
                        lastMouseY_ = y;
                        hasLastMouse_ = true;
                    }
                    input_.deltaX += x - lastMouseX_;
                    input_.deltaY += y - lastMouseY_;
                    lastMouseX_ = x;
                    lastMouseY_ = y;
                    input_.mouseX = x;
                    input_.mouseY = y;
                    break;
                }
                case ButtonPress: {
                    if (ev.xbutton.button == Button1) {
                        input_.mouseLeft = true;
                    } else if (ev.xbutton.button == Button3) {
                        input_.mouseRight = true;
                    } else if (ev.xbutton.button == Button4) {
                        input_.wheelDelta -= 1.0;
                    } else if (ev.xbutton.button == Button5) {
                        input_.wheelDelta += 1.0;
                    }
                    break;
                }
                case ButtonRelease: {
                    if (ev.xbutton.button == Button1) {
                        input_.mouseLeft = false;
                    } else if (ev.xbutton.button == Button3) {
                        input_.mouseRight = false;
                    }
                    break;
                }
                case KeyPress:
                case KeyRelease: {
                    bool pressed = (ev.type == KeyPress);
                    KeySym sym = XLookupKeysym(&ev.xkey, 0);
                    if (sym == XK_q || sym == XK_Q) {
                        input_.keyQ = pressed;
                    } else if (sym == XK_a || sym == XK_A) {
                        input_.keyA = pressed;
                    } else if (sym == XK_Control_L || sym == XK_Control_R) {
                        input_.keyCtrl = pressed;
                    } else if (sym == XK_Shift_L || sym == XK_Shift_R) {
                        input_.keyShift = pressed;
                    }
                    break;
                }
                case ConfigureNotify: {
                    width_ = ev.xconfigure.width;
                    height_ = ev.xconfigure.height;
                    break;
                }
                case ClientMessage: {
                    if (static_cast<Atom>(ev.xclient.data.l[0]) == wmDelete_) {
                        shouldClose_ = true;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    bool shouldClose() const override {
        return shouldClose_;
    }
    InputState input() const override {
        return input_;
    }
    int width() const override {
        return width_;
    }
    int height() const override {
        return height_;
    }

    void swap() override {
        if (display_ && window_) {
            glXSwapBuffers(display_, window_);
        }
    }

    void* glGetProcAddress(const char* name) override {
        return reinterpret_cast<void*>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
    }

private:
    Display* display_ = nullptr;
    Window window_ = 0;
    GLXContext context_ = nullptr;
    Atom wmDelete_ = 0;
    bool shouldClose_ = false;
    bool valid_ = false;

    InputState input_{};
    int width_ = 0;
    int height_ = 0;
    bool hasLastMouse_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

std::unique_ptr<IWindow> CreateX11WindowGlx(const WindowConfig& config) {
    auto window = std::make_unique<X11WindowGlx>(config);
    if (!window->isValid()) {
        return nullptr;
    }
    return window;
}

}  // namespace coomer
