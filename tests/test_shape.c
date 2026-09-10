#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "shape.h"
int main(void) {
    bk_shape_params_t p;
    bk_shape_defaults(&p);
    assert(p.morph==0&&p.bulge==0&&p.pinch==0&&p.spikes==0&&p.tilt==.5f&&p.wobble==0);
    p.morph=4;p.bulge=-2;p.pinch=NAN;p.spikes=INFINITY;
    bk_shape_clamp(&p);
    assert(p.morph==1 && p.bulge==0 && p.pinch==0 && p.spikes==0);
    puts("PASS: shape defaults and finite parameter clamping");
}
