#include "shape.h"
#include <math.h>

static float clamp01(float v) {
    if (!isfinite(v)) return 0.0f;
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

void bk_shape_defaults(bk_shape_params_t *p) {
    p->morph = 0.25f;
    p->bulge = 0.35f;
    p->pinch = 0.0f;
    p->spikes = 0.25f;
    p->tilt = 0.5f;
    p->wobble = 0.1f;
}

void bk_shape_clamp(bk_shape_params_t *p) {
    p->morph = clamp01(p->morph);
    p->bulge = clamp01(p->bulge);
    p->pinch = clamp01(p->pinch);
    p->spikes = clamp01(p->spikes);
    p->tilt = clamp01(p->tilt);
    p->wobble = clamp01(p->wobble);
}

void bk_shape_spectrum(const bk_shape_params_t *input, float phase,
                       float out[BK_PARTIALS]) {
    bk_shape_params_t p = *input;
    bk_shape_clamp(&p);
    const float asym = (p.tilt - 0.5f) * 0.7f;
    float energy = 0.0f;

    for (int i = 0; i < BK_PARTIALS; ++i) {
        const float n = (float)(i + 1);
        const float bouba = expf(-0.31f * (n - 1.0f));
        const float kiki = powf(n, -0.72f) * (1.0f + 0.16f * sinf(n * 2.37f));
        float a = bouba + (kiki - bouba) * p.morph;

        a *= 1.0f + p.bulge * 0.45f * expf(-0.045f * (n - 3.0f) * (n - 3.0f));
        a *= 1.0f - p.pinch * 0.72f * expf(-0.16f * (n - 7.0f) * (n - 7.0f));
        a *= 1.0f + p.spikes * p.morph * 0.9f * (n / (float)BK_PARTIALS);
        a *= 1.0f + asym * ((i & 1) ? -1.0f : 1.0f);
        a *= 1.0f + p.wobble * 0.18f * sinf(phase + n * 0.63f);
        if (a < 0.0f) a = 0.0f;
        out[i] = a;
        energy += a * a;
    }

    if (energy > 1.0f) {
        const float gain = 1.0f / sqrtf(energy);
        for (int i = 0; i < BK_PARTIALS; ++i) out[i] *= gain;
    }
}
