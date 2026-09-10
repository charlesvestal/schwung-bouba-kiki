// Convert the attributed Commons SVG into deterministic contour data.
// Geometry derivative: CC BY-SA 3.0; see NOTICE.
import fs from 'node:fs';
const svg=fs.readFileSync('src/ui/reference.svg','utf8');
function decode(id) {
  const tag=svg.match(new RegExp('<path id="'+id+'"[^>]*'))[0];
  const tokens=tag.match(/ d="([^"]+)/)[1].match(/[a-zA-Z]|[-+]?(?:\d*\.\d+|\d+)(?:e[-+]?\d+)?/g);
  let i=0,cmd='',x=0,y=0,control=null;const p=[];
  const num=()=>Number(tokens[i++]);
  while(i<tokens.length){
    if(/^[a-z]$/i.test(tokens[i]))cmd=tokens[i++];
    const relative=cmd===cmd.toLowerCase(), op=cmd.toUpperCase();
    if(op==='Z')break;
    const ox=x,oy=y;
    const pair=()=>{const a=num(),b=num();return [a+(relative?ox:0),b+(relative?oy:0)];};
    if(op==='M'||op==='L'){[x,y]=pair();p.push([x,y]);control=null;if(op==='M')cmd=relative?'l':'L';}
    else if(op==='C'||op==='S'){
      const a=op==='S'?(control?[2*x-control[0],2*y-control[1]]:[x,y]):pair();
      const b=pair(),e=pair();
      for(let k=1;k<=12;k++){const t=k/12,u=1-t;p.push([u*u*u*ox+3*u*u*t*a[0]+3*u*t*t*b[0]+t*t*t*e[0],u*u*u*oy+3*u*u*t*a[1]+3*u*t*t*b[1]+t*t*t*e[1]]);}
      [x,y]=e;control=b;
    }else throw Error('Unsupported path command '+cmd);
  }
  const xs=p.map(v=>v[0]),ys=p.map(v=>v[1]);
  const cx=(Math.min(...xs)+Math.max(...xs))/2,cy=(Math.min(...ys)+Math.max(...ys))/2;
  const scale=Math.max(Math.max(...xs)-Math.min(...xs),Math.max(...ys)-Math.min(...ys))/2;
  return p.map(([a,b])=>[(a-cx)/scale,(b-cy)/scale]);
}
function resample(p,n){
  const length=[0];for(let i=0;i<p.length;i++){const a=p[i],b=p[(i+1)%p.length];length.push(length[i]+Math.hypot(b[0]-a[0],b[1]-a[1]));}
  return Array.from({length:n},(_,i)=>{const d=i*length.at(-1)/n;let k=0;while(length[k+1]<d)k++;const t=(d-length[k])/(length[k+1]-length[k]),a=p[k],b=p[(k+1)%p.length];return [a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])];});
}
const bouba=decode('path2478'),kiki=decode('path2480');
// Start at the uppermost point and use the same winding for a stable morph.
function align(p){let a=resample(p,256);const area=a.reduce((s,v,i)=>{const b=a[(i+1)%a.length];return s+v[0]*b[1]-v[1]*b[0];},0);if(area<0)a.reverse();const start=a.reduce((j,v,i)=>v[1]<a[j][1]?i:j,0);return a.slice(start).concat(a.slice(0,start));}
const data={bouba,kiki,morphBouba:align(bouba),morphKiki:align(kiki)};
const cArray=rows=>'{'+rows.map(row=>'{'+row.map(x=>x.toFixed(8)+'f').join(',')+'}').join(',\n')+'}';
const basis=Array.from({length:256},(_,i)=>{const a=2*Math.PI*i/256;return [Math.cos(3*a),Math.cos(2*a),Math.pow(Math.max(0,Math.cos(11*a)),6),Math.sin(5*a),Math.cos(5*a)];});
fs.writeFileSync('src/dsp/contour_tables.h','/* Generated contour derivative: CC BY-SA 3.0; see NOTICE. */\n#ifndef BK_CONTOUR_TABLES_H\n#define BK_CONTOUR_TABLES_H\n'+
  'static const float BK_BOUBA[256][2]='+cArray(data.morphBouba)+';\n'+
  'static const float BK_KIKI[256][2]='+cArray(data.morphKiki)+';\n'+
  'static const float BK_BASIS[256][5]='+cArray(basis)+';\n#endif\n');
const canvas=fs.readFileSync('src/ui/canvas.js','utf8');
fs.writeFileSync('src/ui/canvas.js',canvas.replace(/\/\* CONTOURS_BEGIN \*\/[\s\S]*?\/\* CONTOURS_END \*\//,'/* CONTOURS_BEGIN */\n    const CONTOURS = '+JSON.stringify(data)+';\n    /* CONTOURS_END */'));
console.log('Compiled source contours:',bouba.length,kiki.length,'points');
