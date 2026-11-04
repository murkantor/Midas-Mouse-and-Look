/* mf_stick.c - see include/midas_feel/mf_stick.h. Source: Project Midas
 * port/src/input_pc.c (inputAxisScaleDz, inputCircleToOctagon,
 * portInputStickGate, inputRawUnits, inputCalibApply, inputEntryLevel). */
#include "midas_feel/mf_stick.h"

#include <math.h>

const int16_t mf_n64_ideal_octagon[8][2] =
{
    { 85, 0 }, { 65, 65 }, { 0, 85 }, { -65, 65 },
    { -85, 0 }, { -65, -65 }, { 0, -85 }, { 65, -65 },
};

static int32_t mf_abs32(int32_t v)
{
    return (v < 0) ? -v : v;
}

int32_t mf_axis_deadzone(int32_t x, int32_t dz, float scale)
{
    if (dz < 0)
    {
        dz = 0;
    }
    if (mf_abs32(x) < dz || dz >= 32768)   /* never divide by zero */
    {
        return 0;
    }
    if (x < 0)
    {
        x += dz;
    }
    else
    {
        x -= dz;
    }
    x = (int32_t) ((int64_t) x * 32768 / (32768 - dz));
    x = (int32_t) ((float) x * scale);
    return (x > 32767) ? 32767 : ((x < -32768) ? -32768 : x);
}

void mf_circle_to_octagon(int32_t *x, int32_t *y, int32_t cardinal, int32_t diag_sum)
{
    const double ax = (*x < 0) ? -(double) *x : (double) *x;
    const double ay = (*y < 0) ? -(double) *y : (double) *y;
    const double r = sqrt(ax * ax + ay * ay);
    double edge;
    double f;

    if (r < 1.0 || cardinal <= 0)
    {
        return;
    }
    edge = (double) diag_sum * r / (ax + ay);                  /* the octagon's diagonal faces */
    f = (double) cardinal * r / ((ax > ay) ? ax : ay);         /* ...and its cardinal faces */
    if (f < edge)
    {
        edge = f;
    }
    f = edge / (double) cardinal;                              /* 1.0 on a cardinal, ~1.08 at 45 deg (N64) */
    *x = (int32_t) ((double) *x * f + ((*x < 0) ? -0.5 : 0.5));
    *y = (int32_t) ((double) *y * f + ((*y < 0) ? -0.5 : 0.5));
}

void mf_octagon_gate(int32_t *px, int32_t *py, int32_t cardinal, int32_t diag_sum)
{
    int32_t x = *px;
    int32_t y = *py;
    int32_t ax = mf_abs32(x);
    int32_t ay = mf_abs32(y);
    int32_t sum;

    if (ax > cardinal)
    {
        x = (x < 0) ? -cardinal : cardinal;
        ax = cardinal;
    }
    if (ay > cardinal)
    {
        y = (y < 0) ? -cardinal : cardinal;
        ay = cardinal;
    }
    sum = ax + ay;
    if (diag_sum > 0 && sum > diag_sum)
    {
        x = x * diag_sum / sum;
        y = y * diag_sum / sum;
    }
    *px = x;
    *py = y;
}

void mf_stick_scaled_n64(int32_t raw_x, int32_t raw_y_up, int32_t dz, float scale,
                         int8_t *out_x, int8_t *out_y)
{
    int32_t x = mf_axis_deadzone(raw_x, dz, scale) * MF_N64_CARDINAL / 32768;
    int32_t y = mf_axis_deadzone(raw_y_up, dz, scale) * MF_N64_CARDINAL / 32768;

    mf_circle_to_octagon(&x, &y, MF_N64_CARDINAL, MF_N64_DIAG_SUM);
    mf_octagon_gate(&x, &y, MF_N64_CARDINAL, MF_N64_DIAG_SUM);
    if (out_x != 0) *out_x = (int8_t) x;
    if (out_y != 0) *out_y = (int8_t) y;
}

int8_t mf_stick_raw_units(int32_t axis)
{
    int32_t v = axis / 256;

    if (v > 127) v = 127;
    if (v < -127) v = -127;
    return (int8_t) v;
}

int mf_calib_apply(const mf_calib *c, int32_t rx, int32_t ry, int32_t radial_dz,
                   int8_t *out_x, int8_t *out_y)
{
    int i;

    if (out_x != 0) *out_x = 0;
    if (out_y != 0) *out_y = 0;
    if (c == 0)
    {
        return 0;
    }
    if ((float) rx * rx + (float) ry * ry < (float) radial_dz * radial_dz)
    {
        return 0;   /* radial deadzone */
    }
    for (i = 0; i < 8; i++)
    {
        const float c1x = c->pt[i][0];
        const float c1y = c->pt[i][1];
        const float c2x = c->pt[(i + 1) & 7][0];
        const float c2y = c->pt[(i + 1) & 7][1];
        const float det = c1x * c2y - c1y * c2x;
        float a, b, s;

        if (det > -1.0f && det < 1.0f)
        {
            continue;   /* degenerate sector (bad measurement) */
        }
        a = ((float) rx * c2y - (float) ry * c2x) / det;
        b = ((float) ry * c1x - (float) rx * c1y) / det;
        if (a < 0.0f || b < 0.0f)
        {
            continue;   /* not this sector */
        }
        s = a + b;
        if (s > 1.0f)
        {
            a /= s;     /* past the measured edge: clamp onto it */
            b /= s;
        }
        if (out_x != 0) *out_x = (int8_t) (a * mf_n64_ideal_octagon[i][0] + b * mf_n64_ideal_octagon[(i + 1) & 7][0]);
        if (out_y != 0) *out_y = (int8_t) (a * mf_n64_ideal_octagon[i][1] + b * mf_n64_ideal_octagon[(i + 1) & 7][1]);
        return 1;
    }
    return 0;
}

float mf_trigger_level(int32_t raw, int32_t threshold)
{
    float l;

    if (threshold < 0) threshold = 0;
    if (threshold >= 32767 || raw <= threshold)
    {
        return 0.0f;
    }
    l = (float) (raw - threshold) / (float) (32767 - threshold);
    return (l > 1.0f) ? 1.0f : l;
}
