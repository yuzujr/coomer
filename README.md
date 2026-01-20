# coomer

Zoomer application for everyone on Linux.

## Features

- **Multi-backend support**: Automatic selection between wlr-screencopy, xdg-desktop-portal, and X11

## Installation

### Releases
Install from [releases](https://github.com/yuzujr/coomer/releases).

### Build from Source

**Requirements**: Linux, xmake, C++17 compiler

```bash
git clone https://github.com/yuzujr/coomer
cd coomer
xmake -y
```

## Usage

```
Usage: coomer [options]

Options:
  --backend <mode>       Capture backend: auto|x11|wlr|portal (default: auto)
  --monitor <name>       Select monitor/output by name (x11/wlr only)
  --list-monitors        List monitors/outputs visible to the backend (x11/wlr only)
  --overlay              Wayland layer-shell overlay (wlr/portal only)
  --no-spotlight         Disable spotlight mode
  --debug                Enable debug logging
  --help, -h             Show this help message

Hotkeys:
  Q or A or Right click: quit
  Hold Ctrl: spotlight (Ctrl + wheel to resize)
```

## Backend Selection

### Auto Mode (Default)

Priority order:
1. **X11** — if `$DISPLAY` is set
2. **wlr-screencopy** — if compositor supports wlr protocols (Hyprland, Sway, niri)
3. **portal** — fallback (all modern Wayland compositors)

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
