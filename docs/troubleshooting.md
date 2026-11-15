# Troubleshooting

## Capture fails or the wrong backend is selected

Start from a terminal in the desktop session and enable diagnostics:

```bash
coomer --debug
```

To save a log, choose a writable file:

```bash
COOMER_LOG_FILE="$HOME/coomer.log" coomer --debug
```

Logs append to that file. Include the command you ran, coomer version
(`coomer --version`), desktop/compositor, selected backend and relevant log
messages when reporting an issue. Logs may include output names and paths;
review them before sharing.

On Wayland, try the system screenshot dialog explicitly:

```bash
coomer --backend portal --portal-interactive
```

This needs a running `xdg-desktop-portal` service with a screenshot provider
for your desktop. The dialog and available capture choices belong to that
provider. Cancelling the dialog ends the capture. A portal request currently
times out after about 30 seconds while waiting for the dialog response.

For wlr-screencopy, the compositor must expose that capture protocol. For X11,
`DISPLAY` must refer to an accessible X server. Launch from the desktop session
as your regular user so the relevant display and session-bus environment is
available.

## Select the correct monitor

For X11 or wlr-screencopy, list the exact output names:

```bash
coomer --list-monitors
coomer --monitor HDMI-A-1
coomer --monitor all
```

If necessary, use the same explicit `--backend` for both listing and capture.
Unknown output names are errors. Portal does not enumerate or accept monitor
names; use its interactive dialog instead.

`--monitor` controls which image is captured. It does not pin the Wayland
window to that output. Window placement belongs to the compositor. An overlay
also requires layer-shell; otherwise coomer opens an xdg-shell fullscreen
window. Use app ID `coomer` when writing rules for that fullscreen window.

## Pixel colors or positions look different

Press **P** to enable inspection. Coordinates and HEX values refer to pixels
in the captured image. With `--monitor all`, outputs can be resampled to fit
their logical layout, particularly when they have different scaling factors.
Capture one output to inspect its original captured pixels.

A single-color capture is displayed as received, with a status message. It
does not necessarily mean capture failed. If the image is unexpected, compare
an explicit wlr/X11 capture with the portal dialog.

If a capture exceeds the GPU texture-size limit, select one monitor or use a
smaller region in the portal dialog, when that provider offers region capture.

## Copying a color fails

Press **P**, point at the captured image and press **C**. The pointer must be
inside the image. Color copying uses an external helper in `PATH`:

- Wayland: `wl-copy`, provided by `wl-clipboard`.
- X11: `xclip`.

These helpers are separate dependencies even when using an AppImage. Install
the helper for your session and launch coomer where it is available. On
Wayland, `WAYLAND_DISPLAY` selects the Wayland helper. The clipboard helper
keeps ownership of the copied text after coomer exits.

## AppImage does not start

Make the downloaded file executable and check its version. Replace the file
name below with your download:

```bash
chmod +x ./coomer-vVERSION-ARCH.AppImage
./coomer-vVERSION-ARCH.AppImage --version
```

Choose the asset matching your CPU architecture: `x86_64` or `aarch64`. If
FUSE is unavailable, extract and run without mounting:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./coomer-vVERSION-ARCH.AppImage --version
```

On NixOS, run the AppImage through `appimage-run`, or use the repository's Nix
package. If `--version` works but a window cannot open, run with `--debug` in
the desktop session and check the capture and OpenGL/EGL messages.

## Shell completion is unavailable

System installation includes Bash, fish and Zsh completion files. Completion
is registered for the command `coomer`; running a versioned AppImage filename
or `nix run` does not register it in the host shell.

For a temporary setup from a checkout, put a `coomer` command in `PATH`, then
load the script for your shell:

```bash
# Bash
source completions/bash/coomer

# fish
source completions/fish/coomer.fish
```

For Zsh, add the checkout's `completions/zsh` directory to `fpath` before
initializing completion with `compinit` in your shell configuration. Monitor
completion queries `coomer --list-monitors`; it needs access to the desktop
session and an X11 or wlr backend.
