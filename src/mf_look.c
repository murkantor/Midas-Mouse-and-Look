/* mf_look.c - see include/midas_feel/mf_look.h. Source: Project Midas
 * src/game/bondview2.c (the Look-stick feed into the natural-look path, and
 * the vanilla canNaturalTurn / canNaturalPitch speed computation) and
 * port/src/input_pc.c (inputAxisScaleLook). */
#include "midas_feel/mf_look.h"
#include "midas_feel/mf_stick.h"

#include <math.h>

void mf_look_cfg_n64(mf_look_cfg *cfg)
{
    cfg->cardinal = MF_N64_CARDINAL;
    cfg->diag_sum = MF_N64_DIAG_SUM;
    cfg->dead = 5;
    cfg->full = 70.0f;
    cfg->exponent = 2.0f;
    cfg->yaw_dps = 3.5f * 60.0f;     /* 3.5 degrees per 60 Hz field */
    cfg->pitch_dps = 3.5f * 60.0f;
}

void mf_look_cfg_midas_direct(mf_look_cfg *cfg)
{
    cfg->cardinal = MF_N64_CARDINAL;
    cfg->diag_sum = 0;
    cfg->dead = 8;
    cfg->full = 85.0f - 8.0f;
    cfg->exponent = 2.0f;
    cfg->yaw_dps = 220.0f;
    cfg->pitch_dps = 140.0f;
}

void mf_look_stick_to_n64(int32_t raw_x, int32_t raw_y_up, int32_t dz,
                          int32_t *out_x, int32_t *out_y)
{
    /* no circle-to-octagon on the Look stick: Project Midas sends it straight
     * through the N64 gate, which trims the round stick's diagonals */
    if (out_x != 0) *out_x = mf_axis_deadzone(raw_x, dz, 1.0f) * MF_N64_CARDINAL / 32768;
    if (out_y != 0) *out_y = mf_axis_deadzone(raw_y_up, dz, 1.0f) * MF_N64_CARDINAL / 32768;
}

static float mf_look_axis(const mf_look_cfg *cfg, int32_t v)
{
    float f;

    v = (v < -cfg->dead) ? v + cfg->dead : ((v > cfg->dead) ? v - cfg->dead : 0);
    if (v == 0 || cfg->full <= 0.0f)
    {
        return 0.0f;
    }
    f = (float) v / cfg->full;
    if (f > 1.0f) f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    if (cfg->exponent == 2.0f)
    {
        return f * ((f < 0.0f) ? -f : f);
    }
    return (f < 0.0f) ? -powf(-f, cfg->exponent) : powf(f, cfg->exponent);
}

void mf_look_rate(const mf_look_cfg *cfg, int32_t x, int32_t y, float fov_ratio,
                  float *yaw_dps, float *pitch_dps)
{
    if (cfg->cardinal > 0)
    {
        mf_octagon_gate(&x, &y, cfg->cardinal, cfg->diag_sum);
    }
    if (fov_ratio <= 0.0f)
    {
        fov_ratio = 1.0f;
    }
    if (yaw_dps != 0) *yaw_dps = mf_look_axis(cfg, x) * fov_ratio * cfg->yaw_dps;
    if (pitch_dps != 0) *pitch_dps = mf_look_axis(cfg, y) * fov_ratio * cfg->pitch_dps;
}

void mf_look_step(const mf_look_cfg *cfg, int32_t x, int32_t y, float fov_ratio,
                  float dt_sec, float *yaw_deg, float *pitch_deg)
{
    float yr = 0.0f, pr = 0.0f;

    mf_look_rate(cfg, x, y, fov_ratio, &yr, &pr);
    if (yaw_deg != 0) *yaw_deg = yr * dt_sec;
    if (pitch_deg != 0) *pitch_deg = pr * dt_sec;
}
