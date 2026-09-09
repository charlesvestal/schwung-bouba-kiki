/* Bouba-Kiki's clean-outline page. Draw hooks consume cached values only. */
(function () {
    const TAU = Math.PI * 2;
    const clamp = v => Math.max(0, Math.min(1, Number.isFinite(Number(v)) ? Number(v) : 0));

    function shapePoints(values, nowMs, width, height, pulse) {
        const morph = clamp(values && values.morph);
        const bulge = clamp(values && values.bulge);
        const pinch = clamp(values && values.pinch);
        const spikes = clamp(values && values.spikes);
        const tilt = clamp(values && values.tilt) - 0.5;
        const wobble = clamp(values && values.wobble);
        const pressure = clamp(values && values.pressure);
        const bite = clamp(morph + pressure * 0.28);
        const clock = (Number(nowMs) || 0) * 0.001;
        const cx = (width - 1) * 0.5, cy = (height - 1) * 0.5;
        const scale = Math.max(1, Math.min(width, height) * 0.43);
        const points = [];
        for (let i = 0; i < 64; i++) {
            const a = TAU * i / 64;
            const roundLobes = bulge * 0.075 * Math.cos(a * 3);
            const inward = pinch * 0.075 * (0.5 + 0.5 * Math.cos(a * 2 - 0.7));
            const teeth = (0.025 * bite + 0.15 * bite * spikes) * Math.cos(a * 7);
            const drift = wobble * 0.045 * Math.cos(a * 3 + clock * 1.7);
            const r = Math.max(0.48, 0.72 + roundLobes - inward + teeth + drift + (pulse || 0));
            const xBias = 1 + tilt * 0.18 * Math.cos(a);
            const x = Math.round(cx + Math.cos(a) * scale * r * xBias);
            const y = Math.round(cy + Math.sin(a) * scale * r);
            points.push([Math.max(0,Math.min(width-1,x)), Math.max(0,Math.min(height-1,y))]);
        }
        return points;
    }

    function line(ctx, x0, y0, x1, y1) {
        if (typeof ctx.line === "function") { ctx.line(x0,y0,x1,y1,1); return; }
        if (typeof ctx.drawLine === "function") { ctx.drawLine(x0,y0,x1,y1,1); return; }
        let dx=Math.abs(x1-x0), dy=Math.abs(y1-y0), sx=x0<x1?1:-1, sy=y0<y1?1:-1, e=dx-dy;
        for (;;) {
            ctx.fillRect(x0,y0,1,1,1); if (x0===x1 && y0===y1) break;
            const e2=2*e; if(e2>-dy){e-=dy;x0+=sx;} if(e2<dx){e+=dx;y0+=sy;}
        }
    }

    globalThis.canvas_overlay = {
        drawPage(ctx, {values, nowMs}) {
            const state = ctx.state || (ctx.state = {});
            if (state.pulse === undefined) state.pulse = 0;
            state.pulse *= 0.88;
            if (state.pulse < 0.001) state.pulse = 0;
            const supplied = Object.assign({}, values || {}, {pressure: Math.max(clamp(values && values.pressure), state.pressure || 0)});
            const p = shapePoints(supplied, nowMs, ctx.width, ctx.height, state.pulse);
            for (let i=0;i<p.length;i++) {
                const a=p[i], b=p[(i+1)%p.length]; line(ctx,a[0],a[1],b[0],b[1]);
            }
        },
        onMidi(ctx, {data}) {
            if (!data || data.length < 3) return;
            const state = ctx.state || (ctx.state = {}), status=data[0]&0xf0;
            if (status===0x90 && data[2]) state.pulse=Math.max(state.pulse||0,0.025+data[2]/127*0.055);
            if (status===0xa0) state.pressure=data[2]/127;
            if (status===0x80 || (status===0x90 && !data[2])) state.pressure=0;
        }
    };
    globalThis.BOUBA_KIKI_SHAPE_FOR_TEST = (v,t,w,h) => shapePoints(v,t,w,h,0);
})();
