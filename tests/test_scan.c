#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "shape.h"
int main(void){
    float a[BK_CONTOUR_SIZE][2],b[BK_CONTOUR_SIZE][2];
    for(int end=0;end<2;end++)for(int key=0;key<6;key++){
        bk_shape_params_t p={end,0,0,0,.5f,0},q=p;
        switch(key){case 0:p.morph=0;q.morph=1;break;
        case 1:q.bulge=1;break;case 2:q.pinch=1;break;case 3:q.spikes=1;break;
        case 4:p.tilt=0;q.tilt=1;break;case 5:q.wobble=1;break;}
        bk_contour_build(&p,.9f,a);
        bk_contour_build(&q,.9f,b);
        double delta=0;
        for(int i=0;i<BK_CONTOUR_SIZE;i++)for(int c=0;c<2;c++){
            assert(isfinite(b[i][c]));delta+=fabsf(b[i][c]-a[i][c]);
        }
        assert(delta/(BK_CONTOUR_SIZE*2)>.08);
    }
    puts("PASS: six geometric controls deform the audio contour at both endpoints");
}
