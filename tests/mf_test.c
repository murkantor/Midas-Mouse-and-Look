/**
 * mf_test.c - checks the library against the Project Midas originals.
 *
 * The REF_* functions are copied from Project Midas (input_pc.c, bondview2.c)
 * with only the game globals turned into parameters. Each library function is
 * swept against its reference, so an edit that changes the feel fails here.
 */
#include "midas_feel/mf_stick.h"
#include "midas_feel/mf_look.h"
#include "midas_feel/mf_mouse.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int s_Fail;
static int s_Checks;

#define CHECK(cond, ...) do { s_Checks++; if (!(cond)) { s_Fail++; if (s_Fail < 20) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

/* ---- references (Project Midas, verbatim apart from parameters) -------- */

static int32_t REF_inputAxisScaleDz(int32_t x, int32_t dz, float stickScale)
{
    if (abs(x) < dz || dz >= 32768) return 0;
    if (x < 0) x += dz; else x -= dz;
    x = x * 32768 / (32768 - dz);
    x = (int32_t) (x * stickScale);
    return (x > 32767) ? 32767 : ((x < -32768) ? -32768 : x);
}

static void REF_inputCircleToOctagon(int32_t *x, int32_t *y)
{
    const double ax = (*x < 0) ? -(double) *x : (double) *x;
    const double ay = (*y < 0) ? -(double) *y : (double) *y;
    const double r = sqrt(ax * ax + ay * ay);
    double edge, f;

    if (r < 1.0) return;
    edge = 130.0 * r / (ax + ay);
    f = 85.0 * r / ((ax > ay) ? ax : ay);
    if (f < edge) edge = f;
    f = edge / 85.0;
    *x = (int32_t) ((double) *x * f + ((*x < 0) ? -0.5 : 0.5));
    *y = (int32_t) ((double) *y * f + ((*y < 0) ? -0.5 : 0.5));
}

static void REF_portInputStickGate(int32_t *px, int32_t *py)
{
    int32_t x = *px, y = *py;
    int32_t ax = (x < 0) ? -x : x;
    int32_t ay = (y < 0) ? -y : y;
    int32_t sum;

    if (ax > 85) { x = (x < 0) ? -85 : 85; ax = 85; }
    if (ay > 85) { y = (y < 0) ? -85 : 85; ay = 85; }
    sum = ax + ay;
    if (sum > 130) { x = x * 130 / sum; y = y * 130 / sum; }
    *px = x;
    *py = y;
}

/* bondview2.c: Look stick -> natural look (gate, dead 5) -> vanilla speed
 * (/70, clamp, square, x fov ratio) -> vv_theta += speed * dt * 3.5, in
 * degrees per 60 Hz field; returned here as degrees per second. */
static void REF_lookStick(int32_t lx, int32_t ly, float fovRatio, float *yawDps, float *pitchDps)
{
    int32_t gx = lx, gy = ly, ax, ay;
    float t, p;

    if (gx > 85) gx = 85; else if (gx < -85) gx = -85;
    if (gy > 85) gy = 85; else if (gy < -85) gy = -85;
    ax = (gx < 0) ? -gx : gx;
    ay = (gy < 0) ? -gy : gy;
    if (ax + ay > 130) { gx = gx * 130 / (ax + ay); gy = gy * 130 / (ax + ay); }
    gx = (gx < -5) ? gx + 5 : ((gx > 5) ? gx - 5 : 0);
    gy = (gy < -5) ? gy + 5 : ((gy > 5) ? gy - 5 : 0);

    t = (float) gx / 70.0f;
    if (t > 1) t = 1; else if (t < -1) t = -1;
    if (t >= 0) t *= t; else t *= -t;
    p = (float) gy / 70.0f;
    if (p > 1) p = 1; else if (p < -1) p = -1;
    if (p >= 0) p *= p; else p *= -p;

    *yawDps = t * fovRatio * 3.5f * 60.0f;
    *pitchDps = p * fovRatio * 3.5f * 60.0f;
}

static void REF_mouseDeg(int dx, int dy, float sx, float sy, float *yaw, float *pitch)
{
    *yaw = ((float) dx / 10.0f) * (sx / 2.5f);
    *pitch = ((float) dy / 10.0f) * (sy / 2.5f);
}

/* ---- tests ------------------------------------------------------------- */

static void test_deadzone(void)
{
    int32_t x;
    const int32_t dzs[] = { 0, 1000, 3277, 8000, 32767 };
    const float scales[] = { 1.0f, 1.5f };
    unsigned d, s;

    for (d = 0; d < sizeof(dzs) / sizeof(dzs[0]); d++)
        for (s = 0; s < 2; s++)
            for (x = -32768; x <= 32767; x += 7)
            {
                const int32_t a = mf_axis_deadzone(x, dzs[d], scales[s]);
                const int32_t b = REF_inputAxisScaleDz(x, dzs[d], scales[s]);
                CHECK(a == b, "deadzone x=%d dz=%d scale=%.1f: %d vs %d", x, dzs[d], scales[s], a, b);
            }
    CHECK(mf_axis_deadzone(32767, 3277, 1.0f) == 32766, "full push reaches the top: %d", mf_axis_deadzone(32767, 3277, 1.0f));
    CHECK(mf_axis_deadzone(3276, 3277, 1.0f) == 0, "inside deadzone");
}

static void test_octagon(void)
{
    int32_t x, y;

    for (x = -90; x <= 90; x++)
        for (y = -90; y <= 90; y++)
        {
            int32_t ax = x, ay = y, bx = x, by = y;
            mf_circle_to_octagon(&ax, &ay, MF_N64_CARDINAL, MF_N64_DIAG_SUM);
            REF_inputCircleToOctagon(&bx, &by);
            CHECK(ax == bx && ay == by, "circle->octagon (%d,%d): (%d,%d) vs (%d,%d)", x, y, ax, ay, bx, by);

            ax = bx = x * 2;
            ay = by = y * 2;
            mf_octagon_gate(&ax, &ay, MF_N64_CARDINAL, MF_N64_DIAG_SUM);
            REF_portInputStickGate(&bx, &by);
            CHECK(ax == bx && ay == by, "gate (%d,%d): (%d,%d) vs (%d,%d)", x * 2, y * 2, ax, ay, bx, by);
        }
    {   /* a round pad's full diagonal (~60,60) lands on the N64 corner (65,65) */
        int32_t dx = 60, dy = 60;
        mf_circle_to_octagon(&dx, &dy, MF_N64_CARDINAL, MF_N64_DIAG_SUM);
        CHECK(dx == 65 && dy == 65, "diagonal reach (%d,%d)", dx, dy);
    }
}

static void test_scaled_pipeline(void)
{
    int8_t x, y;

    mf_stick_scaled_n64(32767, 0, MF_DEADZONE_DEFAULT, 1.0f, &x, &y);
    CHECK(x == 84 && y == 0, "full right -> (%d,%d)", x, y);   /* 32766*85/32768 = 84 */
    mf_stick_scaled_n64(0, 0, MF_DEADZONE_DEFAULT, 1.0f, &x, &y);
    CHECK(x == 0 && y == 0, "centre");
    mf_stick_scaled_n64(23170, 23170, MF_DEADZONE_DEFAULT, 1.0f, &x, &y);
    CHECK(x + y <= MF_N64_DIAG_SUM && x >= 60 && y >= 60, "round diagonal -> (%d,%d)", x, y);
}

static void test_raw_units(void)
{
    CHECK(mf_stick_raw_units(80 * 256) == 80, "raw 80");
    CHECK(mf_stick_raw_units(-32768) == -127, "raw clamp low: %d", mf_stick_raw_units(-32768));
    CHECK(mf_stick_raw_units(32767) == 127, "raw clamp high");
    CHECK(mf_stick_raw_units(-5 * 256) == -5, "raw -5");
}

static void test_calib(void)
{
    mf_calib c;
    int i;
    int8_t x, y;

    /* a calibration that measured exactly the ideal octagon, x256, is identity */
    for (i = 0; i < 8; i++)
    {
        c.pt[i][0] = (int16_t) (mf_n64_ideal_octagon[i][0] * 256);
        c.pt[i][1] = (int16_t) (mf_n64_ideal_octagon[i][1] * 256);
    }
    CHECK(mf_calib_apply(&c, 85 * 256, 0, 0, &x, &y) == 1 && x == 85 && y == 0, "calib E (%d,%d)", x, y);
    CHECK(mf_calib_apply(&c, 65 * 256, 65 * 256, 0, &x, &y) == 1 && x == 65 && y == 65, "calib NE (%d,%d)", x, y);
    CHECK(mf_calib_apply(&c, 120 * 256, 0, 0, &x, &y) == 1 && x == 85, "calib clamps past edge (%d)", x);
    CHECK(mf_calib_apply(&c, 100, 100, 3277, &x, &y) == 0 && x == 0 && y == 0, "calib deadzone");

    /* a worn stick that only reaches 70 on cardinals / 50 on diagonals gets full reach back */
    for (i = 0; i < 8; i++)
    {
        const int diag = (i & 1);
        c.pt[i][0] = (int16_t) ((mf_n64_ideal_octagon[i][0] != 0) * (mf_n64_ideal_octagon[i][0] > 0 ? 1 : -1) * (diag ? 50 : 70) * 256);
        c.pt[i][1] = (int16_t) ((mf_n64_ideal_octagon[i][1] != 0) * (mf_n64_ideal_octagon[i][1] > 0 ? 1 : -1) * (diag ? 50 : 70) * 256);
    }
    CHECK(mf_calib_apply(&c, 0, 70 * 256, 0, &x, &y) == 1 && x == 0 && y == 85, "worn N (%d,%d)", x, y);
    CHECK(mf_calib_apply(&c, -50 * 256, -50 * 256, 0, &x, &y) == 1 && x == -65 && y == -65, "worn SW (%d,%d)", x, y);
}

static void test_look(void)
{
    mf_look_cfg cfg;
    int32_t x, y;
    const float fovs[] = { 1.0f, 0.5f, 0.25f };
    unsigned f;

    mf_look_cfg_n64(&cfg);
    for (f = 0; f < 3; f++)
        for (x = -100; x <= 100; x += 3)
            for (y = -100; y <= 100; y += 3)
            {
                float a1, a2, b1, b2;
                mf_look_rate(&cfg, x, y, fovs[f], &a1, &a2);
                REF_lookStick(x, y, fovs[f], &b1, &b2);
                CHECK(fabsf(a1 - b1) < 1e-3f && fabsf(a2 - b2) < 1e-3f,
                      "look (%d,%d) fov %.2f: (%.4f,%.4f) vs (%.4f,%.4f)", x, y, fovs[f], a1, a2, b1, b2);
            }
    {
        float yd, pd;
        mf_look_rate(&cfg, 85, 0, 1.0f, &yd, &pd);
        CHECK(fabsf(yd - 210.0f) < 1e-3f, "full yaw 210 deg/s: %.3f", yd);
        mf_look_rate(&cfg, 5, -5, 1.0f, &yd, &pd);
        CHECK(yd == 0.0f && pd == 0.0f, "dead band");
        mf_look_step(&cfg, 85, 0, 1.0f, 1.0f / 120.0f, &yd, &pd);
        CHECK(fabsf(yd - 1.75f) < 1e-4f, "120 fps step: %.4f", yd);
    }
    {   /* look stick conversion: 10% deadzone, rescaled, to +/-85 */
        int32_t lx, ly;
        mf_look_stick_to_n64(32767, -32768, MF_DEADZONE_DEFAULT, &lx, &ly);
        CHECK(lx == 84 && ly == -85, "look stick to n64 (%d,%d)", lx, ly);
    }
}

static void test_mouse(void)
{
    mf_mouse_cfg cfg;
    mf_mouse m;
    float yaw, pitch, ry, rp;
    float bx = 0.0f, by = 0.0f;

    mf_mouse_cfg_default(&cfg);
    mf_mouse_reset(&m);

    /* three polls inside one tick all count */
    mf_mouse_add(&m, 10, -4);
    mf_mouse_add(&m, 5, 0);
    mf_mouse_add(&m, 0, -6);
    mf_mouse_tick(&m);
    CHECK(m.tick_dx == 15 && m.tick_dy == -10 && m.moved_tick, "accumulate (%d,%d)", m.tick_dx, m.tick_dy);
    mf_mouse_deg(&m, &cfg, &yaw, &pitch);
    REF_mouseDeg(15, -10, 2.5f, 2.5f, &ry, &rp);
    CHECK(fabsf(yaw - ry) < 1e-6f && fabsf(pitch + rp) < 1e-6f, "deg (%.3f,%.3f) vs (%.3f,%.3f)", yaw, pitch, ry, -rp);
    CHECK(fabsf(yaw - 1.5f) < 1e-6f && fabsf(pitch - 1.0f) < 1e-6f, "0.1 deg per count, forward = up");

    mf_mouse_tick(&m);   /* no new motion: next tick is zero, not a repeat */
    CHECK(m.tick_dx == 0 && m.tick_dy == 0 && !m.moved_tick, "drained");

    cfg.invert_y = 1;
    mf_mouse_counts_to_deg(0, -10, &cfg, &yaw, &pitch);
    CHECK(pitch < 0.0f, "invert");
    cfg.invert_y = 0;

    CHECK(fabsf(mf_fov_ratio(30.0f, 60.0f) - 0.5f) < 1e-6f, "fov ratio");
    CHECK(mf_fov_ratio(30.0f, 0.0f) == 1.0f, "fov guard");

    /* isotropy: 1 degree right and 1 degree up move the same pixels on 320x240 */
    mf_aim_mouse_step(&bx, &by, 1.0f, 1.0f, &cfg, 320.0f, 240.0f, 0.65f, 0.65f);
    CHECK(fabsf(bx * 160.0f - by * 120.0f) < 1e-4f, "iso px x %.4f y %.4f", bx * 160.0f, by * 120.0f);
    {   /* ~376 counts at default sens reach the GoldenEye 0.65 box */
        float x = 0.0f, yy = 0.0f;
        mf_aim_mouse_step(&x, &yy, 37.6f, 0.0f, &cfg, 320.0f, 240.0f, 0.0f, 0.0f);
        CHECK(fabsf(x - 0.65f) < 0.01f, "aim reach %.4f", x);
    }
    CHECK(mf_aim_edge_push(0.4f, 0.65f) == 0.0f, "edge: inside onset");
    CHECK(fabsf(mf_aim_edge_push(-0.65f, 0.65f) + 1.0f) < 1e-6f, "edge: full left");
    CHECK(fabsf(mf_aim_edge_push(0.65f * 0.86f, 0.65f) - 0.5f) < 1e-4f, "edge: half");
    CHECK(fabsf(mf_lowpass_steady_state(0.1f, 0.8812f) - 0.1f / 0.1188f) < 1e-5f, "snap");
}

int main(void)
{
    test_deadzone();
    test_octagon();
    test_scaled_pipeline();
    test_raw_units();
    test_calib();
    test_look();
    test_mouse();
    printf("%d checks, %d failed\n", s_Checks, s_Fail);
    return s_Fail != 0;
}
