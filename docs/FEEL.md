# Why it feels the way it does

Project Midas gets a lot of compliments on how good the right-stick look is on a
modern controller and how smooth and responsive the mouse is. (How a real N64 pad
feels on a raphnet is covered by the separate
[Raphnet RAW driver](https://github.com/murkantor/Midas-Raphnet-Raw-Driver).)
Very little of that comes from clever arithmetic. Most of it comes from a short list of decisions about where input
enters the game and what is kept *out* of the path. This file is that list. If
you port only the code and break one of these rules, you lose the feel.

---

## 1. Modern pad, Move stick: make a round stick behave like an octagon

- **Deadzone with rescale** (10 %, per axis). The travel left after the deadzone
  is stretched back to the full range, so there is no jump at the deadzone edge
  and a full push still reaches maximum.
- **Circle to octagon.** A modern stick's round housing reaches only about
  (60, 60) on a diagonal, where the N64 reaches (65, 65). Without correction the
  diagonals walk about 8 % slow. Each direction's full reach is scaled out to the
  octagon edge in that direction. Cardinals are unchanged, and half a push stays
  half a push.
- **Octagon gate**: ±85 per axis, |x| + |y| ≤ 130. This applies to scaled
  sticks only, never to raw or calibrated ones.
- **8-point calibration** exists for worn sticks and odd adapters. You measure
  the eight notches and the mapping is piecewise-linear onto the ideal octagon.

The stick mode is chosen **by device, not by the user**: a raphnet-class adapter
gets raw (`mf_stick_raw_units`: the adapter reports N64 units x256 through the
joystick API, so /256 recovers the real stick, with no deadzone), a calibrated
device gets calibrated, and everything else gets scaled. Players can't pick the
wrong mode by accident.

## 2. Modern pad, Look stick: the "amazing look"

The right stick goes through the **N64 game's own natural-look path**: the same
dead band, curve and rate GoldenEye uses for its stick-look control styles,
rather than a new curve written for PC. Players tried a linear response (twitchy
at the centre, slow at the rim) and a separate PC-tuned curve, and preferred
this one:

```
deadzone 10% rescaled -> +/-85 -> octagon gate (85 / 130) -> dead band 5 (subtracted)
-> /70, clamp +/-1 -> squared, sign kept -> x (FOV / unzoomed FOV) -> x 210 deg/s
```

- **Squared response.** A small lean aims precisely and a full push spins.
- **Full speed at about 88 % of the throw**, so you don't have to slam the stick
  into the rim.
- **No turn acceleration and no smoothing.** The rate takes effect immediately.
- **Scaled by real frame time**, so it's identical at 30, 60 or 240 fps.
- **Divided by the *unzoomed* FOV, not a literal 60.** A zoom slows the look
  down, but raising the FOV setting doesn't make the look twitchier. (Midas
  shipped a bug where some look paths divided by 60: at FOV 120, yaw ran twice
  as fast as pitch.)
- Yaw and pitch use the same curve and rate.

For a non-N64 game, `mf_look_cfg` exposes every number. Keep the shape (dead
band, squared, rate × dt, FOV-scaled) and tune `yaw_dps` / `pitch_dps` to suit
the game.

## 3. Mouse: an angle, not a rate

This is the biggest single difference between "floaty" ports and Midas.

1. **Raw counts only.** Under SDL2, set `SDL_HINT_MOUSE_RELATIVE_MODE_WARP=0`
   (raw input, never the warped OS pointer with "Enhance pointer precision") and
   `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE=0` (no Windows pointer-speed curve).
   Never add acceleration of your own. The 1964 injector Midas modelled this on
   has an optional acceleration curve, and it was deliberately not ported.
2. **The mouse moves the camera ANGLE directly**, at 0.1° per count at the
   default sensitivity, added *after* the game's own camera integration. It is
   **never** fed in as a stick deflection or a turn rate. If you feed a rate,
   the game's turn smoothing and acceleration act on the mouse, and that *is*
   the floaty feeling. Midas's first version did this and it was reworked.
3. **Accumulate, then consume once per simulation tick.** Add the counts from
   every poll or event between two sim ticks and give the total to the tick. An
   uncapped renderer polls several times per tick, and reading only the last
   poll throws motion away.
4. **Drain every tick, but discard while look isn't allowed** (menus, pause,
   cutscenes, death, demo playback). Motion never banks up and snaps the view
   when control comes back. Midas uses **one** permission predicate for this:
   "would the stick move the player right now?". Every consumer reads the
   already-gated value, and none of them re-checks permission. Per-consumer
   checks kept missing cases.
5. **Drop the first delta after capture**, because it carries the jump from
   wherever the OS pointer was. Release capture when the window loses focus.
6. **FOV scaling, the same as the stick:** current FOV divided by unzoomed FOV.
7. **Player 1 only in split-screen**, and zero the deltas on other players'
   ticks, or one mouse turns everyone.

### Aim mode (a crosshair that moves inside the screen)

- **The same pixels per degree on both axes.** Console games usually keep the
  aim crosshair in "box" units, where ±1 is the viewport edge. That's right for a
  stick, but wrong for a mouse: one degree of hand movement then moves the
  crosshair 4/3 as many pixels sideways as up in a 4:3 view, and about 3× in a
  two-player half-height view. Scale the mouse's Y by width ÷ height.
- **No glide.** Many games ease the crosshair toward its target with a one-pole
  low-pass (GoldenEye: damp ≈ 0.88, a time constant of about 140 ms). That's fine
  for a stick held in position, but feels like dragging through syrup with a
  mouse. When the mouse is driving, set the crosshair to the filter's steady
  state (`target / (1 - damp)`) so it lands where the mouse put it on the same
  tick. Gun and crosshair still move together because both derive from that
  value. Stick aim keeps the glide. Players confirmed this "feels good".
- **Edge turn, mouse only.** Holding the crosshair beyond 72 % of its reach
  turns the view, ramping linearly to full speed at the edge. Stick aim keeps the
  game's own band.
- **A separate aim sensitivity** (`sens_aim`), because dragging a crosshair and
  rotating a camera want different speeds.

## 4. Things that look like feel but are really bugs

These cost Midas time. Check for them in your port before you blame the curves.

- **Stacked deadzones.** Your 10 % plus the game's own dead band plus the OS's.
  Only one of them should be doing real work.
- **FOV normalised against a literal number** in some code paths but not
  others, so yaw and pitch scale differently once the FOV setting changes.
- **Pitch sign conventions.** Engines often count pitch downward, and stick and
  mouse can arrive with opposite signs. Fix the sign in one place, then test
  both the upright and the inverted setting.
- **Sampling the mouse per rendered frame but consuming it per sim tick** (or
  the other way round). Motion is lost or counted twice.
- **The mouse moving during attract-mode demos.** It steers the replay camera
  and desyncs the recording. Your permission predicate must say "no" during
  demo playback.
