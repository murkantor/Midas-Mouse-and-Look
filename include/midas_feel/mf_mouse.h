/**
 * mf_mouse.h - mouse look and mouse aim, lifted from Project Midas.
 *
 * Why the Midas mouse feels smooth and responsive comes down to a few rules.
 * The code here is small; the RULES are the point (docs/FEEL.md has the why):
 *
 *  1. Raw counts only. No OS acceleration, no OS pointer-speed curve, no
 *     acceleration of our own, ever. (SDL2: see mf_mouse_sdl_hints below.)
 *  2. The mouse moves the camera ANGLE directly - degrees per count - added
 *     AFTER the game's own camera integration. It is never fed in as a stick
 *     or turn RATE: a game's turn smoothing / acceleration applied to a mouse
 *     IS the "floaty" feel.
 *  3. Every count is used exactly once. Accumulate counts from every poll
 *     between two simulation ticks and hand the sum to the tick that
 *     consumes them (mf_mouse_add / mf_mouse_tick). Uncapped renderers poll
 *     several times per tick; sampling only the last poll drops motion.
 *  4. Drain every tick, but DISCARD the counts whenever the game would not
 *     obey the stick (menus, pause, cutscenes, death, demo playback), so
 *     motion never banks up and snaps the view when control returns.
 *  5. Scale by current FOV / unzoomed FOV: zoom slows the mouse like it
 *     slows the stick, but a wider FOV setting does not make it twitchier.
 *  6. In an aim mode (crosshair moves inside the screen) the crosshair moves
 *     the same number of PIXELS per degree horizontally and vertically, it
 *     lands where the mouse puts it the same tick (no glide), and holding it
 *     in the outer 28% of its reach turns the view.
 *
 * Sensitivity: degrees = counts / 10 * (sens / 2.5). Midas's default
 * sens = 2.5 equals the 1964 GoldenEye/Perfect Dark mouse injector's default
 * SENSITIVITY 40, so 0.1 degree per count at default.
 *
 * Pure math + a tiny accumulator; no SDL, no global state.
 */
#ifndef MIDAS_FEEL_MF_MOUSE_H
#define MIDAS_FEEL_MF_MOUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_MOUSE_SENS_DEFAULT 2.5f

/* Crosshair travel per degree of mouse movement in aim mode, in "box" units
 * where +/-1 is the viewport edge. Matched to the 1964 injector: the stock
 * GoldenEye aim reach (0.65) takes ~376 counts at default sensitivity. */
#define MF_AIM_BOX_PER_DEG 0.0173f

/* Aim-mode edge turn: the view starts turning once the crosshair passes this
 * fraction of its reach, ramping linearly to full speed at the edge. */
#define MF_AIM_EDGE_ONSET 0.72f

typedef struct mf_mouse_cfg
{
    float sens_x;      /* 2.5 default */
    float sens_y;      /* 2.5 default */
    float sens_aim;    /* multiplier for aim-mode crosshair speed, 1.0 default */
    int   invert_y;    /* 0 = mouse forward looks up (standard FPS) */
} mf_mouse_cfg;

void mf_mouse_cfg_default(mf_mouse_cfg *cfg);

/** Per-player accumulator. Zero-initialise, or call mf_mouse_reset. */
typedef struct mf_mouse
{
    int32_t acc_dx, acc_dy;    /* counts since the last tick */
    int32_t tick_dx, tick_dy;  /* counts the current tick consumes */
    int32_t moved_acc;         /* any poll since the last tick saw motion */
    int32_t moved_tick;
} mf_mouse;

void mf_mouse_reset(mf_mouse *m);

/**
 * Add relative counts. Call on every poll / every SDL_MOUSEMOTION event
 * while the pointer is captured. ADD, never overwrite.
 */
void mf_mouse_add(mf_mouse *m, int32_t dx, int32_t dy);

/**
 * Once per SIMULATION tick (not per rendered frame), before the game reads
 * input: move the accumulated counts into tick_dx/dy and start a new window.
 * Every reader within that tick then sees one stable value.
 */
void mf_mouse_tick(mf_mouse *m);

/**
 * Throw away anything pending. Call when capture starts (drops the pre-lock
 * jump), when the window loses focus, and on ticks where look is not
 * permitted (rule 4) - after mf_mouse_tick, so the window still drains.
 */
void mf_mouse_discard(mf_mouse *m);

/**
 * This tick's motion as camera degrees, sensitivity and invert applied.
 * Positive yaw = turn right, positive pitch = look up (mouse forward).
 * Multiply by mf_fov_ratio() and add straight onto the camera angles.
 */
void mf_mouse_deg(const mf_mouse *m, const mf_mouse_cfg *cfg, float *yaw_deg, float *pitch_deg);

/** Same conversion for arbitrary counts (for ports that keep their own accumulator). */
void mf_mouse_counts_to_deg(int32_t dx, int32_t dy, const mf_mouse_cfg *cfg,
                            float *yaw_deg, float *pitch_deg);

/**
 * current FOV / unzoomed FOV, guarded. Use the SAME kind of FOV (vertical or
 * horizontal) for both arguments.
 */
float mf_fov_ratio(float current_fov, float unzoomed_fov);

/* ---- aim mode (a crosshair that moves inside the screen) ------------- */

/**
 * Move an aim-mode crosshair by this tick's mouse degrees.
 *   *box_x, *box_y   crosshair position in box units (+/-1 = viewport edge),
 *                    +x right, +y up. Updated in place.
 *   reach_x, reach_y clamp (GoldenEye's is 0.65 of the box).
 *   vp_w, vp_h       viewport size in pixels: Y is scaled by w/h so one
 *                    degree is the same PIXEL distance on both axes (without
 *                    it a 4:3 view moves 4/3 as far sideways, a 2-player
 *                    half-height view ~3x).
 */
void mf_aim_mouse_step(float *box_x, float *box_y, float yaw_deg, float pitch_deg,
                       const mf_mouse_cfg *cfg, float vp_w, float vp_h,
                       float reach_x, float reach_y);

/**
 * Edge turn for one axis: how hard the view should turn (-1..1) because the
 * crosshair is held near the edge of its reach. 0 inside MF_AIM_EDGE_ONSET of
 * the reach, ramping to +/-1 at the edge. Feed it to the game's own aim-turn
 * input. Use only for MOUSE-driven aim; stick aim keeps the game's own band.
 */
float mf_aim_edge_push(float box_pos, float reach);

/**
 * Crosshair "snap": many console games glide the crosshair toward its target
 * through a one-pole low-pass, pos = pos * damp + target each tick (GoldenEye:
 * damp ~0.88, ~140 ms time constant). Fine for a stick, floaty for a mouse.
 * When the mouse drives the crosshair, set pos to the filter's steady state
 * instead: this returns target / (1 - damp). Returns target if damp >= 1.
 */
float mf_lowpass_steady_state(float target, float damp);

#ifdef __cplusplus
}
#endif

#endif
