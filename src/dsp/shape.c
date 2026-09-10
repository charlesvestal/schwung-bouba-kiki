#include "shape.h"
#include <math.h>
#include "contour_tables.h"
static float clamp01(float v) {
    if(!isfinite(v)) return 0;
    return fmaxf(0,fminf(1,v));
}
void bk_shape_defaults(bk_shape_params_t *p) {
    *p=(bk_shape_params_t){0,0,0,0,.5f,0};
}

/* Modulate toward the limit rather than adding and clamping.
 *
 * Adding threw the envelope away exactly when the amount was turned up: at a
 * base of 0.5 with amount +1 the target reached its ceiling halfway up the
 * attack and sat flat for the rest, so the ADSR's shape stopped reaching the
 * sound. Scaling by the room that is left keeps the whole contour of the
 * envelope audible at every base position and every amount.
 *
 * A negative amount still cannot pull a control below zero -- nothing can --
 * but it now moves proportionally to how far up the control already is,
 * instead of clamping flat the moment it reaches the floor. */
static float toward(float base,float amount){
    if(amount>=0)return base+amount*(1-base);
    return base+amount*base;
}
bk_shape_params_t bk_shape_modulate(const bk_shape_params_t *base,const float depth[6],float envelope,float pressure){
    bk_shape_params_t p=*base;
    p.morph=toward(p.morph,depth[0]*envelope);
    p.bulge=toward(p.bulge,depth[1]*envelope);
    p.pinch=toward(p.pinch,depth[2]*envelope);
    p.spikes=toward(p.spikes,depth[3]*envelope);
    p.tilt=toward(p.tilt,depth[4]*envelope);
    p.wobble=toward(p.wobble,depth[5]*envelope);
    p.morph+=.3f*pressure;
    bk_shape_clamp(&p);return p;
}
void bk_shape_clamp(bk_shape_params_t *p) {
    p->morph=clamp01(p->morph);p->bulge=clamp01(p->bulge);
    p->pinch=clamp01(p->pinch);p->spikes=clamp01(p->spikes);
    p->tilt=clamp01(p->tilt);p->wobble=clamp01(p->wobble);
}

/* Same geometry as canvas.js. Bounded 256-point work, no FFT or allocation. */
void bk_contour_build(const bk_shape_params_t *p,float phase,float out[BK_CONTOUR_SIZE][2]) {
    const float sn=sinf(phase),cs=cosf(phase);
    for(int i=0;i<BK_CONTOUR_SIZE;i++) {
        float x=BK_BOUBA[i][0]+(BK_KIKI[i][0]-BK_BOUBA[i][0])*p->morph;
        float y=BK_BOUBA[i][1]+(BK_KIKI[i][1]-BK_BOUBA[i][1])*p->morph;
        const float *t=BK_BASIS[i];
        const float lobe=1+p->bulge*(.65f+1.1f*t[0]);
        const float warp=lobe*(1+p->spikes*(2.8f*t[2]-.45f))*(1+.7f*p->wobble*(t[3]*cs+t[4]*sn));
        x*=warp*(1-.93f*p->pinch*(.5f+.5f*t[1]));
        y*=warp*(1+.6f*p->pinch);
        x+=2.4f*(p->tilt-.5f)*y;
        out[i][0]=x;out[i][1]=y;
    }
}
