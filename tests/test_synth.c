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

static void set_control(bk_shape_params_t *p,int key,float value) {
    switch(key){
    case 0:p->morph=value;break;case 1:p->bulge=value;break;
    case 2:p->pinch=value;break;case 3:p->spikes=value;break;
    case 4:p->tilt=value;break;case 5:p->wobble=value;break;
    }
}
static void control_range(void) {
    for(int key=0;key<6;key++){
        bk_synth_t low,high;bk_synth_init(&low,44100);bk_synth_init(&high,44100);
        bk_shape_params_t p=low.shape,q=high.shape;
        set_control(&p,key,0);set_control(&q,key,1);
        bk_synth_set_shape(&low,&p);bk_synth_set_shape(&high,&q);
        bk_synth_note_on(&low,48,110);bk_synth_note_on(&high,48,110);
        double energy=0,high_energy=0,difference=0,stereo=0;
        float a[256],b[256];
        for(int block=0;block<600;block++){
            bk_synth_render(&low,a,128);bk_synth_render(&high,b,128);
            if(block<50)continue;
            for(int i=0;i<256;i++){
                assert(isfinite(b[i])&&fabsf(b[i])<=1);
                energy+=a[i]*a[i];high_energy+=b[i]*b[i];difference+=(a[i]-b[i])*(a[i]-b[i]);
            }
            for(int i=0;i<128;i++)stereo+=(a[2*i]-a[2*i+1])*(a[2*i]-a[2*i+1]);
        }
        printf("control %d relative difference %.3f\n",key,sqrt(difference/energy));
        assert(difference/energy>0.03); // >17% RMS change for every shape knob.
        assert(stereo>1);
        assert(high_energy/energy>.25 && high_energy/energy<4); // No >6 dB loss/gain across a sweep.
    }
}

int main(void) {
    setbuf(stdout,NULL);
    control_range();
    const float rates[]={32000,44100,48000,96000};
    for(int rate=0;rate<4;rate++)for(int note=0;note<128;note++){
        bk_synth_t stress;bk_synth_init(&stress,rates[rate]);
        bk_shape_params_t extreme={1,1,1,1,1,1};
        bk_synth_set_shape(&stress,&extreme);bk_synth_set_attack_release(&stress,0,0);
        bk_synth_note_on(&stress,note,127);bk_synth_pressure(&stress,note,127);
        assert(render_peak(&stress,4)<=.99f);
    }
    bk_synth_t soft, pressed;
    bk_synth_init(&soft, 44100.0f); bk_synth_init(&pressed, 44100.0f);
    bk_synth_set_attack_release(&soft, 1.0f, 0.2f);
    bk_synth_set_attack_release(&pressed, 1.0f, 0.2f);
    bk_synth_note_on(&soft, 60, 100); bk_synth_note_on(&pressed, 60, 100);
    bk_synth_pressure(&pressed, 60, 127);
    render_peak(&soft, 1); render_peak(&pressed, 1);
    assert(pressed.voices[0].envelope > soft.voices[0].envelope * 1.1f);
    assert(pressed.voices[0].pressure > 0.0f && pressed.voices[0].pressure < 1.0f);
    render_peak(&soft,30);render_peak(&pressed,30);
    double pressure_deformation=0;
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++)
        pressure_deformation+=fabsf(soft.voices[0].contour[soft.voices[0].contour_index][i][c]-
            pressed.voices[0].contour[pressed.voices[0].contour_index][i][c]);
    assert(pressure_deformation>1); // Pressure changes the scanned geometry, not only FM depth.

    /* Stealing carries the old sample into a short decay instead of cutting it. */
    bk_synth_t steal;
    bk_synth_init(&steal, 44100.0f); bk_synth_set_attack_release(&steal, 0, 0.2f);
    for (int n=60;n<60+BK_VOICES;n++) bk_synth_note_on(&steal,n,120);
    float before[2], after[2];
    for (int i=0;i<300;i++) bk_synth_render(&steal,before,1);
    bk_synth_note_on(&steal,72,120); /* one past capacity: must steal */
    assert(fabsf(steal.voices[0].steal_tail) > 1e-6f);
    bk_synth_render(&steal,after,1);
    assert(fabsf(after[0]-before[0]) < 0.7f);

    bk_synth_t s;
    bk_synth_init(&s, 44100.0f);
    const float initial_attack=s.current_attack,initial_release=s.current_release;
    bk_synth_set_attack_release(&s,1,1);
    assert(s.current_attack==initial_attack&&s.current_release==initial_release);
    render_peak(&s,1);
    assert(s.current_attack>initial_attack&&s.current_attack<.9f);
    assert(s.current_release>initial_release&&s.current_release<.9f);
    bk_synth_init(&s,44100);
    assert(bk_synth_active_voices(&s) == 0);

    for (int n = 60; n < 60+BK_VOICES; ++n) bk_synth_note_on(&s, n, 100);
    assert(bk_synth_active_voices(&s) == BK_VOICES);
    for (int n = 60; n < 60+BK_VOICES; ++n) assert(bk_synth_has_note(&s, n));
    bk_synth_note_on(&s, 60+BK_VOICES, 100);
    assert(!bk_synth_has_note(&s, 60) && bk_synth_has_note(&s, 60+BK_VOICES));

    bk_synth_note_off(&s, 61);
    render_peak(&s, 2);
    bk_synth_note_on(&s, 61+BK_VOICES, 100);
    assert(!bk_synth_has_note(&s, 61) && bk_synth_has_note(&s, 62));

    bk_synth_set_attack_release(&s, 0.0f, 0.0f);
    float peak = render_peak(&s, 30);
    assert(peak > 0.005f && peak <= 1.0f);

    bk_synth_pressure(&s, 65, 127);
    assert(bk_synth_note_pressure(&s, 65) > 0.99f);
    bk_synth_pressure(&s, 65, 0);
    assert(bk_synth_note_pressure(&s, 65) == 0.0f);

    /* A shape turn moves its target immediately and settles in milliseconds. */
    float old_morph = s.current.morph;
    bk_shape_params_t sharp = s.shape;
    sharp.morph = sharp.spikes = 1.0f;
    bk_synth_set_shape(&s, &sharp);
    assert(s.current.morph == old_morph && s.shape.morph == 1.0f);
    render_peak(&s, 1);
    assert(s.current.morph > old_morph && s.current.morph < 1.0f);
    render_peak(&s, 8);
    assert(s.current.morph > 0.98f);

    sharp.wobble = 1.0f;
    bk_synth_set_shape(&s, &sharp);
    float wobble_phase = s.wobble_phase;
    render_peak(&s, 8);
    assert(fabsf(s.wobble_phase - wobble_phase) > 1e-6f);

    bk_synth_all_notes_off(&s);
    render_peak(&s, 80);
    assert(bk_synth_active_voices(&s) == 0);
    bk_synth_note_on(&s, 70, 100); bk_synth_kill_all(&s);
    assert(bk_synth_active_voices(&s) == 0);
    puts("PASS: synth engine");
}
