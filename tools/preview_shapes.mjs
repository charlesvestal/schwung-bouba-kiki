import fs from "node:fs";
import vm from "node:vm";
import zlib from "node:zlib";

const source=fs.readFileSync(new URL("../src/ui/canvas.js",import.meta.url),"utf8");
const box={globalThis:{}}; vm.runInNewContext(source,box);
const shape=box.globalThis.BOUBA_KIKI_SHAPE_FOR_TEST;
const cases=[
  ["BOUBA",{morph:0,bulge:.35,pinch:0,spikes:.25,tilt:.5,wobble:0}],
  ["MID",{morph:.5,bulge:.35,pinch:0,spikes:.25,tilt:.5,wobble:0}],
  ["KIKI",{morph:1,bulge:.35,pinch:0,spikes:1,tilt:.5,wobble:0}],
  ["BULGE",{morph:.2,bulge:1,pinch:0,spikes:0,tilt:.5,wobble:0}],
  ["PINCH",{morph:.4,bulge:0,pinch:1,spikes:0,tilt:.5,wobble:0}],
  ["TILT",{morph:.5,bulge:.2,pinch:.2,spikes:.4,tilt:1,wobble:0}],
];
const cellW=128,cellH=64,cols=3,rows=2,w=cellW*cols,h=cellH*rows,pix=new Uint8Array(w*h);
function line(x0,y0,x1,y1){let dx=Math.abs(x1-x0),dy=Math.abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,e=dx-dy;for(;;){if(x0>=0&&x0<w&&y0>=0&&y0<h)pix[y0*w+x0]=255;if(x0===x1&&y0===y1)break;let e2=2*e;if(e2>-dy){e-=dy;x0+=sx}if(e2<dx){e+=dx;y0+=sy}}}
cases.forEach(([name,v],n)=>{const ox=n%cols*cellW,oy=Math.floor(n/cols)*cellH,p=shape(v,1200,118,54);for(let i=0;i<p.length;i++){const a=p[i],b=p[(i+1)%p.length];line(ox+a[0]+5,oy+a[1]+5,ox+b[0]+5,oy+b[1]+5)}process.stdout.write(`${name} `)});
function crc(buf){let c=0xffffffff;for(const b of buf){c^=b;for(let k=0;k<8;k++)c=(c>>>1)^((c&1)?0xedb88320:0)}return (c^0xffffffff)>>>0}
function chunk(type,data){const t=Buffer.from(type),n=Buffer.alloc(4),sum=Buffer.alloc(4);n.writeUInt32BE(data.length);sum.writeUInt32BE(crc(Buffer.concat([t,data])));return Buffer.concat([n,t,data,sum])}
const raw=Buffer.alloc((w+1)*h);for(let y=0;y<h;y++){raw[y*(w+1)]=0;for(let x=0;x<w;x++)raw[y*(w+1)+1+x]=pix[y*w+x]}
const ihdr=Buffer.alloc(13);ihdr.writeUInt32BE(w,0);ihdr.writeUInt32BE(h,4);ihdr[8]=8;ihdr[9]=0;
const png=Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk("IHDR",ihdr),chunk("IDAT",zlib.deflateSync(raw)),chunk("IEND",Buffer.alloc(0))]);
fs.mkdirSync("shapes-out",{recursive:true});fs.writeFileSync("shapes-out/shapes.png",png);console.log("-> shapes-out/shapes.png");
