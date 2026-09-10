/* Timbre controls must not move the pitch. An operator ratio that shares no
   period with the carrier makes the composite aperiodic, which the ear hears
   as a lost or halved fundamental — so hold the ratio near a whole number and
   keep pressure out of it entirely. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "synth.h"
#define RATE 44100.0f
#define FRAMES 2048
static float periodicity(bk_synth_t *s,float hz){
    static float buf[FRAMES*2];
    float mono[FRAMES];
    for(int b=0;b<24;b++)bk_synth_render(s,buf,FRAMES); /* settle slews and attack */
    bk_synth_render(s,buf,FRAMES);
    double mean=0;
    for(int i=0;i<FRAMES;i++){mono[i]=(buf[2*i]+buf[2*i+1])*.5f;mean+=mono[i];}
    mean/=FRAMES;
    for(int i=0;i<FRAMES;i++)mono[i]-=(float)mean;
    const int lag=(int)(RATE/hz+.5f);
    double num=0,den=0;
    for(int i=0;i+lag<FRAMES;i++){num+=mono[i]*mono[i+lag];den+=mono[i]*mono[i];}
    return den>1e-12?(float)(num/den):0;
}
static float run(bk_shape_params_t p,int pressure){
    bk_synth_t s;bk_synth_init(&s,RATE);
    bk_adsr_t amp={0,.25f,1,.25f};
    bk_synth_set_envelopes(&s,&amp);
    bk_synth_set_shape(&s,&p);
    bk_synth_note_on(&s,48,110);
    if(pressure)bk_synth_pressure(&s,48,pressure);
    return periodicity(&s,440.0f*powf(2,(48-69)/12.0f));
}
int main(void){
    const char *names[]={"neutral","morph","bulge","tilt","wobble"};
    bk_shape_params_t cases[]={
        {0,0,0,0,.5f,0},{1,0,0,0,.5f,0},{0,1,0,0,.5f,0},{0,0,0,0,1,0},{0,0,0,0,.5f,1}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i){
        const float ac=run(cases[i],0);
        printf("  %-8s periodicity at the fundamental %.3f\n",names[i],ac);
        assert(ac>.9f);
    }
    /* The other two are expected to go inharmonic, and are checked for it: if
       Pinch ever starts holding the pitch, the ratio has been flattened and
       the character has quietly gone with it. */
    {   const bk_shape_params_t pinch={0,0,1,0,.5f,0},spikes={0,0,0,1,.5f,0};
        const float pa=run(pinch,0),sa=run(spikes,0);
        printf("  pinch    periodicity %.3f (inharmonic by design)\n",pa);
        printf("  spikes   periodicity %.3f (inharmonic by design)\n",sa);
        assert(pa<.5f); assert(sa<.7f); }
    /* Pressure is the strictest case: it is a continuous gesture, so any ratio
       movement under it is heard as the note sliding out of tune. */
    for(int p=32;p<=127;p+=31){
        const float ac=run(cases[0],p);
        printf("  pressure %3d periodicity %.3f\n",p,ac);
        assert(ac>.9f);
    }
    puts("PASS: Morph and pressure hold the fundamental; Pinch and Spikes stay inharmonic");
}
