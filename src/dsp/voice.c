#include "voice.h"
#include <math.h>
#include <string.h>

#define BK_PI 3.14159265358979323846f

void bk_voice_start(bk_voice_t *v, int note, int velocity, uint64_t age,
                    float sample_rate, const float spectrum[BK_PARTIALS]) {
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->held = 1;
    v->note = note;
    v->velocity = (float)velocity / 127.0f;
    v->age = age;
    const float hz = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
    for (int i = 0; i < BK_PARTIALS; ++i) {
        const float partial_hz = hz * (float)(i + 1);
        v->osc_cos[i] = 1.0f;
        v->amps[i] = spectrum[i];
        if (partial_hz < sample_rate * 0.48f) {
            const float step = 2.0f * BK_PI * partial_hz / sample_rate;
            v->step_sin[i] = sinf(step);
            v->step_cos[i] = cosf(step);
        }
    }
}

float bk_voice_render(bk_voice_t *v, float attack_s, float release_s) {
    if (!v->active) return 0.0f;
    if (v->held) {
        const float inc = 1.0f / (44100.0f * attack_s);
        v->envelope += inc;
        if (v->envelope > 1.0f) v->envelope = 1.0f;
    } else {
        v->envelope -= 1.0f / (44100.0f * release_s);
        if (v->envelope <= 0.0f) {
            v->envelope = 0.0f;
            v->active = 0;
            return 0.0f;
        }
    }

    float sample = 0.0f;
    for (int i = 0; i < BK_PARTIALS; ++i) {
        if (v->step_sin[i] == 0.0f) continue;
        const float s = v->osc_sin[i], c = v->osc_cos[i];
        v->osc_sin[i] = s * v->step_cos[i] + c * v->step_sin[i];
        v->osc_cos[i] = c * v->step_cos[i] - s * v->step_sin[i];
        const float bite = 1.0f + v->pressure * 1.5f * ((float)i / (float)(BK_PARTIALS - 1));
        sample += v->osc_sin[i] * v->amps[i] * bite;
    }
    return sample * v->velocity * v->envelope * 0.16f;
}
