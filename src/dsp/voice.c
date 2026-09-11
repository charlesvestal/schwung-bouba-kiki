#include "voice.h"
#include <math.h>
#include <string.h>

static float wrap(float p){return p-floorf(p);}
static float scan(const float a[BK_CONTOUR_SIZE][2],const float b[BK_CONTOUR_SIZE][2],
                  float phase,int channel,float blend){
    // wrap() can return exactly 1 for a tiny negative phase, so mask i as well as j.
    float pos=wrap(phase)*BK_CONTOUR_SIZE;int cell=(int)pos;
    int i=cell&(BK_CONTOUR_SIZE-1),j=(i+1)&(BK_CONTOUR_SIZE-1);
    float t=pos-cell,x=a[i][channel]+t*(a[j][channel]-a[i][channel]);
    float y=b[i][channel]+t*(b[j][channel]-b[i][channel]);
    return x+(y-x)*blend;
}
void bk_voice_start(bk_voice_t *v,int note,int velocity,uint64_t age,float sample_rate){
    float tail[2]={v->active?v->last_output[0]:0,v->active?v->last_output[1]:0};
    memset(v,0,sizeof(*v));v->active=v->held=1;v->note=note;v->age=age;
    v->sample_rate=sample_rate;v->slew=1-expf(-1/(.004f*sample_rate));
    // Longer than the pressure slew: the tail has to outlast the new note's
    // minimum 2 ms attack so the two overlap instead of meeting at a gap.
    v->steal_slew=1-expf(-1/(.008f*sample_rate));
    // Curve the velocity rather than using it raw. Straight through, a pad hit
    // at 10 landed 31 dB down and a hit at 1 more than 50 dB down -- quiet and,
    // because velocity also drives the index, dull with it, so soft hits simply
    // vanished. The exponent lifts the bottom of the range into audibility
    // while leaving the top untouched.
    v->velocity=powf(fmaxf(1.0f,(float)velocity)/127.0f,.55f);
    v->increment=440*powf(2,(note-69)/12.0f)/sample_rate;
    // Hold the gate open briefly so a note released inside the same block still sounds.
    v->min_gate=(int)(.004f*sample_rate);
    // Stagger each voice's ripple so a chord breathes instead of pulsing as one.
    // Golden-ratio spacing, because consecutive notes take consecutive ages and
    // any small integer step would leave them bunched within a few degrees.
    {   const float turns=(float)age*0.61803399f;
        v->wobble_phase=(turns-floorf(turns))*6.28318530718f; }
    // Per channel: the outgoing note is a stereo signal, and collapsing it to
    // one decaying mono value snaps the image to centre as it fades.
    v->steal_tail[0]=tail[0];v->steal_tail[1]=tail[1];
    // Six-pole lowpass before 4x decimation. No detuned VA oscillators.
    const float q[3]={.51763809f,.70710678f,1.93185165f};
    const float k=tanf(3.14159265359f*fminf(13000,.30f*sample_rate)/(4*sample_rate));
    for(int n=0;n<3;n++){
        float norm=1/(1+k/q[n]+k*k);
        v->filter_b[n][0]=k*k*norm;v->filter_b[n][1]=2*k*k*norm;v->filter_b[n][2]=k*k*norm;
        v->filter_a[n][0]=2*(k*k-1)*norm;v->filter_a[n][1]=(1-k/q[n]+k*k)*norm;
    }
}
static float filter(bk_voice_t *v,float x,int ch){
    for(int n=0;n<3;n++){
        float *z=v->filter_z[ch][n],*b=v->filter_b[n],*a=v->filter_a[n];
        float y=b[0]*x+z[0];z[0]=b[1]*x-a[0]*y+z[1];z[1]=b[2]*x-a[1]*y;x=y;
    }
    return x;
}
void bk_voice_render(bk_voice_t *v,const bk_shape_params_t *p,
                     const bk_adsr_t *amp,float motion,
                     const float previous[BK_CONTOUR_SIZE][2],
                     const float next[BK_CONTOUR_SIZE][2],
                     const float mod_previous[BK_CONTOUR_SIZE][2],
                     const float mod_next[BK_CONTOUR_SIZE][2],float blend,
                     float *left,float *right){
    *left=*right=0;if(!v->active)return;
    v->pressure+=(v->target_pressure-v->pressure)*v->slew;
    const int gate=v->held||v->min_gate>0;
    if(v->min_gate>0)v->min_gate--;
    bk_envelope_gate(&v->amp_env,gate);
    bk_adsr_t pressure_amp=*amp;pressure_amp.attack*=1-.65f*v->pressure;
    v->envelope=bk_envelope_tick(&v->amp_env,&pressure_amp,v->sample_rate);
    if(!gate&&v->amp_env.stage==BK_ENV_IDLE){v->active=0;v->last_output[0]=v->last_output[1]=0;return;}
    // The ratio is taken from the knobs alone -- not from the modulation
    // envelope, and not from pressure. Both of those move during a note, and
    // the ratio sets where every sideband sits, so letting either one reach it
    // slides the whole partial structure and the ear hears a glissando. A mod
    // envelope aimed at Spikes then reads as a pitch envelope rather than a
    // timbre one. Held still, the partials stay put and the envelope is heard
    // as the shape changing, which is what it is.
    const bk_shape_params_t knobs=*p;
    const bk_shape_params_t effective=bk_shape_modulate(p,v->pressure);
    p=&effective;
    float l=0,r=0;
    // Key-track the index UPWARD, which is the opposite of the usual move and
    // of what was here first.
    //
    // The contour is band-limited by pitch, so the top of the keyboard has far
    // fewer harmonics to be bright with -- about seven at C7 against a couple
    // of hundred at C2, because the eighth would sit above 16 kHz. Nothing can
    // return those. What can be done is to fill the band that remains: driving
    // the modulation harder puts more sidebands inside it, and relative
    // brightness at C6 goes from 2.2x the fundamental to 5.3x, at C7 from 1.9x
    // to 3.8x, with the bottom two octaves untouched.
    //
    // It also lowers the energy below the fundamental rather than raising it,
    // 0.06 to 0.11 percent against 0.24 to 0.29. Pushing past this buys
    // nothing -- at 4x, C7 comes out no brighter, which is the available band
    // being full.
    const float keytrack=fminf(2.5f,fmaxf(1.f,v->increment*v->sample_rate/400.f));
    // Velocity drives the index as well as the level. On an FM instrument that
    // is the expressive gesture -- playing harder has to get brighter, not just
    // louder -- and the index is already this engine's brightness control.
    const float touch=.4f+.6f*v->velocity;
    const float index=(.025f+.28f*p->spikes+.22f*p->pinch+.3f*p->morph+.2f*p->bulge+.2f*v->pressure)*touch*keytrack;
    // Free-running and irrational, driven by Spikes and Pinch alone. Two
    // operators at an irrational ratio share no period, so the partials stop
    // lining up into a harmonic series -- which is the clangorous character
    // this instrument is for. Morph is kept out of it, and therefore pressure
    // too since pressure reaches the shape through Morph, so the primary axis
    // and the pad gesture stay strictly in tune.
    //
    // The range starts at 2, not 1. Sidebands sit at f*(1 - n*ratio); for a
    // ratio between 1 and 2 the first of them lands BELOW the fundamental and
    // drags the perceived pitch down with it -- measured, Pinch at 0.5 put the
    // lowest strong partial 246 cents flat and Spikes at 1.0 put it 1508 cents
    // flat. From 2 upwards every sideband folds back above the fundamental, so
    // the note keeps its pitch while the partials between are as inharmonic as
    // before. Metallic, not detuned.
    const float ratio=2+1.41421356f*knobs.spikes+1.7320508f*knobs.pinch;
    for(int os=0;os<4;os++){
        v->phase_a=wrap(v->phase_a+v->increment*.25f);
        v->phase_b=wrap(v->phase_b+v->increment*.25f*ratio);
        const float mod=scan(mod_previous,mod_next,v->phase_b,1,blend);
        const float phase=v->phase_a+index*mod+.06f*p->wobble*v->feedback;
        const float x=scan(previous,next,phase,0,blend);
        const float y=scan(previous,next,phase+.035f*motion,1,blend);
        v->feedback=.75f*x+.25f*y;
        l=filter(v,.85f*x+.35f*y,0);r=filter(v,.35f*x+.85f*y,1);
    }
    float raw[2]={l,r},out[2];
    for(int ch=0;ch<2;ch++){
        out[ch]=raw[ch]-v->dc_in[ch]+.997f*v->dc_out[ch];
        v->dc_in[ch]=raw[ch];v->dc_out[ch]=out[ch];
    }
    // Pressure swells the level as well as the timbre. Leaning into a held pad
    // should push the note forward, which is what aftertouch is for; the output
    // saturator downstream keeps the top end from running away.
    const float gain=v->velocity*v->envelope*.7f*(1+.45f*v->pressure);
    *left=out[0]*gain+v->steal_tail[0];*right=out[1]*gain+v->steal_tail[1];
    for(int ch=0;ch<2;ch++){
        v->steal_tail[ch]*=1-v->steal_slew;
        if(fabsf(v->steal_tail[ch])<1e-7f)v->steal_tail[ch]=0;
    }
    v->last_output[0]=*left;v->last_output[1]=*right;
}
