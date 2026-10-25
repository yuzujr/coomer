#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cassert>
#include <glad/gl.h>
#include "render/RendererGL.hpp"

int main() {
    auto platformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    assert(platformDisplay);
    EGLDisplay display = platformDisplay(EGL_PLATFORM_SURFACELESS_MESA, nullptr, nullptr);
    assert(eglInitialize(display, nullptr, nullptr));
    assert(eglBindAPI(EGL_OPENGL_API));
    EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint count;
    assert(eglChooseConfig(display, attributes, &config, 1, &count) && count);
    EGLint size[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, size);
    EGLint version[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
    assert(eglMakeCurrent(display, surface, surface, context));
    coomer::RendererGL renderer;
    assert(renderer.initGL([](const char* name) {
        return reinterpret_cast<void*>(eglGetProcAddress(name));
    }));
    coomer::ImageRGBA image{2, 2, {255,0,0,255, 0,255,0,255,
                                  0,0,255,255, 255,255,255,255}};
    assert(renderer.uploadScreenshotTexture(image));
    coomer::CameraState camera;
    camera.screenW = camera.screenH = 32;
    camera.zoom = 2;
    assert(renderer.colorAt(camera, 0.5f, 0.5f) == "#0000FF");
    assert(renderer.colorAt(camera, 0.5f, 3.5f) == "#FF0000");
    assert(renderer.colorAt(camera, -1, 0).empty());
    assert(renderer.colorAt(camera, 4, 0).empty());
    unsigned char smooth[4], pixels[4];
    coomer::SpotlightState spotlight;
    renderer.renderFrame(camera, spotlight, 0, false);
    glReadPixels(2, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, smooth);
    renderer.renderFrame(camera, spotlight, 0, true);
    glReadPixels(2, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    assert(smooth[0] < 230 && pixels[0] == 255 && pixels[1] == 255);
    // High magnification still uses smooth filtering unless explicitly enabled.
    camera.zoom = 16;
    renderer.renderFrame(camera, spotlight, 0, false);
    glReadPixels(20, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, smooth);
    renderer.renderFrame(camera, spotlight, 0, true);
    glReadPixels(20, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    assert(smooth[0] < 230 && pixels[0] == 255 && pixels[1] == 255);
    assert(glGetError() == GL_NO_ERROR);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
}
