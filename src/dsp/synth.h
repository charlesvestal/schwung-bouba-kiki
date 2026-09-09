#ifndef BOUBA_KIKI_SYNTH_H
#define BOUBA_KIKI_SYNTH_H

#include <stdint.h>
#include "voice.h"

#define BK_VOICES 4

typedef struct bk_synth {
    float sample_rate;
    float attack;
    float release;
    uint64_t age_counter;
    bk_shape_params_t shape;
    float spectrum[BK_PARTIALS];
    bk_voice_t voices[BK_VOICES];
} bk_synth_t;

void bk_synth_init(bk_synth_t *s, float sample_rate);
void bk_synth_set_shape(bk_synth_t *s, const bk_shape_params_t *shape);
void bk_synth_set_attack_release(bk_synth_t *s, float attack, float release);
void bk_synth_note_on(bk_synth_t *s, int note, int velocity);
void bk_synth_note_off(bk_synth_t *s, int note);
void bk_synth_pressure(bk_synth_t *s, int note, int value);
void bk_synth_all_notes_off(bk_synth_t *s);
void bk_synth_render(bk_synth_t *s, float *out_lr, int frames);
int bk_synth_active_voices(const bk_synth_t *s);
int bk_synth_has_note(const bk_synth_t *s, int note);
float bk_synth_note_pressure(const bk_synth_t *s, int note);

#endif
