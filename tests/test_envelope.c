#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "envelope.h"
static void advance(bk_envelope_t *e,const bk_adsr_t *p,int n){for(int i=0;i<n;i++)bk_envelope_tick(e,p,1000);}
int main(void){
    bk_adsr_t p={.1f,.2f,.3f,.4f};bk_envelope_t e={0};
    bk_envelope_gate(&e,1);advance(&e,&p,50);assert(fabsf(e.level-.5f)<.011f);
    advance(&e,&p,51);assert(e.level>.98f);
    advance(&e,&p,202);assert(fabsf(e.level-.3f)<.001f);
    advance(&e,&p,1000);assert(fabsf(e.level-.3f)<.001f);
    bk_envelope_gate(&e,0);advance(&e,&p,200);assert(fabsf(e.level-.15f)<.002f);
    advance(&e,&p,202);assert(e.level==0&&e.stage==BK_ENV_IDLE);
    // Release during attack must start at the current level and take R seconds.
    bk_envelope_gate(&e,1);advance(&e,&p,20);float level=e.level;
    bk_envelope_gate(&e,0);assert(e.level==level);advance(&e,&p,200);
    assert(fabsf(e.level-level*.5f)<.002f);
    // A re-press starts a new attack without a discontinuous value reset.
    level=e.level;bk_envelope_gate(&e,1);assert(e.level==level);
    advance(&e,&p,200);assert(e.level>level);
    bk_adsr_t zero={0,0,0,0};e=(bk_envelope_t){0};
    bk_envelope_gate(&e,1);advance(&e,&zero,4);assert(e.level==0);
    bk_envelope_gate(&e,0);advance(&e,&zero,4);assert(e.stage==BK_ENV_IDLE);
    puts("PASS: ADSR stages, sustain, early release, retrigger and zero times");
}
