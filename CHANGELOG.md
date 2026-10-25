# Changelog

## 1.3.0

### Added

- Click a location while holding Space in overview to center it at the previous
  zoom. Release Space without selecting to return to the previous view.
- Press P to toggle pixel inspection at any zoom. Inspection uses nearest pixel
  sampling, a pixel grid, screenshot coordinates and HEX color values. It stays
  enabled during overview and its animations until switched off manually.
- Press C during pixel inspection to copy the selected pixel's `#RRGGBB` value.
  A brief message confirms success or reports failure. Install `wl-clipboard`
  on Wayland or `xclip` on X11 to enable clipboard access.
- CMake generates `compile_commands.json` by default for clangd. Development,
  Nix packages and AppImage builds use the same CMake configuration; the Nix
  development environment includes CMake, Ninja and clangd.

### Changed

- Pixel inspection is off by default and no longer activates automatically at
  high magnification. Its color display stays visible while inspection is on.
- Use Q or Escape to quit. A and right-click no longer exit the application.
- Overview transitions keep exponential easing with a slightly faster response.

### Fixed

- Rapid Space presses during a return animation preserve the original zoom and
  destination instead of progressively moving toward the overview size.
- Track header and generated Wayland protocol dependencies through CMake so
  edits rebuild affected sources automatically.
