#ifndef BOUBA_KIKI_SHAPE_H
#define BOUBA_KIKI_SHAPE_H


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
bk_shape_params_t bk_shape_modulate(const bk_shape_params_t *base,float pressure);
#define BK_CONTOUR_SIZE 256
void bk_contour_build(const bk_shape_params_t *p,float phase,float out[BK_CONTOUR_SIZE][2]);

#endif
