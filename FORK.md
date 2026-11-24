# HTPC fork

This is a fork of [complexlogic/big-launcher](https://github.com/complexlogic/big-launcher)
(upstream dormant since 2025-08) maintained for a living-room HTPC build
(Alpine/musl, gamescope, 1080p60).

Branch layout: **`master`** tracks upstream untouched (for future merges);
**`htpc`** carries our changes and is what actually ships to the box.

## Changes on `htpc`

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