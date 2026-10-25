# coomer

Zoomer application for everyone on Linux.

![](./demo.gif)

## Features

- **Free navigation**: Smooth pan and zoom from 0.05× to 32×, with a one-key reset
- **Overview navigation**: Hold Space to see the whole capture; click a location to return there at the previous zoom
- **Pixel inspection**: Toggle nearest sampling, a pixel grid, screenshot coordinates and HEX colors with P; copy a color with C
- **Spotlight**: Hold Ctrl to highlight the pointer area
- **Multi-backend support**: Automatic selection between wlr-screencopy, xdg-desktop-portal, and X11
- **Multi-monitor support**: Capture specific monitor or all monitors (X11/wlr)

## Installation

### AppImage
Download the x86_64 or aarch64 AppImage from [releases](https://github.com/yuzujr/coomer/releases), make it executable, and run it. If FUSE is unavailable, use `APPIMAGE_EXTRACT_AND_RUN=1 ./coomer-v<version>-<arch>.AppImage`. On NixOS, use `appimage-run`.

See [CHANGELOG.md](CHANGELOG.md) for release notes.

### Arch Linux (AUR)
* `coomer` — build from source
* `coomer-bin` — AppImage

Install one of them:

use `paru`:
```bash
paru -S coomer
# or
paru -S coomer-bin
```

or use `yay`:
```bash
yay -S coomer
# or
yay -S coomer-bin
```

### NixOS

Add to your `flake.nix`:

```nix
inputs.coomer.url = "github:yuzujr/coomer";
```

Then in your NixOS configuration:

```nix
environment.systemPackages = [
  inputs.coomer.packages.${pkgs.system}.default
];
```

Or try it without installing:

```bash
nix run github:yuzujr/coomer
```

### Build from Source

Requirements:

- Linux
- `clang` or `g++` with C17/C++17 support
- `pkg-config`
- CMake 3.18 or newer
- Ninja (or a Makefile generator)
- `libGL` and `libEGL`
- `wayland-scanner` when Wayland support is enabled
- `libX11` and `libXrandr` when X11 support is enabled
- `wayland`, `wayland-egl`, and `libxkbcommon` when Wayland support is enabled
- `dbus` when portal support is enabled

Configure and build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/coomer
# Optional system installation:
sudo cmake --install build --prefix /usr
```

All backends are enabled by default. Disable optional backends at configuration
time; each build directory keeps its own options:

```bash
# X11 only
cmake -S . -B build/x11 -G Ninja -DCOOMER_WAYLAND=OFF -DCOOMER_PORTAL=OFF
cmake --build build/x11 --parallel

# Wayland only, no portal
cmake -S . -B build/wayland -G Ninja -DCOOMER_X11=OFF -DCOOMER_PORTAL=OFF
cmake --build build/wayland --parallel
```

For Nix development, the project shell includes the build dependencies, CMake,
Ninja and clangd:

```bash
nix develop
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
nvim .
```

CMake generates `build/compile_commands.json` during configuration by default.
Clangd discovers the database in `build/`; no Bear or recording build is needed.
CMake updates the database when configuration changes and tracks header and
protocol dependencies for incremental builds. Generated protocol sources and
headers stay inside the build directory. Build artifacts are excluded from Git.

Tests build by default. Run them with:

```bash
ctest --test-dir build --output-on-failure
```

The rendering test requires surfaceless EGL/OpenGL support. For Mesa software
rendering, use `LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build --output-on-failure`.
Clipboard tests use fake helpers and leave the desktop clipboard untouched.
Use `-DBUILD_TESTING=OFF` when configuring a package without test executables.

## Usage

```
Usage: coomer [options]

Options:
  --backend <mode>       Capture backend: auto|x11|wlr|portal (default: auto)
  --monitor <name>       Select monitor/output by name (x11/wlr only, use 'all' to capture all monitors)
  --list-monitors        List monitors/outputs visible to the backend (x11/wlr only)
  --overlay              Wayland layer-shell overlay (wlr/portal only)
  --portal-interactive   Enable interactive mode for portal (show selection dialog)
  --no-spotlight         Disable spotlight mode
  --version              Show version
  --debug                Enable debug logging
  --help, -h             Show this help message

Hotkeys:
  Q or Escape: quit
  Hold Left click: pan
  Scroll wheel: zoom (0.05x to 32x)
  Hold Space: overview; click to center the original zoom on a location
  P: toggle pixel inspection (nearest sampling, grid, coordinates and HEX color)
  C: copy HEX color while inspecting
  1: reset view
  Hold Ctrl: spotlight (Ctrl + wheel to resize)
```

Pixel inspection is off by default and controlled only by P, at any zoom. It
stays active during overview and its animations; press P again for smooth
sampling. C copies the screenshot pixel's original RGB value, without the grid
or spotlight effects. Copying requires `wl-copy` (the `wl-clipboard` package) on
Wayland or `xclip` on X11, including when running the AppImage.

In overview, clicking inside the screenshot starts returning toward that
location at the previous zoom, even if Space remains held. Clicking the margin
does nothing. Releasing Space without selecting restores the previous view.
The 1 key resets to the initial view. Q and Escape exit; A and right-click do not.

Pixel coordinates refer to the captured image. With `--monitor all`, the wlr
backend composites outputs at their logical sizes, which can resample captures
on scaled monitors; inspection then reports the composite image's pixels.

**Note**: On Wayland with multiple monitors, the portal backend only supports fullscreen capture, which may include unwanted areas. Use `--portal-interactive` to show a selection dialog on each launch, allowing you to choose the capture mode.

On the wlr backend, the default capture is the first enumerated output. To
capture the focused niri output at launch:

```bash
coomer --overlay --monitor "$(niri msg -j focused-output | jq -r '.name')"
```

This requires `jq`. The monitor option selects the capture; niri chooses the
overlay output independently. Keep the intended display focused when launching.

## Backend Selection

### Auto Mode (Default)

- In an X11-only session, use X11.
- In a Wayland session, try wlr-screencopy, then the Screenshot portal.
- If those are unavailable and `$DISPLAY` is set, fall back to X11.

### Manual Selection

```bash
# wlr-screencopy:
coomer --backend wlr

# Portal: may require authorization
coomer --backend portal

# X11:
coomer --backend x11
```

## Architecture

```
┌─────────────┐
│   coomer    │
├─────────────┤
│ Capture     │──┬─→ wlr-screencopy (Wayland)
│ Backend     │  ├─→ xdg-desktop-portal (Wayland/X11)
│             │  └─→ XGetImage (X11)
├─────────────┤
│ Window      │──┬─→ layer-shell EGL (Wayland overlay)
│ Backend     │  ├─→ xdg-shell EGL (Wayland fullscreen)
│             │  └─→ GLX (X11 fullscreen)
├─────────────┤
│ Renderer    │──→ OpenGL 3.3 Core (embedded shaders)
└─────────────┘
```

## Acknowledgements

This project was inspired by:

- **[boomer](https://github.com/tsoding/boomer)** by [@tsoding](https://github.com/tsoding) — Original zoomer application.
- **[woomer](https://github.com/coffeeispower/woomer)** by [@coffeeispower](https://github.com/coffeeispower) — A Wayland (wlroots) port of boomer.

## License

MIT
