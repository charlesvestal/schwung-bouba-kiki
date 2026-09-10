#ifndef BOUBA_KIKI_VOICE_H
#define BOUBA_KIKI_VOICE_H
#include <stdint.h>
#include "shape.h"
#include "envelope.h"

typedef struct bk_voice {
    int active, held, note;
    float velocity, pressure, target_pressure, envelope;
    bk_envelope_t amp_env,mod_env;
    float phase_a, phase_b, increment;
    float sample_rate, slew;
    float filter_b[3][3],filter_a[3][2],filter_z[2][3][2];
    float dc_in[2],dc_out[2],feedback;
    float contour[2][BK_CONTOUR_SIZE][2];
    int contour_index,contour_ready,min_gate;
    float phase_c,wobble_phase;
    int ratio_step;
    float steal_tail, last_output;
    uint64_t age;
} bk_voice_t;

void bk_voice_start(bk_voice_t *v, int note, int velocity, uint64_t age,
                    float sample_rate);
void bk_voice_render(bk_voice_t *v, const bk_shape_params_t *shape,
                     const bk_adsr_t *amp,const bk_adsr_t *mod,float motion,
                     const float mod_depth[6],
                     const float previous[BK_CONTOUR_SIZE][2],
                     const float next[BK_CONTOUR_SIZE][2],float blend,
                     float *left, float *right);
#endif
