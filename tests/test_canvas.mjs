import fs from "node:fs";
import vm from "node:vm";
import assert from "node:assert/strict";

const source = fs.readFileSync("src/ui/canvas.js", "utf8");
assert(!/drawPage[\s\S]{0,5000}getParam/.test(source), "drawPage must not read params");
const box = { globalThis: {} };
vm.runInNewContext(source, box, { filename: "canvas.js" });
const overlay = box.globalThis.canvas_overlay;
assert.equal(typeof overlay.drawPage, "function");
assert.equal(typeof overlay.onMidi, "function");
const points = box.globalThis.BOUBA_KIKI_SHAPE_FOR_TEST;
assert.equal(typeof points, "function");

const base = {morph:0,bulge:0,pinch:0,spikes:0,tilt:0.5,wobble:0,attack:0,release:0,pressure:0};
const bouba = points(base, 0, 96, 42);
const kiki = points({...base,morph:1,spikes:1}, 0, 96, 42);
assert.equal(bouba.length, 64); assert.equal(kiki.length, 64);
for (const shape of [bouba,kiki]) for (const [x,y] of shape)
  assert(x >= 0 && x < 96 && y >= 0 && y < 42, `out of bounds ${x},${y}`);
const roughness = p => p.reduce((sum, q, i) => {
  const a=p[(i+p.length-1)%p.length], b=p[(i+1)%p.length];
  return sum + Math.abs(b[0]-2*q[0]+a[0])+Math.abs(b[1]-2*q[1]+a[1]);
},0);
assert(roughness(kiki) > roughness(bouba) * 1.25, "Kiki should be visibly sharper");

let pixels = 0;
const ctx = {width:96,height:42,fillRect(){pixels++;},line(){pixels++;}};
overlay.drawPage(ctx,{values:{...base,morph:0.5},nowMs:1000});
assert(pixels > 20, "shape page should draw an outline");
overlay.onMidi({state:{}},{data:[0x90,60,127]});
console.log("PASS: canvas");
