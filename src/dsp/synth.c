#include "synth.h"
#include <math.h>
#include <string.h>

#define BK_TAU 6.28318530717958647692f

static float clamp01(float v) {
    if (!isfinite(v) || v < 0.0f) return 0.0f;
    return v > 1.0f ? 1.0f : v;
}

static void prepare_contour(bk_synth_t *s,float pressure,float mod_level,float out[BK_CONTOUR_SIZE][2]) {
    bk_shape_params_t shape=bk_shape_modulate(&s->current,s->mod_depth,mod_level,pressure);
    bk_contour_build(&shape,s->wobble_phase,out);
    float mean[2]={0,0},peak=.1f,energy=0;
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++)mean[c]+=out[i][c]/BK_CONTOUR_SIZE;
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++){
        out[i][c]-=mean[c];peak=fmaxf(peak,fabsf(out[i][c]));energy+=out[i][c]*out[i][c];
    }
    // Preserve body as narrow teeth raise the crest factor; bound extreme peaks.
    const float gain=.55f/fmaxf(sqrtf(energy/(BK_CONTOUR_SIZE*2)),peak/4);
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++)out[i][c]*=gain;
}

void bk_synth_init(bk_synth_t *s, float sample_rate) {
    memset(s, 0, sizeof(*s));
    s->sample_rate = isfinite(sample_rate) && sample_rate >= 32000.0f ? sample_rate : 44100.0f;
    s->attack = 0.05f;
    s->release = 0.25f;
    s->current_attack=s->attack;s->current_release=s->release;
    s->decay=s->current_decay=.25f;s->sustain=s->current_sustain=1;
    s->mod_adsr=s->mod_current=(bk_adsr_t){0,.3f,0,.2f};
    bk_shape_defaults(&s->shape);
    s->current=s->shape;
    s->slew=1.0f-expf(-1.0f/(0.006f*s->sample_rate));
}

void bk_synth_set_shape(bk_synth_t *s, const bk_shape_params_t *shape) {
    s->shape = *shape;
    bk_shape_clamp(&s->shape);
}

void bk_synth_set_attack_release(bk_synth_t *s, float attack, float release) {
    s->attack = clamp01(attack);
    s->release = clamp01(release);
}

void bk_synth_set_envelopes(bk_synth_t *s,const bk_adsr_t *amp,const bk_adsr_t *mod){
    bk_synth_set_attack_release(s,amp->attack,amp->release);
    s->decay=clamp01(amp->decay);s->sustain=clamp01(amp->sustain);
    s->mod_adsr=(bk_adsr_t){clamp01(mod->attack),clamp01(mod->decay),clamp01(mod->sustain),clamp01(mod->release)};
}
void bk_synth_set_modulation(bk_synth_t *s,float amount,int destination){
    s->mod_amount=isfinite(amount)?fmaxf(-1,fminf(1,amount)):0;
    s->mod_destination=destination<0?0:destination>5?5:destination;
}
static bk_adsr_t seconds(bk_adsr_t p){
    return (bk_adsr_t){.002f+2*p.attack*p.attack,.005f+4*p.decay*p.decay,p.sustain,.01f+4*p.release*p.release};
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
    bk_voice_start(choose_voice(s), note, velocity, ++s->age_counter, s->sample_rate);
}

void bk_synth_note_off(bk_synth_t *s, int note) {
    for (int i = 0; i < BK_VOICES; ++i)
        if (s->voices[i].active && s->voices[i].note == note) {
            s->voices[i].held = 0;
            s->voices[i].target_pressure = 0.0f;
        }
}

void bk_synth_pressure(bk_synth_t *s, int note, int value) {
    const float p = clamp01((float)value / 127.0f);
    for (int i = 0; i < BK_VOICES; ++i)
        if (s->voices[i].active && s->voices[i].note == note) s->voices[i].target_pressure = p;
}

void bk_synth_all_notes_off(bk_synth_t *s) {
    for (int i = 0; i < BK_VOICES; ++i) {
        s->voices[i].held = 0;
        s->voices[i].target_pressure = 0.0f;
    }
}

void bk_synth_kill_all(bk_synth_t *s) {
    memset(s->voices, 0, sizeof(s->voices));
}

/* Build one bounded contour per voice/block, then crossfade geometry and scan
 * parameters per sample. No discontinuous phase-modulation index changes. */
void bk_synth_render(bk_synth_t *s, float *out_lr, int frames) {
    if(frames<=0)return;
    const float rate=0.15f+3.85f*s->current.wobble*s->current.wobble;
    const float start=sinf(s->wobble_phase);
    s->wobble_phase=fmodf(s->wobble_phase+BK_TAU*rate*(float)frames/s->sample_rate,BK_TAU);
    const float end=sinf(s->wobble_phase);
    const float block_slew=1-expf(-(float)frames/(.006f*s->sample_rate));
    s->current_attack+=(s->attack-s->current_attack)*block_slew;
    s->current_release+=(s->release-s->current_release)*block_slew;
    s->current_decay+=(s->decay-s->current_decay)*block_slew;
    s->current_sustain+=(s->sustain-s->current_sustain)*block_slew;
#define ENV_SLEW(key) s->mod_current.key+=(s->mod_adsr.key-s->mod_current.key)*block_slew
    ENV_SLEW(attack);ENV_SLEW(decay);ENV_SLEW(sustain);ENV_SLEW(release);
#undef ENV_SLEW
    float previous_depth[6];memcpy(previous_depth,s->mod_depth,sizeof(previous_depth));
    for(int i=0;i<6;i++)s->mod_depth[i]+=((i==s->mod_destination?s->mod_amount:0)-s->mod_depth[i])*block_slew;
    const bk_adsr_t amp=seconds((bk_adsr_t){s->current_attack,s->current_decay,s->current_sustain,s->current_release});
    const bk_adsr_t mod=seconds(s->mod_current);
    const bk_shape_params_t before=s->current;
#define SLEW(key) s->current.key+=(s->shape.key-s->current.key)*block_slew
        SLEW(morph); SLEW(bulge); SLEW(pinch); SLEW(spikes); SLEW(tilt); SLEW(wobble);
#undef SLEW
    for(int i=0;i<BK_VOICES;i++){
        bk_voice_t *v=&s->voices[i];if(!v->active)continue;
        v->contour_index=1-v->contour_index;
        bk_envelope_t predicted=v->mod_env;bk_envelope_gate(&predicted,v->held||v->min_gate>0);
        for(int f=0;f<frames;f++)bk_envelope_tick(&predicted,&mod,s->sample_rate);
        prepare_contour(s,v->pressure,predicted.level,v->contour[v->contour_index]);
        if(!v->contour_ready){
            memcpy(v->contour[1-v->contour_index],v->contour[v->contour_index],sizeof(v->contour[0]));
            v->contour_ready=1;
        }
    }
    for(int f=0;f<frames;f++){
        const float blend=(f+1.0f)/frames;
        float depth[6];for(int i=0;i<6;i++)depth[i]=previous_depth[i]+(s->mod_depth[i]-previous_depth[i])*blend;
        bk_shape_params_t tone;
#define INTERPOLATE(key) tone.key=before.key+(s->current.key-before.key)*blend
        INTERPOLATE(morph);INTERPOLATE(bulge);INTERPOLATE(pinch);
        INTERPOLATE(spikes);INTERPOLATE(tilt);INTERPOLATE(wobble);
#undef INTERPOLATE
        const float motion=(start+(end-start)*(float)f/(float)frames)*s->current.wobble;
        float l=0,r=0;
        for(int v=0;v<BK_VOICES;v++){
            bk_voice_t *voice=&s->voices[v];
            float vl,vr; bk_voice_render(voice,&tone,&amp,&mod,motion,depth,
                voice->contour[1-voice->contour_index],voice->contour[voice->contour_index],blend,&vl,&vr);
            l+=vl;r+=vr;
        }
        // Smooth rational saturation protects chords without shrinking quiet notes.
        out_lr[2*f]=l/(1.0f+fabsf(l)*0.65f);
        out_lr[2*f+1]=r/(1.0f+fabsf(r)*0.65f);
        out_lr[2*f]=fmaxf(-0.99f,fminf(0.99f,out_lr[2*f]));
        out_lr[2*f+1]=fmaxf(-0.99f,fminf(0.99f,out_lr[2*f+1]));
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
        if (s->voices[i].active && s->voices[i].note == note) return s->voices[i].target_pressure;
    return 0.0f;
}
