# HTPC fork

This is a fork of [complexlogic/big-launcher](https://github.com/complexlogic/big-launcher)
(upstream dormant since 2025-08) maintained for a living-room HTPC build
(Alpine/musl, gamescope, 1080p60).

Branch layout:
- **`master`** tracks upstream untouched (for future merges)
- **`htpc`** — the v1 launcher (incremental fixes on upstream). Kept as a working fallback.
- **`v2`** — the full couch-frontend rewrite (current shipping branch on the box). See below.

## Changes on `v2` (full rewrite)

A ground-up rewrite into a proper couch frontend. New `src/` layout
(`core/`, `platform/`, `input/`, `ui/`, `screens/`, `jobs/`, `render/`),
RAII throughout, action-based input, dirty-flag rendering, and no blocking
the UI thread. Replaces the v1 god-class files.

- **Core**: `App` (main loop, dirty rendering), `Config` (INI read **and**
  write — atomic + `.bak`, preserves comments/order), `LayoutModel`
  (libxml2 read+write).
- **Platform**: `Process` (fork+setsid+pidfd reap, kill-by-group; app state =
  PID alive), `power` (OpenRC), `sysctl` (builds `htpc-ctl` commands).
- **Input**: `Action` enum + `ActionMapper` (keyboard/gamepad),
  `evdev_global` (`BL_EVDEV`), `cec` stub (`BL_CEC`).
- **UI**: `Screen`/`ScreenStack`, `ListScreen`, `Anim` (tweens), `TextSprite`,
  `shapes`, `TextInput` (OSK), `StatusBar`.
- **Screens**: `home` (sidebar+grid), `quick_menu` (overlay), settings hub +
  audio/display/network/bluetooth/system/power/launcher pages, and the
  in-UI **layout editor** (menu → entries → actions).
- **Jobs**: `JobQueue` (worker thread + SDL user-event delivery; streaming).
- **Quick-menu overlay**: `big-launcher --overlay` runs a minimal app that
  gamescope composites on top of the running app via
  `GAMESCOPE_EXTERNAL_OVERLAY`.
- **System backend**: `htpc-ctl` (POSIX-sh; audio/display/wifi/bt/net/sys/power)
  supersedes the old `htpc-net`.
- **Tests**: doctest (config round-trip, layout round-trip, action mapping,
  process reaping).

CMake options: `BL_CEC` (off), `BL_EVDEV` (on), `BL_WERROR` (off).

## Changes on `htpc` (v1)

- **WiFi / Bluetooth settings UI**: new full-screen `BL::SettingsScreen`
  (WiFi + Bluetooth pages). WiFi lists scanned SSIDs (wpa_cli, strongest
  first) with live status; join = typed password via the physical keyboard
  (SDL text input). Bluetooth lists known devices, scans, and
  pairs/trusts/connects (bluetoothctl). Driven by the `htpc-net` shell
  helper (see the HTPC project repo); degrades gracefully with no radio.
  Special commands `:wifi` / `:bt` open the pages.
- **Clock**: upstream shipped the `[Clock]` config section but no
  implementation. Implemented here — strftime-formatted clock
  (default `%H:%M`) rendered at the bottom of the sidebar column,
  re-rendering its texture only when the displayed string changes.
  Config: `[Clock] Enabled=true` / `Format=%H:%M`.
- **Fixes for fmt >= 10**: explicit `<fmt/format.h>` includes where
  `fmt::format` is used (`<fmt/core.h>` no longer exports it).
- `BL::Config::add_string` implemented (was declared, never defined).
- Typos in log strings (`Sucessfully`, `occured`).

## Building (Linux)

Same deps as upstream: SDL3, SDL3_image, SDL3_ttf, libxml2, fmt, spdlog.

```bash
mkdir build && cd build
cmake ..
make
```

Assets are not in the repo; download
[assets.zip](https://github.com/complexlogic/big-launcher/files/10326572/assets.zip)
and extract into the launch directory (see upstream README).