"""Render real plugin audio and compare controls. Requires numpy for analysis."""
import ctypes as c
import json
import pathlib
import sys
import time
import wave
import numpy as np

Create=c.CFUNCTYPE(c.c_void_p,c.c_char_p,c.c_char_p)
Destroy=c.CFUNCTYPE(None,c.c_void_p)
Midi=c.CFUNCTYPE(None,c.c_void_p,c.POINTER(c.c_uint8),c.c_int,c.c_int)
Set=c.CFUNCTYPE(None,c.c_void_p,c.c_char_p,c.c_char_p)
Get=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_char_p,c.c_char_p,c.c_int)
Render=c.CFUNCTYPE(None,c.c_void_p,c.POINTER(c.c_int16),c.c_int)
class Api(c.Structure):
    _fields_=[('version',c.c_uint32),('create',Create),('destroy',Destroy),('midi',Midi),('set',Set),('get',Get),('error',Get),('render',Render)]

lib=c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
lib.move_plugin_init_v2.restype=c.POINTER(Api)
api=lib.move_plugin_init_v2(None).contents
outdir=pathlib.Path(sys.argv[2]);outdir.mkdir(parents=True,exist_ok=True)
RATE=44100
# The shipped defaults: neutral Bouba with a sustaining amplitude envelope and
# no modulation. Every sweep below moves exactly one control away from this.
base=dict(morph=0,bulge=0,pinch=0,spikes=0,tilt=.5,wobble=0,mod_amount=0,mod_destination=0,
          attack=.05,decay=.25,sustain=1,release=.25,
          mod_attack=0,mod_decay=.3,mod_sustain=0,mod_release=.2)
GEOMETRY=['morph','bulge','pinch','spikes','tilt','wobble']
def render(params,seconds=2,notes=(48,),sweep=None,hold=None):
    """Render one take. `hold` releases the notes that many seconds in."""
    inst=api.create(b'.',None)
    assert inst
    for k,v in params.items():api.set(inst,k.encode(),str(v).encode())
    for note in notes:api.midi(inst,(c.c_uint8*3)(0x90,note,110),3,0)
    blocks=int(seconds*RATE/128);result=[];buf=(c.c_int16*256)()
    release_block=None if hold is None else int(hold*RATE/128)
    started=time.perf_counter()
    for i in range(blocks):
        if release_block is not None and i==release_block:
            for note in notes:api.midi(inst,(c.c_uint8*3)(0x80,note,0),3,0)
        if sweep:api.set(inst,sweep.encode(),str(i/max(1,blocks-1)).encode())
        api.render(inst,buf,128);result.append(np.ctypeslib.as_array(buf).copy().reshape(-1,2))
    elapsed=time.perf_counter()-started
    api.destroy(inst)
    return np.concatenate(result).astype(np.float64)/32768,elapsed/blocks*1e6
def save(name,a):
    with wave.open(str(outdir/(name+'.wav')),'wb') as f:
        f.setnchannels(2);f.setsampwidth(2);f.setframerate(RATE)
        f.writeframes(np.clip(a*32767,-32768,32767).astype('<i2').tobytes())
def rms(a):return float(np.sqrt(np.mean(a**2)))
def metrics(a):
    tail=a[len(a)//2:];mono=tail.mean(axis=1)
    spec=np.abs(np.fft.rfft(mono*np.hanning(len(mono))))**2
    freq=np.fft.rfftfreq(len(mono),1/RATE)
    # Single-note previews use MIDI 48. Exclude a 3 Hz window around harmonics.
    fundamental=440*2**((48-69)/12)
    distance=np.abs(freq-np.rint(freq/fundamental)*fundamental)
    inharmonic=float(np.sum(spec[distance>3])/max(1e-12,np.sum(spec)))
    return dict(rms_db=round(float(20*np.log10(rms(tail)+1e-12)),2),
                peak=round(float(np.max(np.abs(a))),4),
                centroid_hz=round(float(np.sum(freq*spec)/max(1e-12,np.sum(spec))),1),
                stereo_difference=round(float(np.sqrt(np.mean((tail[:,0]-tail[:,1])**2))),5),
                off_harmonic_energy=round(inharmonic,4))
def onset_seconds(a,fraction=.9):
    """Time until the smoothed level first reaches `fraction` of its peak."""
    mono=np.abs(a.mean(axis=1));window=np.ones(256)/256
    level=np.convolve(mono,window,'same');target=fraction*level.max()
    return float(np.argmax(level>=target))/RATE
report={}
for name,p in [('bouba',base),('kiki',dict(base,morph=1)),('kiki_spikes',dict(base,morph=1,spikes=.6))]:
    a,cost=render(p);save(name,a);report[name]=dict(metrics(a),us_per_block=round(cost,1))
for key in GEOMETRY:
    low,_=render(dict(base,**{key:0}));high,_=render(dict(base,**{key:1}))
    difference=rms(high-low)/max(1e-9,rms(low))
    report[key+'_range']=dict(relative_audio_change=round(float(difference),3),low=metrics(low),high=metrics(high))
    a,_=render(base,seconds=4,sweep=key);save(key+'_sweep',a)
# Amplitude envelope: attack shapes the onset, release shapes the tail after note off.
fast,_=render(dict(base,attack=0),seconds=2);slow,_=render(dict(base,attack=1),seconds=2)
save('attack_fast',fast);save('attack_slow',slow)
report['attack']=dict(fast_onset_s=round(onset_seconds(fast),3),slow_onset_s=round(onset_seconds(slow),3))
short,_=render(dict(base,release=0),seconds=3,hold=1);long,_=render(dict(base,release=1),seconds=3,hold=1)
save('release_short',short);save('release_long',long)
tail=slice(int(1.25*RATE),int(2.5*RATE))
report['release']=dict(short_tail_db=round(float(20*np.log10(rms(short[tail])+1e-12)),2),
                       long_tail_db=round(float(20*np.log10(rms(long[tail])+1e-12)),2))
# Modulation: a slow mod envelope must move its destination during a held note.
for destination,name in enumerate(GEOMETRY):
    a,_=render(dict(base,mod_destination=destination,mod_amount=1,mod_attack=.7,mod_sustain=1),seconds=2)
    save('mod_'+name,a)
    early=a[int(.05*RATE):int(.35*RATE)];late=a[int(1.4*RATE):int(1.9*RATE)]
    report['mod_'+name]=dict(early=metrics(early),late=metrics(late))
# Aliasing: a clean oscillator has nothing below its own fundamental. The
# contour is a wavetable, so its narrow teeth fold down without the index
# key-track and the pitch-dependent contour smoothing.
for note in (60,72,84,96):
    f0=440*2**((note-69)/12)
    a,_=render(dict(base,spikes=1),seconds=1.2,notes=(note,))
    save('alias_note_%d'%note,a)
    m=a[len(a)//2:].mean(axis=1)
    sp=np.abs(np.fft.rfft(m*np.hanning(len(m))))**2;f=np.fft.rfftfreq(len(m),1/RATE)
    below=float(sp[(f>40)&(f<f0*.94)].sum());total=float(sp[f>40].sum())
    report['alias_note_%d'%note]=round(100*below/max(1e-12,total),3)
chord,cost=render(dict(base,morph=1,spikes=1,bulge=1),seconds=3,notes=(48,55,60,64))
save('kiki_chord',chord);report['chord']=dict(metrics(chord),us_per_block=round(cost,1))
for index in range(6):
    a,_=render(dict(base,preset=index),seconds=2)
    save('preset_%d'%index,a);report['preset_%d'%index]=metrics(a)
(outdir/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
def moved(a,b):
    """True when two takes differ in timbre, not merely in level.

    Brightness alone is not enough: Tilt shears the contour, which rebalances
    the two scan axes (stereo width) instead of adding upper partials.
    """
    return (max(a['centroid_hz'],b['centroid_hz'])/max(1e-9,min(a['centroid_hz'],b['centroid_hz']))>1.2
            or max(a['stereo_difference'],b['stereo_difference'])/max(1e-9,min(a['stereo_difference'],b['stereo_difference']))>1.5
            or abs(a['off_harmonic_energy']-b['off_harmonic_energy'])>.05)
if '--check' in sys.argv:
    assert report['bouba']['off_harmonic_energy']<.01, 'neutral Bouba should remain tonal'
    assert report['kiki']['off_harmonic_energy']>.1, 'Kiki must add non-VA inharmonic character without extra knobs'
    for key in GEOMETRY:
        r=report[key+'_range']
        assert r['relative_audio_change']>.3, key+' needs a substantial waveform change'
        assert moved(r['low'],r['high']), key+' needs a timbral change, not just polarity or level'
        assert abs(r['low']['rms_db']-r['high']['rms_db'])<6, key+' loses too much body'
    assert report['attack']['slow_onset_s']>report['attack']['fast_onset_s']+.3, 'Amp Attack must stretch the onset'
    assert report['release']['long_tail_db']>report['release']['short_tail_db']+12, 'Amp Release must hold the tail'
    for name in GEOMETRY:
        r=report['mod_'+name]
        assert moved(r['early'],r['late']), 'modulating '+name+' must change the tone during a held note'
    for note in (60,72,84,96):
        assert report['alias_note_%d'%note]<2.5, 'note %d folds partials below its fundamental'%note
    print('PASS: six geometric ranges, both envelopes, every modulation destination, six presets and no folded partials')
