# Where this came from

This library was extracted from Project Midas (the GoldenEye 007 PC port), where
a copy is also kept as `libs/midas-feel`. This file maps each piece back to its
source, so fixes can be carried in either direction.

## Function map

| Midas Mouse and Look | Project Midas source |
|---|---|
| `mf_axis_deadzone` | `port/src/input_pc.c` `inputAxisScaleDz` |
| `mf_circle_to_octagon` | `port/src/input_pc.c` `inputCircleToOctagon` (85 / 130 now parameters) |
| `mf_octagon_gate` | `port/src/input_pc.c` `portInputStickGate` (85 / 130 now parameters) |
| `mf_stick_scaled_n64` | the scaled Move-stick path in `portInputPadMerge`, plus `portInputStickGate` |
| `mf_stick_raw_units` | `port/src/input_pc.c` `inputRawUnits` |
| `mf_calib_apply`, `mf_n64_ideal_octagon` | `port/src/input_pc.c` `inputCalibApply`, `sCalibIdeal` |
| `mf_trigger_level` | the trigger branch of `inputEntryLevel` |
| `mf_look_stick_to_n64` | `inputAxisScaleLook(..) * 85 / 32768` in `portInputPadMerge` |
| `mf_look_rate` / `mf_look_step` | `src/game/bondview2.c`: the Look stick feed into `analogTurn/analogPitch` (gate, dead 5) and the vanilla `canNaturalTurn` / `canNaturalPitch` speed (`/70`, square, `viGetFovY() / GE_DEFAULT_FOV`, `* g_GlobalTimerDelta * 3.5`) |
| `mf_look_cfg_midas_direct` | `bondview2.c` `GE_PADLOOK=direct` path (`PORT_PADLOOK_*_DEG_PER_SEC`) |
| `mf_mouse_add` / `mf_mouse_tick` | `port/src/input_pc.c` `portInputMouseUpdate` (accumulation), `portInputMouseTickConsume` |
| `mf_mouse_counts_to_deg` | `port/src/input_pc.c` `portInputMouseLookDeltaDeg` |
| `mf_fov_ratio` | `bondview2.c` `portMouseFovRatio` (see differences) |
| `mf_aim_mouse_step`, `MF_AIM_BOX_PER_DEG` | `bondview2.c` aim-mode swivel accumulation, `AIM_CROSSHAIR_PER_DEG`, `portAimIsoY` |
| `mf_aim_edge_push`, `MF_AIM_EDGE_ONSET` | `bondview2.c` aim edge turn (`AIMSCROLL_ONSET`) |
| `mf_lowpass_steady_state` | `src/game/gunfire.c` `g_portAimCrosshairSnap` branch |

Background reading in Project Midas: `docs/port/input-and-display.md` (mouse
look, aim isotropy, look speed against FOV, crosshair snap),
`docs/port/port-inputs-vs-n64-inputs.md` (the Look stick through the N64 stick
path) and `docs/port/bug-triage-2026-08.md` (F7.1: why the mouse became an
angle).

## Differences from the Midas code

These are deliberate. Everything not listed behaves identically, and
`tests/mf_test.c` checks the math against verbatim copies of the originals.

- **`mf_fov_ratio(cur, base)`** is a plain `cur / base`. Midas's
  `portMouseFovRatio` uses 60 as the base whenever the current FOV is ≤ 60, which
  is specific to how GoldenEye's FOV override and zoom interact. The stick-look
  paths in Midas already use `cur / configured base`, which is what this
  function gives.
- **`mf_calib_apply`** returns 0 and writes (0, 0) inside the deadzone. Midas
  left the pad value untouched there, so a keyboard merged into the same pad
  kept its value.
- **Sign conventions** are made explicit: positive yaw is right and positive
  pitch is up, for stick, mouse and aim box. Midas follows GoldenEye's internal
  conventions (`vv_verta` counts downward, `invertPitch == 1` means upright), so
  the signs are flipped at different places there.
- **The aim box reach** is a parameter. Midas uses GoldenEye's 0.65, widened in
  wide single-player windows (`portAimReachX`, a GoldenEye HUD detail that was
  not extracted).

## Left out on purpose

These parts are GoldenEye- or Midas-specific and wouldn't carry over:

- Binding, layout presets, rebind capture, device records, and Player 1 claim
  and assignment (`input_pc.c`).
- The Port action suite (`joy_port.c`) and GoldenEye control-style and
  twin-stick routing.
- The GoldenEye camera, tank turret, Look Ahead, aim lock and widescreen aim
  reach.
- Input recording and playback, and the headless harnesses.
- Mouse in menus: the virtual cursor, warping, and parking.
- The raphnet driver, which has its own repository:
  [Midas-Raphnet-Raw-Driver](https://github.com/murkantor/Midas-Raphnet-Raw-Driver).

## Keeping in sync

If the feel changes in Project Midas, update the matching function here and its
`REF_*` copy in `tests/mf_test.c` together. Then the test shows exactly what
changed.
