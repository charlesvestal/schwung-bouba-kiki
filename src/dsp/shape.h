#ifndef BOUBA_KIKI_SHAPE_H
#define BOUBA_KIKI_SHAPE_H

#define BK_PARTIALS 32

typedef struct bk_shape_params {
    float morph;
    float bulge;
    float pinch;
    float spikes;
    float tilt;
    float wobble;
} bk_shape_params_t;

void bk_shape_defaults(bk_shape_params_t *p);
void bk_shape_clamp(bk_shape_params_t *p);
void bk_shape_spectrum(const bk_shape_params_t *p, float phase, float out[BK_PARTIALS]);

#endif
