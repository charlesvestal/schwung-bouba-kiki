#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "synth.h"

static float render_peak(bk_synth_t *s, int blocks) {
    float out[128 * 2], peak = 0.0f;
    for (int b = 0; b < blocks; ++b) {
        bk_synth_render(s, out, 128);
        for (int i = 0; i < 256; ++i) {
            assert(isfinite(out[i]));
            if (fabsf(out[i]) > peak) peak = fabsf(out[i]);
        }
    }
    return peak;
}

int main(void) {
    bk_synth_t soft, pressed;
    bk_synth_init(&soft, 44100.0f); bk_synth_init(&pressed, 44100.0f);
    bk_synth_set_attack_release(&soft, 1.0f, 0.2f);
    bk_synth_set_attack_release(&pressed, 1.0f, 0.2f);
    bk_synth_note_on(&soft, 60, 100); bk_synth_note_on(&pressed, 60, 100);
    bk_synth_pressure(&pressed, 60, 127);
    render_peak(&soft, 1); render_peak(&pressed, 1);
    assert(pressed.voices[0].envelope > soft.voices[0].envelope * 2.0f);

    bk_synth_t s;
    bk_synth_init(&s, 44100.0f);
    assert(bk_synth_active_voices(&s) == 0);

    for (int n = 60; n < 64; ++n) bk_synth_note_on(&s, n, 100);
    assert(bk_synth_active_voices(&s) == 4);
    for (int n = 60; n < 64; ++n) assert(bk_synth_has_note(&s, n));
    bk_synth_note_on(&s, 64, 100);
    assert(!bk_synth_has_note(&s, 60) && bk_synth_has_note(&s, 64));

    bk_synth_note_off(&s, 61);
    render_peak(&s, 2);
    bk_synth_note_on(&s, 65, 100);
    assert(!bk_synth_has_note(&s, 61) && bk_synth_has_note(&s, 62));

    bk_synth_set_attack_release(&s, 0.0f, 0.0f);
    float peak = render_peak(&s, 30);
    assert(peak > 0.005f && peak <= 1.0f);

    bk_synth_pressure(&s, 65, 127);
    assert(bk_synth_note_pressure(&s, 65) > 0.99f);
    bk_synth_pressure(&s, 65, 0);
    assert(bk_synth_note_pressure(&s, 65) == 0.0f);

    /* A shape turn moves targets immediately but not oscillator gains. */
    float old_amp = s.voices[0].amps[15];
    bk_shape_params_t sharp = s.shape;
    sharp.morph = sharp.spikes = 1.0f;
    bk_synth_set_shape(&s, &sharp);
    assert(fabsf(s.voices[0].amps[15] - old_amp) < 1e-7f);
    assert(fabsf(s.voices[0].target_amps[15] - old_amp) > 1e-5f);
    render_peak(&s, 1);
    assert(fabsf(s.voices[0].amps[15] - old_amp) > 1e-7f);
    assert(fabsf(s.voices[0].amps[15] - s.voices[0].target_amps[15]) > 1e-7f);

    sharp.wobble = 1.0f;
    bk_synth_set_shape(&s, &sharp);
    float wobble_target = s.voices[0].target_amps[9];
    render_peak(&s, 8);
    assert(fabsf(s.voices[0].target_amps[9] - wobble_target) > 1e-6f);

    bk_synth_all_notes_off(&s);
    render_peak(&s, 80);
    assert(bk_synth_active_voices(&s) == 0);
    puts("PASS: synth engine");
}
