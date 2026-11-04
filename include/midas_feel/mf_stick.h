/**
 * mf_stick.h - analog stick shaping, lifted from Project Midas.
 *
 * Pure math, no SDL, no allocation, no global state. Every function takes raw
 * values in and returns shaped values out, so it drops into any port no matter
 * how that port reads its pads.
 *
 * Units used throughout:
 *   raw axis   SDL-style signed 16-bit, -32768..32767. Y is UP-positive here;
 *              SDL reports Y down-positive, so negate SDL's Y before calling.
 *   N64 units  the signed byte an N64 game reads from OSContPad.stick_x/y.
 *              A good original pad reaches about +/-85 on a cardinal and
 *              about (65, 65) on a diagonal: the N64's octagonal gate.
 *
 * Three stick modes come out of Project Midas, chosen per DEVICE, not per user:
 *   scaled      generic modern pad: deadzone rescale -> N64 units -> stretch
 *               the round gate onto the N64 octagon -> clamp to the octagon.
 *   raw         raphnet-class adapter with a real N64 pad: the adapter already
 *               reports true N64 units. No deadzone, no reshaping, nothing.
 *               This is most of the reason a raphnet "feels right".
 *   calibrated  8-point measured octagon (a worn stick, an odd adapter) mapped
 *               piecewise-linearly onto the ideal N64 octagon.
 */
#ifndef MIDAS_FEEL_MF_STICK_H
#define MIDAS_FEEL_MF_STICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The N64's gate, as the game expects it. */
#define MF_N64_CARDINAL   85   /* full push straight up/down/left/right */
#define MF_N64_DIAG_SUM  130   /* |x| + |y| on the diagonal faces: (65, 65) */

/* Project Midas defaults. */
#define MF_DEADZONE_DEFAULT        3277  /* of 32768, 10% - Move and Look stick */
#define MF_TRIGGER_THRESHOLD_DEFAULT 3276 /* 10% of a trigger pull = "pressed" */

/**
 * Per-axis deadzone with rescale (the Perfect Dark port's inputAxisScale):
 * values inside +/-dz read 0, and the remaining travel is stretched back to
 * the full range so there is no jump at the deadzone edge and full push still
 * reaches 32767. `scale` is an extra sensitivity multiplier (1.0 = none).
 * Returns -32768..32767.
 */
int32_t mf_axis_deadzone(int32_t raw, int32_t dz, float scale);

/**
 * Stretch a ROUND stick onto the N64 octagon, in N64 units, in place.
 * A modern pad's circular housing only reaches ~(60, 60) on a diagonal where
 * the N64 reaches (65, 65), so diagonals walk ~8% slow without this.
 * Cardinals are unchanged and half a push stays half a push: each direction's
 * full reach is scaled to the octagon edge in that direction.
 *
 * `cardinal` / `diag_sum` describe the target gate; pass MF_N64_CARDINAL /
 * MF_N64_DIAG_SUM for N64 games (Project Midas's exact behaviour).
 */
void mf_circle_to_octagon(int32_t *x, int32_t *y, int32_t cardinal, int32_t diag_sum);

/**
 * Clamp a stick to the octagon: each axis to +/-cardinal, then |x|+|y| to
 * diag_sum (scaled down along the same direction). Apply to SCALED sticks only
 * - a raw raphnet stick IS the real stick and clipping it defeats the point.
 */
void mf_octagon_gate(int32_t *x, int32_t *y, int32_t cardinal, int32_t diag_sum);

/**
 * The whole "scaled" pipeline for a generic pad's Move stick, as Project Midas
 * runs it: deadzone rescale -> N64 units -> circle-to-octagon -> octagon gate.
 * raw_x / raw_y_up are raw axes with Y up-positive.
 */
void mf_stick_scaled_n64(int32_t raw_x, int32_t raw_y_up, int32_t dz, float scale,
                         int8_t *out_x, int8_t *out_y);

/**
 * The "raw" pipeline for a raphnet-class adapter read through SDL's joystick
 * API (VID 0x289B): the adapter reports N64 units x256 across the SDL range,
 * so /256 recovers the real value exactly. Clamped to +/-127 so a full-range
 * reading can never wrap the byte. No deadzone. Y must already be up-positive.
 */
int8_t mf_stick_raw_units(int32_t raw_axis);

/* ---- 8-point calibration ---------------------------------------------- */

/**
 * Measured full-push readings, in raw axis units (an N64 value x256 for a
 * raphnet RAW stick), in the order E, NE, N, NW, W, SW, S, SE, Y up-positive.
 * Measure each by asking the player to hold the stick hard into that notch.
 */
typedef struct mf_calib
{
    int16_t pt[8][2];
} mf_calib;

/** The ideal N64 octagon the calibration maps onto, same order as mf_calib. */
extern const int16_t mf_n64_ideal_octagon[8][2];

/**
 * Map a raw reading through the measured octagon onto the ideal one: find the
 * sector the reading falls in, express it as a*corner1 + b*corner2, and rebuild
 * it from the ideal corners. Readings past the measured edge clamp to it.
 * `radial_dz` is a radial deadzone in raw units (0 for none).
 * Returns 1 and writes N64 units; returns 0 (and writes 0, 0) inside the
 * deadzone or if no sector matched (degenerate calibration).
 */
int mf_calib_apply(const mf_calib *c, int32_t raw_x, int32_t raw_y_up, int32_t radial_dz,
                   int8_t *out_x, int8_t *out_y);

/* ---- triggers ---------------------------------------------------------- */

/**
 * How far a trigger is pulled, 0..1, counted from the threshold so it ramps
 * from 0 rather than jumping straight to the threshold's own fraction.
 * `raw` is 0..32767 (SDL GameController trigger range).
 */
float mf_trigger_level(int32_t raw, int32_t threshold);

#ifdef __cplusplus
}
#endif

#endif
