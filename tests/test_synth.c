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

    bk_synth_all_notes_off(&s);
    render_peak(&s, 80);
    assert(bk_synth_active_voices(&s) == 0);
    puts("PASS: synth engine");
}
