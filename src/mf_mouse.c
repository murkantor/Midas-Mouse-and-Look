/* mf_mouse.c - see include/midas_feel/mf_mouse.h. Source: Project Midas
 * port/src/input_pc.c (portInputMouseUpdate, portInputMouseTickConsume,
 * portInputMouseLookDeltaDeg) and src/game/bondview2.c (portMouseFovRatio,
 * portAimIsoY, the aim-mode swivel and edge turn), src/game/gunfire.c
 * (the crosshair snap). The angle model follows the 1964 GoldenEye / Perfect
 * Dark mouse injector, reimplemented; its acceleration curve was NOT ported. */
#include "midas_feel/mf_mouse.h"

void mf_mouse_cfg_default(mf_mouse_cfg *cfg)
{
    cfg->sens_x = MF_MOUSE_SENS_DEFAULT;
    cfg->sens_y = MF_MOUSE_SENS_DEFAULT;
    cfg->sens_aim = 1.0f;
    cfg->invert_y = 0;
}

void mf_mouse_reset(mf_mouse *m)
{
    m->acc_dx = m->acc_dy = 0;
    m->tick_dx = m->tick_dy = 0;
    m->moved_acc = m->moved_tick = 0;
}

void mf_mouse_add(mf_mouse *m, int32_t dx, int32_t dy)
{
    m->acc_dx += dx;
    m->acc_dy += dy;
    m->moved_acc |= (dx != 0 || dy != 0);
}

void mf_mouse_tick(mf_mouse *m)
{
    m->tick_dx = m->acc_dx;
    m->tick_dy = m->acc_dy;
    m->acc_dx = m->acc_dy = 0;
    m->moved_tick = m->moved_acc;
    m->moved_acc = 0;
}

void mf_mouse_discard(mf_mouse *m)
{
    mf_mouse_reset(m);
}

void mf_mouse_counts_to_deg(int32_t dx, int32_t dy, const mf_mouse_cfg *cfg,
                            float *yaw_deg, float *pitch_deg)
{
    /* /2.5 puts the default sens (2.5) on the injector's SENSITIVITY 40:
     * 0.1 degree per count. Strictly linear. */
    const float yaw = ((float) dx / 10.0f) * (cfg->sens_x / 2.5f);
    float pitch = ((float) dy / 10.0f) * (cfg->sens_y / 2.5f);

    /* SDL / OS counts are +y DOWN; mouse forward (dy < 0) looks up */
    pitch = -pitch;
    if (cfg->invert_y)
    {
        pitch = -pitch;
    }
    if (yaw_deg != 0) *yaw_deg = yaw;
    if (pitch_deg != 0) *pitch_deg = pitch;
}

void mf_mouse_deg(const mf_mouse *m, const mf_mouse_cfg *cfg, float *yaw_deg, float *pitch_deg)
{
    mf_mouse_counts_to_deg(m->tick_dx, m->tick_dy, cfg, yaw_deg, pitch_deg);
}

float mf_fov_ratio(float current_fov, float unzoomed_fov)
{
    if (unzoomed_fov < 1.0f || current_fov <= 0.0f)
    {
        return 1.0f;
    }
    return current_fov / unzoomed_fov;
}

static float mf_clampf(float v, float lim)
{
    if (lim <= 0.0f) return v;
    return (v > lim) ? lim : ((v < -lim) ? -lim : v);
}

void mf_aim_mouse_step(float *box_x, float *box_y, float yaw_deg, float pitch_deg,
                       const mf_mouse_cfg *cfg, float vp_w, float vp_h,
                       float reach_x, float reach_y)
{
    const float k = MF_AIM_BOX_PER_DEG * ((cfg->sens_aim > 0.0f) ? cfg->sens_aim : 1.0f);
    const float iso = (vp_w > 0.0f && vp_h > 0.0f) ? vp_w / vp_h : 1.0f;

    *box_x = mf_clampf(*box_x + yaw_deg * k, reach_x);
    *box_y = mf_clampf(*box_y + pitch_deg * k * iso, reach_y);
}

float mf_aim_edge_push(float pos, float reach)
{
    float push;

    if (reach <= 0.0f)
    {
        return 0.0f;
    }
    push = (((pos < 0.0f) ? -pos : pos) / reach - MF_AIM_EDGE_ONSET) / (1.0f - MF_AIM_EDGE_ONSET);
    if (push <= 0.0f)
    {
        return 0.0f;
    }
    if (push > 1.0f)
    {
        push = 1.0f;
    }
    return (pos < 0.0f) ? -push : push;
}

float mf_lowpass_steady_state(float target, float damp)
{
    return (damp < 1.0f) ? target / (1.0f - damp) : target;
}
