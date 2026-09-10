#include "synth.h"
#include <math.h>
#include <string.h>

#define BK_TAU 6.28318530717958647692f

static float clamp01(float v) {
    if (!isfinite(v) || v < 0.0f) return 0.0f;
    return v > 1.0f ? 1.0f : v;
}

/* Band-limit the contour for the pitch that will scan it. The contour is a
 * wavetable, and narrow teeth are high harmonics of it: at 2 kHz the 100th
 * harmonic of a 256-point table lands at 200 kHz and folds back as grit. A
 * circular box smoother, applied twice, is the cheap mip-map -- it rounds the
 * teeth off as the note rises, which is also what the eye expects of a small
 * shape. Below roughly C4 the width is 1 and this is a no-op. */
static void band_limit(float c[BK_CONTOUR_SIZE][2],float hz) {
    /* Only above the pitch where folding actually starts. Below this the teeth
       are worth more than the handful of stray partials they produce, and the
       index key-track is likewise inactive. */
    if(!(hz>420.0f))return;
    const int keep=(int)(15000.0f/hz);              /* harmonics under ~15 kHz */
    if(keep>=BK_CONTOUR_SIZE/2)return;
    int width=BK_CONTOUR_SIZE/(keep>0?keep:1);
    if(width<2)return;
    if(width>BK_CONTOUR_SIZE/4)width=BK_CONTOUR_SIZE/4;
    float tmp[BK_CONTOUR_SIZE][2];
    const int half=width/2;
    const float inv=1.0f/(float)width;
    for(int pass=0;pass<2;pass++){
        /* Sliding sum: each output costs one add and one subtract regardless of
           width. The naive form was O(width) per point, which at the top of the
           keyboard is where the widths are widest and the voice count hurts. */
        float sx=0,sy=0;
        for(int k=0;k<width;k++){
            const int j=(k-half)&(BK_CONTOUR_SIZE-1);
            sx+=c[j][0];sy+=c[j][1];
        }
        for(int i=0;i<BK_CONTOUR_SIZE;i++){
            tmp[i][0]=sx*inv;tmp[i][1]=sy*inv;
            const int drop=(i-half)&(BK_CONTOUR_SIZE-1);
            const int add=(i-half+width)&(BK_CONTOUR_SIZE-1);
            sx+=c[add][0]-c[drop][0];sy+=c[add][1]-c[drop][1];
        }
        memcpy(c,tmp,sizeof(tmp));
    }
}
static void prepare_contour(const bk_shape_params_t *shape,float hz,float phase,float out[BK_CONTOUR_SIZE][2]) {
    bk_contour_build(shape,phase,out);
    float mean[2]={0,0},peak=.1f,energy=0;
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++)mean[c]+=out[i][c]/BK_CONTOUR_SIZE;
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++){
        out[i][c]-=mean[c];peak=fmaxf(peak,fabsf(out[i][c]));energy+=out[i][c]*out[i][c];
    }
    // Preserve body as narrow teeth raise the crest factor; bound extreme peaks.
    const float gain=.55f/fmaxf(sqrtf(energy/(BK_CONTOUR_SIZE*2)),peak/4);
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++)out[i][c]*=gain;
    /* After normalising, so smoothing genuinely removes energy instead of
       being handed back by the gain. */
    band_limit(out,hz);
}

void bk_synth_init(bk_synth_t *s, float sample_rate) {
    memset(s, 0, sizeof(*s));
    s->sample_rate = isfinite(sample_rate) && sample_rate >= 32000.0f ? sample_rate : 44100.0f;
    s->attack = 0.05f;
    s->release = 0.25f;
    s->current_attack=s->attack;s->current_release=s->release;
    s->decay=s->current_decay=.25f;s->sustain=s->current_sustain=1;
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

void bk_synth_set_envelopes(bk_synth_t *s,const bk_adsr_t *amp){
    bk_synth_set_attack_release(s,amp->attack,amp->release);
    s->decay=clamp01(amp->decay);s->sustain=clamp01(amp->sustain);
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
    const float advance=BK_TAU*rate*(float)frames/s->sample_rate;
    /* Kept for the display, which draws one shape for the newest note. */
    s->wobble_phase=fmodf(s->wobble_phase+advance,BK_TAU);
    const float block_slew=1-expf(-(float)frames/(.006f*s->sample_rate));
    s->current_attack+=(s->attack-s->current_attack)*block_slew;
    s->current_release+=(s->release-s->current_release)*block_slew;
    s->current_decay+=(s->decay-s->current_decay)*block_slew;
    s->current_sustain+=(s->sustain-s->current_sustain)*block_slew;
    const bk_adsr_t amp=seconds((bk_adsr_t){s->current_attack,s->current_decay,s->current_sustain,s->current_release});
    const bk_shape_params_t before=s->current;
#define SLEW(key) s->current.key+=(s->shape.key-s->current.key)*block_slew
        SLEW(morph); SLEW(bulge); SLEW(pinch); SLEW(spikes); SLEW(tilt); SLEW(wobble);
#undef SLEW
    float motion_start[BK_VOICES]={0},motion_end[BK_VOICES]={0};
    for(int i=0;i<BK_VOICES;i++){
        bk_voice_t *v=&s->voices[i];if(!v->active)continue;
        v->contour_index=1-v->contour_index;
        /* Each voice carries its own ripple phase, so a held chord moves
           internally instead of every note breathing in lockstep. */
        motion_start[i]=sinf(v->wobble_phase)*s->current.wobble;
        v->wobble_phase=fmodf(v->wobble_phase+advance,BK_TAU);
        motion_end[i]=sinf(v->wobble_phase)*s->current.wobble;
        const bk_shape_params_t shape=bk_shape_modulate(&s->current,v->pressure);
        const float hz=v->increment*s->sample_rate;
        prepare_contour(&shape,hz,v->wobble_phase,v->contour[v->contour_index]);
        /* The modulator scans its own contour, sharpened relative to the
           carrier's. In phase modulation the modulator's own harmonics multiply
           out into sidebands, so a spikier modulator is a brighter result --
           this is what gives Morph and pressure real range while they stay at a
           1:1 ratio and therefore stay in tune.
           The sharpening retreats where Pinch and Spikes open, and squared so
           it retreats quickly: those two earn their character from an
           irrational ratio, and flooding the spectrum with harmonic sidebands
           would dilute exactly what makes them worth having. At either
           extreme the modulator is the carrier's own contour again. */
        bk_shape_params_t sharpened=shape;
        const float clang=fmaxf(shape.pinch,shape.spikes),open_room=(1-clang)*(1-clang);
        sharpened.spikes=fminf(1,shape.spikes+.55f*open_room);
        prepare_contour(&sharpened,hz,v->wobble_phase,v->mod_contour[v->contour_index]);
        if(!v->contour_ready){
            memcpy(v->contour[1-v->contour_index],v->contour[v->contour_index],sizeof(v->contour[0]));
            memcpy(v->mod_contour[1-v->contour_index],v->mod_contour[v->contour_index],sizeof(v->mod_contour[0]));
            v->contour_ready=1;
        }
    }
    for(int f=0;f<frames;f++){
        const float blend=(f+1.0f)/frames;
        bk_shape_params_t tone;
#define INTERPOLATE(key) tone.key=before.key+(s->current.key-before.key)*blend
        INTERPOLATE(morph);INTERPOLATE(bulge);INTERPOLATE(pinch);
        INTERPOLATE(spikes);INTERPOLATE(tilt);INTERPOLATE(wobble);
#undef INTERPOLATE
        float l=0,r=0;
        for(int v=0;v<BK_VOICES;v++){
            bk_voice_t *voice=&s->voices[v];
            const float motion=motion_start[v]+(motion_end[v]-motion_start[v])*(float)f/(float)frames;
            float vl,vr; bk_voice_render(voice,&tone,&amp,motion,
                voice->contour[1-voice->contour_index],voice->contour[voice->contour_index],
                voice->mod_contour[1-voice->contour_index],voice->mod_contour[voice->contour_index],
                blend,&vl,&vr);
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
