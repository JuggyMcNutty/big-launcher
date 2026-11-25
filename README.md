# Foyer

**Foyer** is a couch-first, big-picture launcher for TVs: a fullscreen, gamepad/remote-friendly front
door for your living-room PC. It is written in C++23 on SDL3 and runs under
[gamescope](https://github.com/ValveSoftware/gamescope).

It is the launcher half of **[foyer-os](https://github.com/JuggyMcNutty/foyer-os)**, a reproducible
Alpine (musl) / Debian (glibc) appliance image built from per-device profiles.

> Foyer began as a fork of [complexlogic/big-launcher](https://github.com/complexlogic/big-launcher)
> (dormant since 2025-08) and has since been rewritten. Thanks to complexlogic for the original
> project. See [FORK.md](FORK.md) for the history.

## Status

- **v2.x** (current): a couch frontend with a sidebar and card grid, settings (audio, display,
  network, Bluetooth, system, power, launcher), on-screen keyboard, status bar, in-UI layout editor,
  and a quick-menu overlay drawn over running apps (gamescope external overlay).
- **v3** (in progress on `main`): a Big-Picture-style UI engine. It uses resolution-independent dp
  layout from 480p to 4K, a spring-animated focus engine, TOML themes/layouts/apps, and drop-in
  user themes.

## Building

Dependencies: SDL3, SDL3_image, SDL3_ttf, libxml2, fmt and spdlog. libevdev is optional (global
Home/volume keys).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

CMake options: `FOYER_EVDEV` (on), `FOYER_CEC` (off), `FOYER_WERROR` (off).

## Branches & releases

`main` is the only branch. Releases are git tags (`v2.0.0` is the last big-launcher-named build).
Packages (`.apk` and `.deb`) are attached to foyer-os releases.

## License

Upstream big-launcher was published **without a license**, so no license is granted here yet.
Relicensing needs complexlogic's permission (or a full rewrite of all remaining upstream code).
