> **Disclaimer:** this library was extracted from Project Midas with AI assistance ([Claude Code](https://claude.com/claude-code)).

# Midas Mouse and Look

The mouse and controller feel from **Project Midas** (the GoldenEye 007 PC
port), as a small C library for other PC decomps and recomps: N64, GameCube,
PS2, Xbox or anything else.

| Part | What it gives you |
|---|---|
| **Mouse** (`mf_mouse`) | Raw, unaccelerated mouse look applied as an **angle**, not a rate. Per-tick accumulation so no counts are lost or doubled, FOV scaling, and aim-mode crosshair helpers (the same pixels per degree on both axes, edge turn, no glide). |
| **Look curve** (`mf_look`) | The right-stick look players praise: dead band, squared response, FOV-scaled, frame-rate independent. |
| **Stick shaping** (`mf_stick`) | Deadzone with rescale, round stick stretched onto the N64 octagon, octagon gate, raw 1:1 mode for raphnet-class adapters, 8-point calibration for worn sticks, trigger levels. |

Plain C99, no dependencies, no allocation and no globals.

For real N64 controllers on a raphnet adapter, see the companion
[Midas-Raphnet-Raw-Driver](https://github.com/murkantor/Midas-Raphnet-Raw-Driver).

## Add it to your game with one prompt

In Claude Code (or another AI coding agent), from your game's repository:

```
Add https://github.com/murkantor/Midas-Mouse-and-Look to this project. Follow its CLAUDE.md.
```

[CLAUDE.md](CLAUDE.md) tells the agent how to vendor the library, find the
right hook points in your port, wire up mouse and look, and test the result.
You stay in charge of the choices only you can make, such as which control
styles get mouse look.

## Why it feels right

Players praise how smooth and responsive the mouse is in Project Midas, and how
good the look is on a controller. Most of that comes from where input enters the
game, not from the arithmetic:

- **The mouse moves the camera angle directly**, added after the game's own
  camera update. It is never fed in as a stick or turn rate, so the game's turn
  smoothing can't make it floaty.
- **Raw counts, no acceleration**, and every count used exactly once per game
  tick, at any frame rate.
- **One "would the stick move the player now?" check** zeroes the mouse in
  menus, pause, cutscenes and demos, so motion never banks up and snaps the
  view.
- **Zoom slows look down, but a wider FOV setting doesn't speed it up**, for the
  mouse and the stick alike.
- **The right stick uses the N64 game's own look curve**: a small dead band, a
  squared response (a small lean aims, a full push spins), and no acceleration.
- **Aim mode**: the crosshair moves the same pixels per degree on both axes and
  lands where the mouse puts it, with no glide.

## Docs

- **[docs/FEEL.md](docs/FEEL.md)** explains in full why it feels the way it does.
  Skip this and copy only the code, and you won't get the feel.
- **[docs/INTEGRATION.md](docs/INTEGRATION.md)** is step-by-step merging into
  other ports, with the pitfalls for N64, GameCube, PS2 and Xbox.
- **[docs/PROVENANCE.md](docs/PROVENANCE.md)** maps every function back to the
  Project Midas code it came from.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

As a subproject:

```cmake
add_subdirectory(third_party/midas-mouse-and-look)
target_link_libraries(mygame PRIVATE midas_feel_core)
```

Or add `src/*.c` to your build and `include/` to your include path (link `m` on
Unix).

## Thirty-second tour

```c
#include "midas_feel/mf_stick.h"
#include "midas_feel/mf_look.h"
#include "midas_feel/mf_mouse.h"

/* modern pad left stick -> N64 stick */
mf_stick_scaled_n64(lx, -ly_sdl, MF_DEADZONE_DEFAULT, 1.0f, &pad.stick_x, &pad.stick_y);

/* modern pad right stick -> camera, once per frame */
int32_t lkx, lky; float dyaw, dpitch;
mf_look_stick_to_n64(rx, -ry_sdl, MF_DEADZONE_DEFAULT, &lkx, &lky);
mf_look_step(&look_cfg, lkx, lky, mf_fov_ratio(cur_fov, base_fov), dt, &dyaw, &dpitch);

/* mouse: add every event, consume once per sim tick, apply as an angle */
mf_mouse_add(&mouse, ev.motion.xrel, ev.motion.yrel);    /* input pump */
mf_mouse_tick(&mouse);                                    /* top of sim tick */
if (!look_permitted) mf_mouse_discard(&mouse);
mf_mouse_deg(&mouse, &mouse_cfg, &dyaw, &dpitch);
camera.yaw   += dyaw   * mf_fov_ratio(cur_fov, base_fov); /* AFTER the game's own camera update */
camera.pitch += dpitch * mf_fov_ratio(cur_fov, base_fov);
```

## Status

All of it is checked against verbatim copies of the Project Midas originals
over full input sweeps (`tests/mf_test.c`, about 170k checks). It builds with
GCC and MinGW with no warnings.

## Licence and credits

MIT (see LICENSE), same as Project Midas.

- The mouse angle model follows the 1964 GoldenEye / Perfect Dark mouse
  injector. It was reimplemented, not copied, and the injector's acceleration
  curve was deliberately left out.
- The deadzone rescale follows the Perfect Dark PC port's `inputAxisScale`.
