import fs from "node:fs";
import vm from "node:vm";
import assert from "node:assert/strict";

const source = fs.readFileSync("src/ui/canvas.js", "utf8");
assert(!/drawPage[\s\S]{0,5000}getParam/.test(source), "drawPage must not read params");
const box = { globalThis: {} };
vm.runInNewContext(source, box, { filename: "canvas.js" });
const overlay = box.globalThis.canvas_overlay;
assert.equal(typeof overlay.drawPage, "function");
assert.equal(overlay.onMidi, undefined, "as_page must not rely on fullscreen-only MIDI hooks");
const points = box.globalThis.BOUBA_KIKI_SHAPE_FOR_TEST;
assert.equal(typeof points, "function");

const base = {morph:0,bulge:0,pinch:0,spikes:0,tilt:0.5,wobble:0,attack:0,release:0,pressure:0};
const bouba = points(base, 0, 96, 42);
const kiki = points({...base,morph:1}, 0, 96, 42);
assert(bouba.length > 100); assert(kiki.length > 20);
// Every control must visibly deform the base, including at both morph endpoints.
const controls=['bulge','pinch','spikes','tilt','wobble'];
assert.deepEqual(points({...base,attack:1,release:1},0,96,42),bouba,'envelopes do not distort the neutral base');
for (const morph of [0,1]) {
  for(const key of controls){
    const steps=[0,.25,.5,.75,1].map(value=>JSON.stringify(points({...base,morph,[key]:value},900,96,42)));
    assert.equal(new Set(steps).size,5,`${key} must keep changing across its range at morph ${morph}`);
    const low=points({...base,morph,[key]:0},900,96,42);
    const high=points({...base,morph,[key]:1},900,96,42);
    assert.notDeepEqual(low,high,`${key} must deform morph ${morph}`);
    // Compare actual illuminated pixels, not point indices or a tiny numeric delta.
    const raster=p=>{
      const pixels=new Set();
      for(let i=0;i<p.length;i++){
        const a=p[i],b=p[(i+1)%p.length],n=Math.max(Math.abs(b[0]-a[0]),Math.abs(b[1]-a[1]),1);
        for(let j=0;j<=n;j++) pixels.add(`${Math.round(a[0]+(b[0]-a[0])*j/n)},${Math.round(a[1]+(b[1]-a[1])*j/n)}`);
      }
      return pixels;
    };
    const a=raster(low),b=raster(high);
    const changed=[...a].filter(p=>!b.has(p)).length+[...b].filter(p=>!a.has(p)).length;
    assert(changed/(a.size+b.size)>.35,`${key} must dramatically change rendered silhouette at morph ${morph}`);
  }
}
for (const shape of [bouba,kiki]) for (const [x,y] of shape)
  assert(x >= 0 && x < 96 && y >= 0 && y < 42, `out of bounds ${x},${y}`);
const roughness = p => p.reduce((sum, q, i) => {
  const a=p[(i+p.length-1)%p.length], b=p[(i+1)%p.length];
  return sum + Math.abs(b[0]-2*q[0]+a[0])+Math.abs(b[1]-2*q[1]+a[1]);
},0);
assert(roughness(kiki) > 0, 'reference Kiki has corners');

let pixels = 0;
const ctx = {width:96,height:42,fillRect(){pixels++;},line(){pixels++;}};
overlay.drawPage(ctx,{values:{...base,morph:0.5,pulse:1,pressure:1},nowMs:1000});
assert(pixels > 20, "shape page should draw an outline");
console.log("PASS: canvas");

// The host creates a fresh frame context per draw. Events must survive that,
// and a slow telemetry poll must not lose a note that already ended.
function capture(visual,t){
  const segments=[];
  overlay.drawPage({width:128,height:48,line(...p){segments.push(p);}},
    {values:{...base,morph:.5,visual},nowMs:t});
  return JSON.stringify(segments);
}
const resting=capture('7,0,0,0',1000);
const event=capture('7,1,1,0',1400);
assert.notEqual(resting,event,'late note event still creates pulse');
const settled=capture('7,1,1,0',2400);
assert.equal(resting,settled,'pulse decays with wall time');
assert.equal(capture('8,0,0,0',1400),resting,'independent instance has no pulse');
assert.equal(capture('7,1,1,0',2500),settled,'unchanged state is stable');
assert.notEqual(capture('9,0,0,0,0',1000),capture('10,0,0,0,1',1000),
  'the live amplitude envelope must expand and relax the outline');
// id, serial, velocity, pressure, envelope, then the six shape values as the
// DSP actually applied them, the ripple phase, and the six knob values it
// started from: 18 fields. The drawing adds the difference to its own fresh
// knob values, so pressure's deformation shows without waiting for a poll.
// The offsets stay small enough here that adding them to the page's morph of
// .5 does not reach the rail, where both would clamp to the same outline.
// The drawing must round the teeth off by pitch exactly as the DSP does, or it
// shows partials the sound does not contain. Field 19 carries the frequency.
{
  const seg=(hz)=>{
    const out=[];
    overlay.drawPage({width:128,height:48,line(...p){out.push(p);}},
      {values:{...base,morph:.5,spikes:1,
               visual:['30'+String(hz|0),0,0,0,1,.5,0,0,1,.5,0,0,.5,0,0,1,.5,0,hz].join(',')},nowMs:1000});
    return JSON.stringify(out);
  };
  assert.notEqual(seg(130),seg(1500),'a high note must draw a smoother outline than a low one');
  assert.equal(seg(130),seg(200),'below 420 Hz nothing is smoothed, so the outline is identical');
}
const telemetry=(id,morph)=>[id,0,0,0,1, morph,0,0,0,.5,0, 0, 0,0,0,0,.5,0].join(',');
assert.notEqual(capture(telemetry(11,.1),1000),capture(telemetry(12,.4),1000),
  'the offset the DSP reports must reach the drawn contour');
