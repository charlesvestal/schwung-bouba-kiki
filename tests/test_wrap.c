/* scan() must stay inside the 256-entry contour for every phase it can be
   given. wrap() returns exactly 1 for a tiny negative phase, which used to
   index one past the end; the negative phases come from the mean-removed
   contour sample that phase modulation adds. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "voice.c"
int main(void){
    /* The edge case is real: wrap() rounds up to exactly 1 just below zero. */
    assert(wrap(-1e-9f)==1.0f);
    /* Guard bands around the contour catch a read one entry past either end. */
    static struct { float guard_low[2][2]; float body[BK_CONTOUR_SIZE][2]; float guard_high[2][2]; } a, b;
    const float sentinel=1e30f;
    for(int i=0;i<2;i++)for(int c=0;c<2;c++)
        a.guard_low[i][c]=a.guard_high[i][c]=b.guard_low[i][c]=b.guard_high[i][c]=sentinel;
    for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++){
        a.body[i][c]=sinf(6.2831853f*i/BK_CONTOUR_SIZE);
        b.body[i][c]=cosf(6.2831853f*i/BK_CONTOUR_SIZE);
    }
    const float phases[]={-1e-9f,-3e-8f,-1e-7f,-.5f,-1,0,1e-9f,.5f,1,1.5f,-1234.5f,4096};
    for(unsigned p=0;p<sizeof(phases)/sizeof(phases[0]);p++)
        for(int c=0;c<2;c++)for(int blend=0;blend<2;blend++){
            float v=scan(a.body,b.body,phases[p],c,(float)blend);
            assert(isfinite(v)&&fabsf(v)<=1.0001f);
        }
    for(int i=0;i<2;i++)for(int c=0;c<2;c++){
        assert(a.guard_low[i][c]==sentinel&&a.guard_high[i][c]==sentinel);
        assert(b.guard_low[i][c]==sentinel&&b.guard_high[i][c]==sentinel);
    }
    puts("PASS: contour scan stays in bounds for negative and out-of-range phases");
}
