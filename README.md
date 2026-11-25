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

- **v3** (current, `main`): a Big-Picture-style shell with top tabs, a focused-art hero and
  tile rows, an app library, and settings (appearance, layout, display, audio, network,
  Bluetooth, launcher, system, power). It also has an on-screen keyboard and a quick panel
  drawn over running apps (gamescope external overlay).
- **Engine**: a retained node tree with flex layout in **dp units** (480p → 4K, any aspect),
  critically damped springs, spatial D-pad focus with row memory, smooth scrollers, 9-slice
  rounded/shadow rendering and an async image cache with an LRU budget.
- **Customization (TOML)** in `/etc/foyer/`:
  - `settings.toml`
  - `layout.toml` (tabs → rows → tiles)
  - `apps.d/*.toml` (per-app title, command, art)
  - `themes/<name>/theme.toml` (colors, fonts, type scale, metrics per breakpoint, motion,
    wallpaper). Drop-in user themes are supported.

  Everything is editable from the couch. A legacy `layout.xml` + `config.ini` is imported once.
- **v2.0.0** (tag): the last big-launcher-named release.

## Building

Dependencies: SDL3, SDL3_image, SDL3_ttf, libxml2, fmt and spdlog. libevdev is optional (global
Home/volume keys).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

CMake options: `FOYER_EVDEV` (on), `FOYER_CEC` (off), `FOYER_WERROR` (off),
`FOYER_BUNDLE_SDL` (off; static SDL3 family for portable glibc builds), `FOYER_TESTS` (on).

Run it in a window at any size, or render a screenshot offscreen:

```sh
FOYER_DATA_DIR=$PWD/data ./build/src/foyer -c /tmp/foyer-cfg --window --size 1280x720
FOYER_DATA_DIR=$PWD/data ./build/src/foyer -c /tmp/foyer-cfg --size 3840x2160 --screenshot 4k.png --open :display
```

Keys: arrows, Enter = A, Esc/Backspace = B, Tab/Menu = quick panel, Home, F12 = screenshot.

## Branches & releases

`main` is the only branch. Releases are git tags (`v2.0.0` is the last big-launcher-named build).
Packages (`.apk` and `.deb`) are attached to foyer-os releases.

## License

Upstream big-launcher was published **without a license**, so no license is granted here yet.
Relicensing needs complexlogic's permission (or a full rewrite of all remaining upstream code).
