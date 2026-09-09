#ifndef BOUBA_KIKI_VOICE_H
#define BOUBA_KIKI_VOICE_H

#include <stdint.h>
#include "shape.h"

typedef struct bk_voice {
    int active;
    int held;
    int note;
    float velocity;
    float pressure;
    float envelope;
    uint64_t age;
    float osc_sin[BK_PARTIALS];
    float osc_cos[BK_PARTIALS];
    float step_sin[BK_PARTIALS];
    float step_cos[BK_PARTIALS];
    float amps[BK_PARTIALS];
    float target_amps[BK_PARTIALS];
} bk_voice_t;

void bk_voice_start(bk_voice_t *v, int note, int velocity, uint64_t age,
                    float sample_rate, const float spectrum[BK_PARTIALS]);
float bk_voice_render(bk_voice_t *v, float attack_s, float release_s);

#endif
