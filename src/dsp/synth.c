#include "synth.h"
#include <math.h>
#include <string.h>

static float clamp01(float v) {
    if (!isfinite(v) || v < 0.0f) return 0.0f;
    return v > 1.0f ? 1.0f : v;
}

void bk_synth_init(bk_synth_t *s, float sample_rate) {
    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate > 1000.0f ? sample_rate : 44100.0f;
    s->attack = 0.05f;
    s->release = 0.25f;
    bk_shape_defaults(&s->shape);
    bk_shape_spectrum(&s->shape, 0.0f, s->spectrum);
}

void bk_synth_set_shape(bk_synth_t *s, const bk_shape_params_t *shape) {
    s->shape = *shape;
    bk_shape_clamp(&s->shape);
    bk_shape_spectrum(&s->shape, 0.0f, s->spectrum);
    for (int v = 0; v < BK_VOICES; ++v)
        for (int i = 0; i < BK_PARTIALS; ++i)
            s->voices[v].amps[i] = s->spectrum[i];
}

void bk_synth_set_attack_release(bk_synth_t *s, float attack, float release) {
    s->attack = clamp01(attack);
    s->release = clamp01(release);
}

static bk_voice_t *choose_voice(bk_synth_t *s) {
    bk_voice_t *oldest = &s->voices[0];
    bk_voice_t *release = NULL;
    for (int i = 0; i < BK_VOICES; ++i) {
        bk_voice_t *v = &s->voices[i];
        if (!v->active) return v;
        if (!v->held && (!release || v->envelope < release->envelope)) release = v;
        if (v->age < oldest->age) oldest = v;
    }
    return release ? release : oldest;
}

void bk_synth_note_on(bk_synth_t *s, int note, int velocity) {
    if (note < 0 || note > 127 || velocity <= 0) {
        if (velocity <= 0) bk_synth_note_off(s, note);
        return;
    }
    if (velocity > 127) velocity = 127;
    bk_voice_start(choose_voice(s), note, velocity, ++s->age_counter,
                   s->sample_rate, s->spectrum);
}

void bk_synth_note_off(bk_synth_t *s, int note) {
    for (int i = 0; i < BK_VOICES; ++i)
        if (s->voices[i].active && s->voices[i].note == note) s->voices[i].held = 0;
}

void bk_synth_pressure(bk_synth_t *s, int note, int value) {
    const float p = clamp01((float)value / 127.0f);
    for (int i = 0; i < BK_VOICES; ++i)
        if (s->voices[i].active && s->voices[i].note == note) s->voices[i].pressure = p;
}

void bk_synth_all_notes_off(bk_synth_t *s) {
    for (int i = 0; i < BK_VOICES; ++i) s->voices[i].held = 0;
}

void bk_synth_render(bk_synth_t *s, float *out_lr, int frames) {
    const float attack_s = 0.002f + s->attack * s->attack * 2.0f;
    const float release_s = 0.01f + s->release * s->release * 4.0f;
    for (int f = 0; f < frames; ++f) {
        float x = 0.0f;
        for (int v = 0; v < BK_VOICES; ++v) x += bk_voice_render(&s->voices[v], attack_s, release_s);
        x = tanhf(x * 0.7f);
        out_lr[f * 2] = x;
        out_lr[f * 2 + 1] = x;
    }
}

int bk_synth_active_voices(const bk_synth_t *s) {
    int n = 0;
    for (int i = 0; i < BK_VOICES; ++i) n += s->voices[i].active != 0;
    return n;
}

int bk_synth_has_note(const bk_synth_t *s, int note) {
    for (int i = 0; i < BK_VOICES; ++i)
        if (s->voices[i].active && s->voices[i].note == note) return 1;
    return 0;
}

float bk_synth_note_pressure(const bk_synth_t *s, int note) {
    for (int i = 0; i < BK_VOICES; ++i)
        if (s->voices[i].active && s->voices[i].note == note) return s->voices[i].pressure;
    return 0.0f;
}
