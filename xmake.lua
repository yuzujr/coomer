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

-- Path to a pre-generated glad include directory.
-- When set, skips xmake's glad download (used by the Nix derivation).
option("glad_includedir")
    set_default("")
    set_showmenu(true)
    set_description("Path to pre-generated glad include dir (e.g. from python3Packages.glad2)")
option_end()

local _glad_dir = get_config("glad_includedir")
if _glad_dir and _glad_dir ~= "" then
    -- Derivation mode: glad pre-generated, stb provided via CPATH.
    -- No add_requires needed — xmake must not try to download anything.
elseif has_config("nix") then
    -- nix develop: stb comes from Nix, glad still downloaded by xmake.
    add_requires("nix::stb", {alias = "stb"})
    add_requires("glad", {configs = {api = "gl=3.3", profile = "core"}})
else
    -- Normal: xmake downloads everything.
    add_requires("glad", {configs = {api = "gl=3.3", profile = "core"}})
    add_requires("stb")
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
    add_includedirs("src", "generated")
    local embdir = path.join("$(projectdir)", "assets", "shaders")
    add_cxxflags("--embed-dir=" .. embdir)
    -- glad + stb: pre-generated (derivation) or xmake-managed (devShell/normal)
    local _glad_inc = get_config("glad_includedir")
    if _glad_inc and _glad_inc ~= "" then
        -- Derivation mode: add the include dir, compile glad's generated .c
        -- glad v1 layout: <glad_includedir>/glad/glad.h, <glad_includedir>/../src/glad.c
        add_includedirs(_glad_inc)
        add_files(path.join(_glad_inc, "..", "src", "*.c"))
        -- stb is provided via CPATH in the Nix derivation; no add_packages needed.
    else
        add_packages("glad", "stb")
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
