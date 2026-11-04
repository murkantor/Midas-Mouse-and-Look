# CLAUDE.md: adding Midas Mouse and Look to a game

You are probably reading this because someone asked you to add this library to
*their* game (a PC decomp, recomp or port). This file is your playbook. Follow it
in order. The library is small. The work is finding the right places in the
host game and keeping the rules in `docs/FEEL.md`.

If you are instead working on this library itself, skip to "Working on this
repository" at the end.

## Ground rules

- **Read `docs/FEEL.md` before writing any code.** The feel comes from where
  values enter the game. The most important rule: the mouse is added to the
  camera **angle** after the game's own camera update, and is never fed in as a
  stick value or turn rate.
- **Don't change the library's math to fit the host.** Adapt at the call site:
  units, signs, angle format. If the library really seems wrong for the host,
  say so to the user instead of editing it.
- **Don't guess the host's architecture.** Find the real code (step 2) and cite
  `file:line` when you explain your plan.
- **Ask the user** about game-design choices: which control styles or modes get
  mouse look, the default sensitivity, whether to add a settings UI. Make the
  plumbing decisions yourself.
- Keep the host's existing input behaviour for anything you weren't asked to
  change. Players of that port already rely on it.

## Step 1: vendor the library

Pick whichever matches how the host already keeps third-party code:

```bash
git submodule add https://github.com/murkantor/Midas-Mouse-and-Look third_party/midas-mouse-and-look
```

or a `git subtree add`, or copy `include/`, `src/` and `LICENSE` into the
host's third-party folder. Then wire up the build:

- **CMake:** `add_subdirectory(third_party/midas-mouse-and-look)` and
  `target_link_libraries(<game target> PRIVATE midas_feel_core)`.
- **Make, Meson or another build system:** compile `src/mf_stick.c`,
  `src/mf_look.c` and `src/mf_mouse.c`, add `include/` to the include path, and
  link `m` on Unix.

The code is C99 and also compiles cleanly as part of a C++ project (the headers
have `extern "C"`).

## Step 2: find the four hook points

Search the host and write these down, with `file:line`, before changing
anything:

| # | Find | Search hints |
|---|---|---|
| A | where the port fills the console's **pad struct** from the PC controller | `OSContPad`, `osContGetReadData`, `PADStatus`, `PADRead`, `padman`, `XINPUT_GAMEPAD`, `SDL_GameControllerGetAxis`, `SDL_GetGamepadAxis` |
| B | the **input pump**, where relative mouse motion is read | `SDL_MOUSEMOTION`, `SDL_EVENT_MOUSE_MOTION`, `SDL_GetRelativeMouseState`, `xrel` |
| C | the **top of the simulation tick**, before game logic reads input. This is not per rendered frame | the main loop, `game_tick`, fixed-step accumulator, the call that reads pads |
| D | where the game **updates camera or player yaw and pitch** after its own integration | `yaw`, `pitch`, `theta`, `angle`, `camera`, `rot`, and the code that consumes the right stick or C buttons |

Also find:

- the **"would the stick move the player right now" condition**: menus, pause,
  cutscenes, death, demo or attract playback. There's often a "controls locked"
  flag. This becomes the single mouse permission predicate.
- the game's **current FOV and unzoomed FOV**, if it zooms.
- the **angle units** (degrees, radians, or N64-style 0..65535 binary angles)
  and which way positive pitch points.
- whether the game has its own **turn acceleration or camera smoothing**. The
  mouse goes in after it.

Hook D is usually in game code (a decomp source file), not only the port layer.
That's expected. Guard host-side additions the way the host already does
(`#ifndef TARGET_N64`, `#ifdef PC_PORT`, or similar), so console builds keep
matching.

If any hook point is ambiguous, show the user what you found and ask before
editing.

## Step 3: wire up the mouse

Follow `docs/INTEGRATION.md` section 2. In short:

1. At init (SDL2), set `SDL_HINT_MOUSE_RELATIVE_MODE_WARP` to `"0"` and
   `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE` to `"0"`. Never add acceleration.
2. When capturing the pointer, enter relative mode, call
   `SDL_GetRelativeMouseState(NULL, NULL)` once, then `mf_mouse_discard`.
   Release capture and discard when the window loses focus.
3. At hook B, call `mf_mouse_add` for every motion. Use **one** source: events
   or polled state, never both.
4. At hook C, call `mf_mouse_tick`, then `mf_mouse_discard` if the permission
   predicate says no.
5. At hook D, call `mf_mouse_deg`, multiply by `mf_fov_ratio(current, unzoomed)`,
   convert to the host's angle units and signs, add it to the angles **after**
   the game's own update, then let the game's own pitch clamp run.
6. In split-screen, only player 1 gets the mouse. Zero it on other players'
   ticks.
7. Aim mode (if the game has a crosshair that moves inside the screen): use
   `mf_aim_mouse_step`, `mf_aim_edge_push` and `mf_lowpass_steady_state`, while
   the mouse is driving only.

Expose `sens_x`, `sens_y`, `sens_aim` and `invert_y` (`mf_mouse_cfg`) through
whatever config system the host already has. Don't invent a new config file.

## Step 4: wire up the controller

- **Look stick** (`docs/INTEGRATION.md` section 3): `mf_look_stick_to_n64`, then
  `mf_look_step` with the real frame time, added at hook D like the mouse. If
  the game already has an analog stick-look path, prefer putting the right stick
  into *that* path in the game's own units, and say so to the user.
- **Move stick, N64 games only:** `mf_stick_scaled_n64` at hook A, for modern
  pads only.
- **GameCube, PS2, Xbox:** follow the per-platform notes in
  `docs/INTEGRATION.md` section 4. They say which functions to skip, such as the
  octagon on round-gate consoles, and how to avoid stacking deadzones with the
  game's own.
- **raphnet-class adapters** (VID `0x289B`) read through the joystick API: use
  `mf_stick_raw_units` and skip every deadzone and curve. For the full raphnet
  RAW path, point the user at
  <https://github.com/murkantor/Midas-Raphnet-Raw-Driver>.

## Step 5: verify

1. Build the host. Fix any warnings you introduced.
2. Build and run this library's test:
   `cmake -S third_party/midas-mouse-and-look -B build-mml && cmake --build build-mml && ctest --test-dir build-mml`.
3. Go through the checklist at the end of `docs/INTEGRATION.md` and report each
   item to the user as **verified** (and how), **needs a person**, or **not
   applicable**. Feel needs a human with a mouse and a pad. Say so plainly, and
   don't claim it feels right.
4. If the host has headless or replay tests, run them. Mouse input must not
   change demo or replay playback.

## Step 6: hand back

Tell the user, briefly:

- the hook points you used (`file:line`)
- the settings you exposed and their defaults
- anything you left alone and why
- what they need to test by hand: draw a circle with the mouse in aim mode;
  check yaw against pitch at two FOV settings; check inverted and upright
  pitch; check nothing moves in menus or cutscenes

## Working on this repository

- C99, no dependencies, no globals, no allocation. Keep it that way.
- `tests/mf_test.c` holds `REF_*` copies of the Project Midas originals. A
  change to the feel must change the function and its `REF_*` together, and
  `docs/PROVENANCE.md` must say why.
- Build and test with
  `cmake -S . -B build && cmake --build build && ctest --test-dir build`. It
  must stay warning-free with `-Wall -Wextra`.
