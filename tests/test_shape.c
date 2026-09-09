#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "shape.h"

static float centroid(const float *a) {
    float weighted = 0.0f, total = 0.0f;
    for (int i = 0; i < BK_PARTIALS; ++i) {
        weighted += a[i] * (float)(i + 1);
        total += a[i];
    }
    return weighted / total;
}

int main(void) {
    bk_shape_params_t p;
    float bouba[BK_PARTIALS], kiki[BK_PARTIALS], again[BK_PARTIALS];
    bk_shape_defaults(&p);
    assert(fabsf(p.morph - 0.25f) < 0.0001f);

    p.morph = 0.0f; p.bulge = p.pinch = p.spikes = p.wobble = 0.0f; p.tilt = 0.5f;
    bk_shape_spectrum(&p, 0.0f, bouba);
    p.morph = 1.0f;
    bk_shape_spectrum(&p, 0.0f, kiki);
    bk_shape_spectrum(&p, 0.0f, again);
    assert(centroid(kiki) > centroid(bouba) * 1.45f);
    for (int i = 0; i < BK_PARTIALS; ++i) assert(fabsf(kiki[i] - again[i]) < 1e-7f);

    p.morph = 4.0f; p.bulge = -2.0f; p.pinch = 2.0f;
    p.spikes = 3.0f; p.tilt = -1.0f; p.wobble = 9.0f;
    bk_shape_clamp(&p);
    assert(p.morph == 1.0f && p.bulge == 0.0f && p.pinch == 1.0f);
    assert(p.spikes == 1.0f && p.tilt == 0.0f && p.wobble == 1.0f);
    bk_shape_spectrum(&p, 1.25f, kiki);
    float energy = 0.0f;
    for (int i = 0; i < BK_PARTIALS; ++i) {
        assert(isfinite(kiki[i]) && kiki[i] >= 0.0f && kiki[i] <= 1.0f);
        energy += kiki[i] * kiki[i];
    }
    assert(energy <= 1.0001f && energy > 0.01f);
    puts("PASS: shape model");
}
