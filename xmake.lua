set_project("coomer")
set_version("0.1.0")

set_languages("c17", "cxx17")
add_rules("mode.debug", "mode.release")

option("x11")
    set_default(true)
    set_showmenu(true)
    set_description("Enable X11 backend and window")
option_end()

option("wayland")
    set_default(true)
    set_showmenu(true)
    set_description("Enable Wayland backend and window")
option_end()

option("portal")
    set_default(true)
    set_showmenu(true)
    set_description("Enable xdg-desktop-portal screenshot backend")
option_end()

option("nix")
    set_default(false)
    set_showmenu(true)
    set_description("Use nix:: packages instead of xmake package manager (for nix develop)")
option_end()

option("system")
    set_default(false)
    set_showmenu(true)
    set_description("Skip xmake package manager for stb (provided via CPATH in Nix derivation)")
option_end()

-- glad is vendored in generated/glad/ (generated once with glad2 --reproducible).
-- Only stb needs to come from outside; it's header-only with no .pc file.
if not has_config("system") then
    if has_config("nix") then
        add_requires("nix::stb", {alias = "stb"})
    else
        add_requires("stb")
    end
end

if has_config("x11") then
    add_requires("pkgconfig::x11", "pkgconfig::xrandr", "pkgconfig::gl")
end
if has_config("wayland") then
    add_requires("pkgconfig::wayland-client", "pkgconfig::wayland-egl", "pkgconfig::egl", "pkgconfig::gl", "pkgconfig::xkbcommon")
end
if has_config("portal") then
    add_requires("pkgconfig::dbus-1")
end

target("coomer")
    set_kind("binary")
    add_includedirs("src", "generated", "third_party")
    local embdir = path.join("$(projectdir)", "assets", "shaders")
    add_cxxflags("--embed-dir=" .. embdir)
    -- glad is vendored in third_party/glad/ (generated with glad2 --reproducible)
    add_files("third_party/glad/gl.c")
    -- stb is header-only; in derivation mode it's injected via CPATH
    if not has_config("system") then
        add_packages("stb")
    end
    add_syslinks("dl", "pthread", "m")

    add_files("src/app/main.cpp",
              "src/app/cli.cpp",
              "src/render/RendererGL.cpp",
              "src/capture/BackendAuto.cpp")

    if has_config("x11") then
        add_defines("COOMER_HAS_X11")
        add_files("src/capture/BackendX11.cpp",
                  "src/window/X11WindowGlx.cpp")
        add_packages("pkgconfig::x11", "pkgconfig::xrandr", "pkgconfig::gl")
    end

    if has_config("wayland") then
        add_defines("COOMER_HAS_WAYLAND")
        add_files("src/capture/BackendWlrScreencopy.cpp",
                  "src/window/WaylandWindowXdgEgl.cpp",
                  "src/window/WaylandWindowLayerShellEgl.cpp")
        add_packages("pkgconfig::wayland-client",
                     "pkgconfig::wayland-egl",
                     "pkgconfig::egl",
                     "pkgconfig::gl",
                     "pkgconfig::xkbcommon")

        on_load(function (target)
            local find_program = import("lib.detect.find_program")

            local wayland_scanner = find_program("wayland-scanner")
            if not wayland_scanner then
                raise("wayland-scanner not found. Install wayland-protocols or disable wayland via `xmake f --wayland=n`.")
            end

            os.mkdir("generated")

            for _, xml in ipairs(os.files("protocols/*.xml")) do
                local base   = path.basename(xml)
                local header = path.join("generated", base .. "-client-protocol.h")
                local code   = path.join("generated", base .. "-protocol.c")

                os.execv(wayland_scanner, {"client-header", xml, header})
                os.execv(wayland_scanner, {"private-code",  xml, code})
            end

            local files = os.files("generated/*.c")
            if #files > 0 then
                target:add("files", files)
            end
        end)
    end

    if has_config("portal") then
        add_defines("COOMER_HAS_PORTAL")
        add_files("src/capture/BackendPortalScreenshot.cpp")
        add_packages("pkgconfig::dbus-1")
    end
