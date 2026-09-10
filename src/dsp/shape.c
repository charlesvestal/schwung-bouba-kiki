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

bk_shape_params_t bk_shape_modulate(const bk_shape_params_t *base,float pressure){
    bk_shape_params_t p=*base;
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
