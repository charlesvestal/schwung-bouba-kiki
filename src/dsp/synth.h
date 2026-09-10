#ifndef BOUBA_KIKI_SYNTH_H
#define BOUBA_KIKI_SYNTH_H

#include <stdint.h>
#include "voice.h"

#define BK_VOICES 6

typedef struct bk_synth {
    float sample_rate;
    float attack;
    float release;
    float current_attack,current_release;
    float decay,sustain,current_decay,current_sustain;
    bk_adsr_t mod_adsr,mod_current;
    float mod_amount,mod_depth[6];
    int mod_destination;
    float wobble_phase;
    uint64_t age_counter;
    bk_shape_params_t shape;
    bk_shape_params_t current;
    float slew;
    bk_voice_t voices[BK_VOICES];
} bk_synth_t;

void bk_synth_init(bk_synth_t *s, float sample_rate);
void bk_synth_set_shape(bk_synth_t *s, const bk_shape_params_t *shape);
void bk_synth_set_attack_release(bk_synth_t *s, float attack, float release);
void bk_synth_set_envelopes(bk_synth_t *s,const bk_adsr_t *amp,const bk_adsr_t *mod);
void bk_synth_set_modulation(bk_synth_t *s,float amount,int destination);
void bk_synth_note_on(bk_synth_t *s, int note, int velocity);
void bk_synth_note_off(bk_synth_t *s, int note);
void bk_synth_pressure(bk_synth_t *s, int note, int value);
void bk_synth_all_notes_off(bk_synth_t *s);
void bk_synth_kill_all(bk_synth_t *s);
void bk_synth_render(bk_synth_t *s, float *out_lr, int frames);
int bk_synth_active_voices(const bk_synth_t *s);
int bk_synth_has_note(const bk_synth_t *s, int note);
float bk_synth_note_pressure(const bk_synth_t *s, int note);

#endif
