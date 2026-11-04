/**
 * mf_look.h - controller LOOK response, lifted from Project Midas.
 *
 * What players praise as "the look on the controller" is the N64-style
 * response that Project Midas sends the modern right stick through:
 *
 *   right stick (raw) -> 10% per-axis deadzone, rescaled -> +/-85 N64 units
 *     -> N64 octagon gate (85 cardinal, |x|+|y| <= 130)
 *     -> 5-unit dead band, subtracted (no step at the edge)
 *     -> / 70, clamped to +/-1   (full speed at ~88% of the throw)
 *     -> SQUARED, sign kept      (a small lean aims, a full push spins)
 *     -> x (current FOV / unzoomed FOV)  (zoom slows the look; a wide FOV
 *                                        setting does NOT speed it up)
 *     -> x 210 deg/s             (3.5 degrees per 60 Hz field, the game's own)
 *
 * applied as an angular RATE scaled by the real frame time, so it is identical
 * at 30, 60 or 240 fps. Pitch uses the same curve and rate as yaw.
 *
 * Earlier Midas builds used a linear response, then a "degrees path" with an
 * 8-unit dead zone and 220/140 deg/s; players preferred the curve above.
 * The older numbers are kept as mf_look_cfg_midas_direct() for A/B tests.
 *
 * Pure math, no SDL, no state.
 */
#ifndef MIDAS_FEEL_MF_LOOK_H
#define MIDAS_FEEL_MF_LOOK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf_look_cfg
{
    int32_t cardinal;      /* octagon gate: per-axis clamp (85) */
    int32_t diag_sum;      /* octagon gate: |x|+|y| clamp (130); 0 = no gate */
    int32_t dead;          /* dead band subtracted from each axis (5) */
    float   full;          /* deflection past the dead band that is full speed (70) */
    float   exponent;      /* response curve: 2 = squared (N64), 1 = linear */
    float   yaw_dps;       /* full-speed yaw, degrees per second (210) */
    float   pitch_dps;     /* full-speed pitch, degrees per second (210) */
} mf_look_cfg;

/** The shipped Project Midas look stick (the vanilla N64 natural-look path). */
void mf_look_cfg_n64(mf_look_cfg *cfg);

/** The older GE_PADLOOK=direct path: dead 8, squared over 77, 220 / 140 deg/s, no gate. */
void mf_look_cfg_midas_direct(mf_look_cfg *cfg);

/**
 * Raw right stick (-32768..32767, Y up-positive) to N64 units with the Look
 * deadzone, exactly as Project Midas feeds the look path. dz = 3277 default.
 */
void mf_look_stick_to_n64(int32_t raw_x, int32_t raw_y_up, int32_t dz,
                          int32_t *out_x, int32_t *out_y);

/**
 * Look stick (N64 units, Y up-positive) to an angular rate in degrees per
 * second. fov_ratio = current FOV / the player's unzoomed FOV (1.0 unzoomed,
 * < 1 while zoomed in); pass 1.0 if the game has no zoom.
 * Positive yaw = turn right, positive pitch = look up. Apply your game's own
 * invert-Y setting AFTER this call.
 */
void mf_look_rate(const mf_look_cfg *cfg, int32_t look_x, int32_t look_y, float fov_ratio,
                  float *yaw_dps, float *pitch_dps);

/**
 * Convenience: the rate for one frame, in degrees. dt_sec = real frame time.
 * Accumulate these into the camera angle; do NOT feed them through the game's
 * own turn-acceleration if it has one (that is what makes ports feel floaty).
 */
void mf_look_step(const mf_look_cfg *cfg, int32_t look_x, int32_t look_y, float fov_ratio,
                  float dt_sec, float *yaw_deg, float *pitch_deg);

#ifdef __cplusplus
}
#endif

#endif
