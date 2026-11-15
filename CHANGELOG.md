# Changelog

## Unreleased

### Changed

- Share Wayland input and EGL management between fullscreen and overlay windows.
- Keep camera navigation and interaction state in a separate view controller.
- Transfer screenshot pixels into the renderer without retaining an extra CPU copy.
- Print help to standard output, accept `--option=value`, and explain invalid option combinations.
- Improve output-name completion and support backend selection with `=`.
- Document backend selection, installation and troubleshooting.

### Fixed

- Clear held input on Wayland focus loss and input-device removal.
- Match key releases even when modifiers or keyboard layout change.
- Accept keyboard maps whose advertised size omits the trailing terminator.
- Report unknown output names instead of capturing a different monitor.
- Set the Wayland app ID for desktop integration.
- Retain the portal request path after releasing its D-Bus reply.
- Report invalid image buffers, GPU texture limits and screenshot upload errors.

## 1.3.0

### Added

- Free zoom from 0.05× to 32×, Space overview and 1 to reset the view.
- Click in Space overview to recenter at the previous zoom.
- P toggles pixel inspection; C copies the selected HEX color.
- CMake builds with automatic clangd compilation database generation.

### Changed

- Pixel inspection is manual and off by default.
- Quit with Q or Escape; A and right-click no longer exit.
- Slightly faster exponential overview transitions.
- Development, Nix and AppImage builds share one CMake configuration.

### Fixed

- Rapid Space presses no longer overwrite the original return view.
- Header and Wayland protocol edits rebuild affected sources automatically.
- Preserve single-color captures and show a status message instead of a placeholder.

## 1.2.4

- Show a visual placeholder for single-color captures.
- Check rendered AppImage output in release tests.

## 1.2.3

- Set the X11 window class to `coomer` for window identification.
- Add an AppImage window smoke test.

## 1.2.2

- Build AppImages on Ubuntu 22.04 for broader Linux compatibility.
- Verify the packaged version during release builds.

## 1.2.1

- Add aarch64 AppImage releases.

## 1.2.0

- Switch release downloads to AppImage.
- Add desktop launcher, icon and AppStream metadata.
- Unify builds on Makefile and remove xmake.

## 1.1.4

- Add shell completion for `--version`.

## 1.1.3

- Fix Wayland rendering and pointer coordinates with fractional scaling.
- Preserve capture resolution with `--monitor all` on a single output.
- Add Nix packaging, a development shell and completion installation.
- Add `--version`.

## 1.1.2

- Add `--monitor all` to capture multiple outputs.

## 1.1.1

- Add Bash, Zsh and Fish completions to release archives.

## 1.1.0

- Add `--portal-interactive` for portal capture selection.
- Fix intermittent X11 startup errors and reversed scroll direction.

## 1.0.0

- Initial release with pan, zoom and spotlight.
- X11, wlr-screencopy and Screenshot portal capture backends.
- Monitor selection and Wayland overlay mode.
