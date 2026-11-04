# Merging Midas Mouse and Look into another port

This covers PC decomps and recomps of N64, GameCube, PS2 and Xbox games. Every
port's input layer is different, so this tells you **what to find** in your port
and **what to put there**, rather than claiming to know any one project's API.
For real N64 controllers on a raphnet adapter, see the separate
[Raphnet RAW driver](https://github.com/murkantor/Midas-Raphnet-Raw-Driver).
Read [FEEL.md](FEEL.md) first: the rules there matter more than the calls here.

## 0. Add the library

```cmake
add_subdirectory(third_party/midas-mouse-and-look)          # or a git submodule / subtree
target_link_libraries(game PRIVATE midas_feel_core)
```

Without CMake, compile `src/*.c` and add `include/` to the include path (and
link `m` on Unix). There are no other dependencies.

## 1. The four hook points every port has

Find these four places in your port before writing any code.

| # | Find… | Put there |
|---|---|---|
| A | where the port **fills the console's pad struct** from SDL (or XInput, or whatever it uses) | stick shaping |
| B | the **input pump**: the SDL event loop, or wherever relative mouse motion is read | `mf_mouse_add` |
| C | the **top of the simulation tick**, before the game reads input. Not per rendered frame | `mf_mouse_tick` + permission check |
| D | where the game **updates the camera yaw and pitch**, after its own integration | add mouse and look-stick degrees |

Hook D usually requires touching game code (a decomp source file), not only the
port layer. This is unavoidable: an angle has to go onto the angle (rule 2 of the
Mouse section in FEEL.md).

## 2. Mouse (every platform)

```c
static mf_mouse s_mouse;            /* one per local mouse player (player 1) */
static mf_mouse_cfg s_mcfg;         /* mf_mouse_cfg_default(&s_mcfg) at init */

/* init, SDL2: raw, unaccelerated relative motion */
SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");

/* capture (click in the window / gameplay starts) */
SDL_SetRelativeMouseMode(SDL_TRUE);
SDL_GetRelativeMouseState(NULL, NULL);   /* drop the pre-lock jump */
mf_mouse_discard(&s_mouse);

/* hook B: every SDL_MOUSEMOTION while captured */
mf_mouse_add(&s_mouse, ev.motion.xrel, ev.motion.yrel);
/* (or once per poll: SDL_GetRelativeMouseState(&dx, &dy); mf_mouse_add(&s_mouse, dx, dy);
 *  use one or the other, never both) */

/* focus lost: release and discard */
SDL_SetRelativeMouseMode(SDL_FALSE);
mf_mouse_discard(&s_mouse);

/* hook C: top of each sim tick */
mf_mouse_tick(&s_mouse);
if (!game_would_obey_stick_now())          /* ONE predicate: menus, pause, cutscene, death, demo */
    mf_mouse_discard(&s_mouse);

/* hook D: in the camera update, after the game's own yaw/pitch integration */
float dyaw, dpitch;
mf_mouse_deg(&s_mouse, &s_mcfg, &dyaw, &dpitch);
const float fr = mf_fov_ratio(current_fov, unzoomed_fov);
cam_yaw   += dyaw   * fr * YAW_SIGN;       /* match the engine's conventions */
cam_pitch += dpitch * fr * PITCH_SIGN;     /* then clamp with the engine's own limits */
```

- Convert to the engine's angle units if it doesn't use degrees: N64 games
  often use 0..65535 binary angles (`deg * 65536 / 360`), and some use radians.
- Find out whether the engine counts pitch up or down and set `PITCH_SIGN` once.
  Test both the upright and inverted options.
- If the game has a **turn acceleration or camera smoothing** step, the mouse
  must go in after it, not into it.
- **Fixed-tick games with an interpolated renderer** (a 30 Hz sim with
  uncapped render, common in recomps): the rules above give you mouse
  resolution at the sim rate. For extra smoothness you can also apply the
  pending `acc_dx/acc_dy` to the render camera each frame, but then the sim
  must use the same total, or aim and view will disagree. Midas runs a
  variable-rate sim, so it doesn't need this.
- **Aim mode**: if the game has a crosshair that moves inside the screen, use
  `mf_aim_mouse_step` for the crosshair position, `mf_aim_edge_push` to turn the
  view at the edge, and `mf_lowpass_steady_state` where the game glides the
  crosshair. Apply all three only while the mouse is driving, so stick aim stays
  vanilla.

## 3. Look stick (modern pad, right stick)

```c
static mf_look_cfg s_look;          /* mf_look_cfg_n64(&s_look) at init */

int32_t lx, ly;
float dyaw, dpitch;
mf_look_stick_to_n64(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX),
                     -SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY),   /* SDL Y is down */
                     MF_DEADZONE_DEFAULT, &lx, &ly);
mf_look_step(&s_look, lx, ly, mf_fov_ratio(current_fov, unzoomed_fov), dt_seconds, &dyaw, &dpitch);
/* hook D, like the mouse. Positive pitch = look up; apply invert after this. */
```

- **If the game already has an analog stick-look path** (as GoldenEye does for
  its 1.2 / 1.4 / 2.x control styles), the most faithful option is what Midas
  does: put the right stick into *that* path as N64 units, apply the octagon gate
  and the dead band, and let the game's own curve run. `mf_look_rate` is a
  reimplementation of GoldenEye's version of that path, for games that don't
  have one.
- For a third-person camera (Mario, Zelda, Banjo style), feed `dyaw` into the
  port's free-camera or C-stick camera rather than the player's facing.
- `yaw_dps` / `pitch_dps` of 210 is GoldenEye's own rate. Other games turn at
  different speeds, so keep the curve and tune the rates.

## 4. Per platform

### N64 decomps and recomps

This is the closest fit, because everything is already in N64 units.

- **Hook A** is wherever the port produces `OSContPad` (often an
  `osContGetReadData` replacement, or the controller layer of a shared port
  framework). For modern pads, use
  `mf_stick_scaled_n64` for the stick, then map buttons as the port already does.
- **Don't double-shape.** Many N64 games apply their own dead band and curve to
  `stick_x/y`. Leave those alone: they're what makes the game feel like itself.
  `mf_stick_scaled_n64` only converts a modern pad *into* an N64 stick. It
  doesn't replace the game's own processing.
- **FPS games** (GoldenEye-likes, Turok, Doom 64, Quake 64 and similar): use
  hooks C and D for mouse and look, as above.
- **Recomps.** The recompiled game code reads controller state through the
  runtime's input layer, so hook A is in the runtime, not the game. Hook D
  (camera angle) usually means a patch to the game's camera function. Check how
  your recomp framework lets you patch functions before editing generated code.

### GameCube decomps

- The game reads `PADStatus` (Dolphin SDK): `s8 stickX, stickY, substickX,
  substickY`, `u8 triggerLeft/Right`, and a `u16 button` word. The GC gate is an
  octagon too, but **not** the N64's. Use your game's own numbers:
  `mf_circle_to_octagon(&x, &y, cardinal, diag_sum)` and `mf_octagon_gate()`
  both take the gate as parameters. Measure a real GC pad, or read the clamp
  constants the game passes to `PADClamp`.
- If the game calls `PADClamp` itself, give it the stretched-but-unclamped
  values and let it clamp, and **skip `mf_octagon_gate`**, or you're clamping
  twice.
- C-stick camera: feed the right stick into `substickX/Y` the same way, and let
  the game's own camera code run. Use `mf_look_*` only when you add a new
  free-look the game doesn't have.
- Analog triggers: `mf_trigger_level()` gives 0..1 from the threshold. Scale it
  to the game's `u8` range.
- Mouse: hooks C and D. Most GC games keep the camera as an angle in a camera
  struct. Add the degrees there after the game's camera update.

### PS2 decomps and recomps

- `libpad` analog bytes are **0..255 with 0x80 at centre and Y down**. Convert a
  shaped value with `(uint8_t)(128 + v * 127 / 32767)`, and flip Y.
- The DualShock 2 has a round gate, so **skip the octagon functions**. Use
  `mf_axis_deadzone()` only, and only if the game doesn't apply a large deadzone
  of its own. Many PS2 games do.
- The game probably has its own right-stick camera. Feed the stick into that
  path. Mouse goes in at hooks C and D as an angle, **never** written into the
  analog bytes: a mouse sent through the stick bytes picks up the game's stick
  curve and deadzone, which is exactly the floatiness you're avoiding.
- Pressure-sensitive buttons: `mf_trigger_level()` is a reasonable mapping from
  a modern trigger.

### Xbox (original) ports and recomps

- `XINPUT_GAMEPAD`-style `SHORT` thumbsticks, −32768..32767, **Y up**, round
  gate. Pass sticks through `mf_axis_deadzone()` and **skip the octagon
  functions**.
- Xbox games usually apply their own thumbstick deadzone (often near XInput's
  7849 for the left stick and 8689 for the right). If the game does, use a
  small deadzone of your own (just enough to kill the noise of a modern stick),
  or none, so the two don't stack.
- Analog face buttons and triggers (0..255): use `mf_trigger_level()`, scaled
  to 255.
- Mouse and look: hooks C and D, as above. Halo-likes often have a separate
  aim-assist and turn-acceleration stage on the stick. The mouse must bypass
  it.

## 5. A checklist before you call it done

- [ ] Mouse counts: one source (events *or* polled state), accumulated, consumed once per sim tick.
- [ ] The mouse is applied as an angle, after the game's camera integration.
- [ ] One permission predicate zeroes the mouse in menus, pause, cutscenes, death and demos.
- [ ] Zooming slows mouse and stick, and raising the FOV setting does not speed them up.
- [ ] Yaw and pitch at the same input feel equal, at both FOV 60 and FOV 120.
- [ ] Inverted and upright pitch both work, for mouse and stick.
- [ ] In split-screen, only player 1's view moves with the mouse.
- [ ] In aim mode, drawing a circle with the mouse makes a circle on screen.
