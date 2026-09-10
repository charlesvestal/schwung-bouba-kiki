#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include "synth.h"
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
    bk_synth_t s;float out[256];bk_synth_init(&s,44100);
    for(int n=48;n<52;n++)bk_synth_note_on(&s,n,110);
    double start=now();
    for(int i=0;i<10000;i++)bk_synth_render(&s,out,128);
    printf("four voices: %.2f us / 128 frames\n",(now()-start)*100);
    start=now();
    for(int i=0;i<100000;i++){bk_shape_params_t p=s.shape;p.morph=(i%100)*.01f;bk_synth_set_shape(&s,&p);}
    printf("shape update: %.3f us\n",(now()-start)*10);
    return 0;
}
