#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "synth.h"
static void blocks(bk_synth_t *s,int n){float out[256];for(int i=0;i<n;i++){bk_synth_render(s,out,128);for(int j=0;j<256;j++)assert(isfinite(out[j]));}}
int main(void){
    bk_synth_t s;bk_synth_init(&s,44100);
    assert(s.shape.morph==0&&s.shape.bulge==0&&s.shape.pinch==0&&s.shape.spikes==0&&s.shape.tilt==.5f&&s.shape.wobble==0);
    bk_adsr_t amp={0,.1f,.4f,.1f},mod={0,.2f,0,.1f};
    bk_synth_set_envelopes(&s,&amp,&mod);bk_synth_set_modulation(&s,1,0);
    blocks(&s,20);bk_synth_note_on(&s,60,100);blocks(&s,5);
    assert(s.voices[0].mod_env.level>.7f);
    blocks(&s,100);
    assert(s.voices[0].mod_env.level==0);
    assert(fabsf(s.voices[0].envelope-.4f)<.01f);
    assert(s.shape.morph==0);
    bk_synth_pressure(&s,60,127);blocks(&s,10);
    bk_shape_params_t p=bk_shape_modulate(&s.current,s.mod_depth,s.voices[0].mod_env.level,s.voices[0].pressure);
    assert(p.morph>.29f);
    bk_synth_set_modulation(&s,-1,0);blocks(&s,20);
    s.current.morph=1;p=bk_shape_modulate(&s.current,s.mod_depth,1,0);
    assert(p.morph<.01f);
    bk_synth_note_off(&s,60);blocks(&s,100);assert(!s.voices[0].active);
    // A note released before the first render must still sound: the voice holds
    // its gate open for a minimum time instead of being killed silently.
    bk_synth_note_on(&s,60,110);bk_synth_note_off(&s,60);
    float out[256],peak=0;
    for(int i=0;i<200;i++){bk_synth_render(&s,out,128);for(int j=0;j<256;j++){assert(isfinite(out[j]));if(fabsf(out[j])>peak)peak=fabsf(out[j]);}}
    assert(peak>.001f);
    assert(!s.voices[0].active);
    puts("PASS: pure Bouba default, dual ADSRs, sustain, pressure and bipolar modulation");
}
