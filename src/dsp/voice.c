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
    float tail=v->active?v->last_output:0;
    memset(v,0,sizeof(*v));v->active=v->held=1;v->note=note;v->age=age;
    v->sample_rate=sample_rate;v->slew=1-expf(-1/(.004f*sample_rate));
    v->velocity=velocity/127.0f;v->increment=440*powf(2,(note-69)/12.0f)/sample_rate;
    // Hold the gate open briefly so a note released inside the same block still sounds.
    v->min_gate=(int)(.004f*sample_rate);
    v->ratio_step=-1;
    // Stagger each voice's ripple so a chord breathes instead of pulsing as one.
    // Golden-ratio spacing, because consecutive notes take consecutive ages and
    // any small integer step would leave them bunched within a few degrees.
    {   const float turns=(float)age*0.61803399f;
        v->wobble_phase=(turns-floorf(turns))*6.28318530718f; }
    v->steal_tail=tail;
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
                     const bk_adsr_t *amp,const bk_adsr_t *mod,float motion,
                     const float mod_depth[6],
                     const float previous[BK_CONTOUR_SIZE][2],
                     const float next[BK_CONTOUR_SIZE][2],float blend,
                     float *left,float *right){
    *left=*right=0;if(!v->active)return;
    v->pressure+=(v->target_pressure-v->pressure)*v->slew;
    const int gate=v->held||v->min_gate>0;
    if(v->min_gate>0)v->min_gate--;
    bk_envelope_gate(&v->amp_env,gate);bk_envelope_gate(&v->mod_env,gate);
    bk_adsr_t pressure_amp=*amp;pressure_amp.attack*=1-.65f*v->pressure;
    v->envelope=bk_envelope_tick(&v->amp_env,&pressure_amp,v->sample_rate);
    bk_envelope_tick(&v->mod_env,mod,v->sample_rate);
    if(!gate&&v->amp_env.stage==BK_ENV_IDLE){v->active=0;v->last_output=0;return;}
    const bk_shape_params_t effective=bk_shape_modulate(p,mod_depth,v->mod_env.level,v->pressure);
    // Pressure brightens through the index below, but is deliberately excluded
    // from the ratio: sliding the ratio under a held note is heard as detuning.
    const bk_shape_params_t tonal=bk_shape_modulate(p,mod_depth,v->mod_env.level,0);
    p=&effective;
    float l=0,r=0;
    // Key-track the index. The modulator is a whole contour, not a sine, so its
    // own harmonics multiply out into sidebands; without this the top octaves
    // fold that spread back down as grit. Real FM instruments dull with pitch
    // for the same reason.
    const float keytrack=fminf(1,420.0f/fmaxf(20.0f,v->increment*v->sample_rate));
    const float index=(.025f+.28f*p->spikes+.22f*p->pinch+.1f*p->morph+.2f*p->bulge+.2f*v->pressure)*keytrack;
    // Snap the operator ratio to a whole number, then push it slightly off.
    // An unconstrained irrational ratio leaves the two operators sharing no
    // period at all, so the composite is aperiodic and the ear loses the
    // fundamental. Snapping alone would hold pitch but strip the metallic
    // character, since that character is the inharmonicity. A small offset
    // keeps the fundamental anchored while the upper partials stay clangorous.
    const float raw_ratio=1+.41421356f*(tonal.morph+tonal.spikes)+1.7320508f*tonal.pinch;
    // Rather than snap the ratio to a whole number and step audibly across the
    // boundary, run a modulator at each neighbouring whole number and crossfade
    // between them. Both are near-periodic with the carrier, so the fundamental
    // survives, and the blend is continuous with no glide and no lag.
    const float detune=.05f*(tonal.morph+tonal.pinch+tonal.spikes);
    const int step=(int)raw_ratio;               /* raw_ratio >= 1 always */
    const float frac=raw_ratio-(float)step;
    if(step!=v->ratio_step){
        // Carry the shared accumulator across so the crossfade stays continuous.
        if(v->ratio_step>=0){
            if(step==v->ratio_step+1)v->phase_b=v->phase_c;
            else if(step==v->ratio_step-1)v->phase_c=v->phase_b;
        }
        v->ratio_step=step;
    }
    const float ratio=(float)step+detune,ratio_high=(float)(step+1)+detune;
    for(int os=0;os<4;os++){
        v->phase_a=wrap(v->phase_a+v->increment*.25f);
        v->phase_b=wrap(v->phase_b+v->increment*.25f*ratio);
        v->phase_c=wrap(v->phase_c+v->increment*.25f*ratio_high);
        const float mod=scan(previous,next,v->phase_b,1,blend)*(1-frac)
                       +scan(previous,next,v->phase_c,1,blend)*frac;
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
    const float gain=v->velocity*v->envelope*.7f;
    *left=out[0]*gain+v->steal_tail;*right=out[1]*gain+v->steal_tail;
    v->steal_tail*=1-v->slew;if(fabsf(v->steal_tail)<1e-7f)v->steal_tail=0;
    v->last_output=(*left+*right)*.5f;
}
