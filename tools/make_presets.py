#!/usr/bin/env python3
"""Writes Presets.hpp from the table below. Run it after changing a preset:
    python3 tools/make_presets.py
Each preset lists only what it changes from the defaults (Params.hpp).
Shapes: 0 ROSE, 1 HEART, 2 BUTTERFLY, 3 SPIROGRAPH, 4 LISSAJOUS KNOT, 5 INFINITY,
6 SPINNING LISSAJOUS, 7 MORPHING ROSE, 8 BREATHING SPIROGRAPH, 9 BEATING HEART,
10 ROTATING CUBE; 'c0', 'c1' = the preset's own typed shapes (cust).
Keys: a/b = shape list; am bm = MORPH; aoct asemi afine arot alev avoi (and b...);
atk dec sus rel; lrate lwave (0 sine 1 tri 2 saw 3 square 4 random); poly leg glide vol;
ft (0 LP12 1 LP24 2 HP 3 BP 4 NOTCH) cut res; sub suboct (-1/-2) noise; sync div (0 1/1 .. 4 1/16, 5 1/32,
6 1/4T 7 1/8T 8 1/16T 9 1/4. 10 1/8. 11 1/16.); draw steps st (semitones);
env / lfo = {destination: amount} (destinations: am asemi afine arot alev bm bsemi
bfine brot blev lrate vol cut res sub noise)."""
import os

SAW = ("(t - pi)/pi", "0.3sin(t)")
SQUARE = ("sign(cos(t))*abs(cos(t))^0.25", "sign(sin(t))*abs(sin(t))^0.25")
PULSE = ("sign(sin(t) - 0.5)", "sign(cos(t))")
STAR = ("cos(t) + 0.5cos(4t)", "sin(t) - 0.5sin(4t)")
NOISE = ("sin(13t)sin(7t)", "cos(11t)cos(3t)")
SPIRAL = ("(t/(2pi))cos(8t)", "(t/(2pi))sin(8t)")
NEBULA = ("cos(t)*(1 + 0.3sin(5t + u))", "sin(t)*(1 + 0.3sin(5t + u))")
MARIMBA = ("sin(t)", "0.6sin(4t)")
DIAMOND = ("asin(sin(t))", "asin(cos(t))")

P = []
def p(cat, name, cust=(), **k):
    P.append((cat, name, list(cust), k))

# ---------------- BASS
p("BASS", "SUB SCOPE", a=[5], aoct=-1, cut=600, res=0.2, atk=0.002, dec=0.3, sus=0.8, rel=0.12, poly=0, leg=1, glide=0.05, vol=0.75, sub=0.6)
p("BASS", "CUBE GROWL", a=[10], aoct=-1, avoi=3, b=[3], boct=-2, blev=0.5, ft=1, cut=300, res=0.45, env={"cut": 0.55}, atk=0.001, dec=0.35, sus=0.3, rel=0.15, poly=0)
p("BASS", "SAW BASS", [SAW], a=["c0"], aoct=-1, ft=1, cut=900, res=0.3, env={"cut": 0.35}, atk=0.001, dec=0.25, sus=0.6, rel=0.1, poly=0, vol=0.7, sub=0.4)
p("BASS", "REESE", a=[5], aoct=-1, avoi=6, b=[4], boct=-1, bfine=12, blev=0.6, ft=1, cut=700, res=0.2, lrate=0.3, lfo={"cut": 0.1}, atk=0.005, sus=0.9, rel=0.2, poly=0, leg=1, glide=0.03, vol=0.65, sub=0.4)
p("BASS", "WOBBLE", a=[10], aoct=-1, avoi=3, ft=1, cut=250, res=0.55, sync=1, div=3, lfo={"cut": 0.55}, sus=1, rel=0.1, poly=0, sub=0.5)
p("BASS", "ACID SQUELCH", [SQUARE], a=["c0"], aoct=-1, ft=1, cut=250, res=0.85, env={"cut": 0.5}, dec=0.18, sus=0.2, rel=0.08, poly=0, leg=1, glide=0.07, vol=0.6)
p("BASS", "KNOT DUB", a=[4], aoct=-1, b=[5], boct=-2, blev=0.5, cut=400, res=0.3, sync=1, div=6, lfo={"cut": 0.3}, sus=0.9, poly=0, sub=0.5, suboct=-2)
p("BASS", "PLUCK BASS", a=[3], aoct=-1, ft=1, cut=300, res=0.4, env={"cut": 0.45}, atk=0.001, dec=0.2, sus=0, rel=0.15)
p("BASS", "MORPH BASS", a=[5, 10], aoct=-1, env={"am": 1.0}, sus=0.7, cut=2000, poly=0)
p("BASS", "HEART THUMP", a=[9], aoct=-1, env={"asemi": 0.25}, dec=0.3, sus=0.5, cut=1200, poly=0)
p("BASS", "ROTOR BASS", a=[6], aoct=-1, b=[0], boct=-1, bsemi=7, blev=0.3, lrate=0.5, lfo={"arot": 0.5}, ft=1, cut=800, poly=0)

# ---------------- LEAD
p("LEAD", "PHOSPHOR LEAD", a=[1], avoi=4, b=[0], bsemi=7, blev=0.4, cut=4000, res=0.3, lrate=5.5, lfo={"afine": 0.08}, atk=0.01, dec=0.5, sus=0.8, rel=0.25, poly=0, leg=1, glide=0.08)
p("LEAD", "SPIRAL SAW", a=[3], avoi=6, ft=1, cut=2500, res=0.5, env={"cut": 0.3}, dec=0.6, sus=0.7)
p("LEAD", "SCREAMER", a=[10], avoi=5, b=[2], bsemi=12, blev=0.4, bvoi=3, ft=1, cut=3500, res=0.6, lrate=6, lfo={"afine": 0.06}, poly=0, leg=1, glide=0.06, vol=0.55)
p("LEAD", "SQUARE LEAD", [SQUARE], a=["c0"], avoi=2, cut=3000, res=0.2, lrate=5, lfo={"afine": 0.05}, poly=0, glide=0.04, vol=0.6)
p("LEAD", "STAR LEAD", [STAR], a=["c0"], avoi=3, cut=5000, res=0.3, lrate=3, lfo={"arot": 0.1}, poly=0, leg=1, glide=0.05)
p("LEAD", "SWEEP LEAD", a=[3], avoi=4, ft=1, cut=600, res=0.7, env={"cut": 0.55}, dec=1.2, sus=0.4, poly=0)
p("LEAD", "THEREMIN", a=[5], cut=2500, lrate=6.5, lfo={"afine": 0.1}, atk=0.15, rel=0.5, sus=1, poly=0, leg=1, glide=0.3)
p("LEAD", "CHIPTUNE", [PULSE], a=["c0"], atk=0.001, dec=0.1, sus=0.8, rel=0.05, lrate=7, lfo={"afine": 0.05}, poly=0, vol=0.5)
p("LEAD", "OCTAVE LEAD", a=[1], avoi=2, b=[1], boct=1, bfine=8, blev=0.6, cut=4000, res=0.25, poly=0, leg=1, glide=0.05)
p("LEAD", "BUTTERFLY LEAD", a=[2], avoi=3, ft=1, cut=1800, res=0.5, env={"cut": 0.3}, lrate=0.7, lfo={"arot": 0.15})
p("LEAD", "LASER LEAD", a=[6], env={"asemi": 0.3}, atk=0.001, dec=0.15, sus=0.8, cut=5000, poly=0)

# ---------------- SYNTH
p("SYNTH", "MORPH WAVE", a=[0, 1, 10], am=0.5, lrate=0.3, lwave=1, lfo={"am": 0.5}, cut=6000)
p("SYNTH", "SUPERSAW SCOPE", a=[3], avoi=8, b=[10], boct=-1, bvoi=6, blev=0.5, cut=6000, atk=0.01, rel=0.4, vol=0.45)
p("SYNTH", "STAB", a=[10], avoi=4, b=[2], bsemi=7, blev=0.5, ft=1, cut=1200, res=0.3, env={"cut": 0.45}, dec=0.25, sus=0.2, rel=0.2)
p("SYNTH", "POLY SWEEP", a=[0, 1, 10], am=0.5, lrate=0.25, lwave=1, lfo={"am": 0.5, "cut": 0.25}, ft=1, cut=1500, res=0.4)
p("SYNTH", "HOOVER", a=[2], aoct=-1, avoi=7, b=[3], bvoi=4, blev=0.5, env={"asemi": 0.1}, cut=3000, poly=0, leg=1, glide=0.12, vol=0.5)
p("SYNTH", "TRANCE GATE", a=[3], avoi=6, sync=1, div=4, lwave=3, lfo={"vol": 0.5}, sus=1, cut=5000, vol=0.55)
p("SYNTH", "SCOPE POLY", a=[1], avoi=2, b=[0], bsemi=12, blev=0.35, cut=2500, res=0.2, env={"cut": 0.3}, atk=0.01, dec=0.8, sus=0.6, rel=0.4)
p("SYNTH", "RESO SWEEP", a=[4], avoi=3, ft=1, cut=1500, res=0.75, lrate=0.15, lwave=1, lfo={"cut": 0.3})
p("SYNTH", "NOTCH PHASE", a=[3], avoi=4, ft=4, cut=1000, res=0.3, lrate=0.3, lfo={"cut": 0.4})
p("SYNTH", "TWIN SPIN", a=[6], b=[6], bfine=15, blev=0.7, lrate=0.2, lfo={"arot": 0.5, "brot": -0.5}, cut=4000, vol=0.6)
p("SYNTH", "DRIFTER", a=[8], avoi=3, b=[9], blev=0.4, lrate=1.5, lwave=4, lfo={"afine": 0.1}, cut=3500)

# ---------------- PLUCK
p("PLUCK", "GLASS PLUCK", a=[0], b=[4], bsemi=12, blev=0.4, cut=600, res=0.35, env={"cut": 0.6}, atk=0.001, dec=0.25, sus=0, rel=0.25)
p("PLUCK", "WIRE PLUCK", a=[10], avoi=2, ft=1, cut=1500, env={"cut": 0.4}, atk=0.001, dec=0.15, sus=0, rel=0.15)
p("PLUCK", "KOTO", a=[0], b=[4], bsemi=12, blev=0.3, cut=800, res=0.5, atk=0.001, dec=0.4, sus=0, rel=0.4, noise=0.0, env={"cut": 0.5, "asemi": 0.02, "noise": 0.4})
p("PLUCK", "HARP", a=[5], b=[5], boct=1, blev=0.3, cut=1500, env={"cut": 0.35}, dec=1.0, sus=0, rel=1.0)
p("PLUCK", "MUTED GUITAR", a=[3], aoct=-1, ft=1, cut=500, res=0.2, env={"cut": 0.3}, dec=0.12, sus=0, rel=0.1)
p("PLUCK", "CRYSTAL PLUCK", a=[2], aoct=1, b=[6], boct=2, blev=0.3, ft=2, cut=400, dec=0.35, sus=0, rel=0.35)
p("PLUCK", "MARIMBA", [MARIMBA], a=["c0"], cut=3000, dec=0.5, sus=0, rel=0.4, atk=0.001)
p("PLUCK", "PIZZ", a=[1], avoi=3, cut=1200, env={"cut": 0.4}, dec=0.18, sus=0, rel=0.2)
p("PLUCK", "DATA PLUCK", a=[10], aoct=1, ft=3, cut=1000, res=0.5, env={"cut": 0.3}, dec=0.1, sus=0, rel=0.12, vol=0.9)
p("PLUCK", "SPIN PLUCK", a=[0], env={"arot": 0.5, "cut": 0.3}, cut=2000, dec=0.3, sus=0, rel=0.3)
p("PLUCK", "SPIRO PLUCK", a=[8], avoi=2, ft=1, cut=900, res=0.45, env={"cut": 0.45}, dec=0.25, sus=0, rel=0.25)

# ---------------- PAD
p("PAD", "AURORA PAD", a=[8], avoi=6, b=[7], boct=1, blev=0.5, bvoi=4, atk=1.2, dec=2, sus=0.8, rel=2.5, cut=1800, res=0.15, lrate=0.2, lfo={"arot": 0.3, "cut": 0.15}, vol=0.4)
p("PAD", "SLOW ORBIT", a=[5, 6], am=0.5, avoi=4, lrate=0.1, lwave=1, lfo={"am": 0.5}, ft=1, cut=3000, atk=0.8, dec=1.5, sus=0.9, rel=3, vol=0.5)
p("PAD", "GLASS PAD", a=[0], avoi=5, b=[5], boct=1, blev=0.4, bvoi=3, ft=2, cut=250, atk=1.5, dec=2, sus=0.9, rel=3, lrate=0.1, lfo={"arot": 0.2}, vol=0.45)
p("PAD", "WARM BLANKET", a=[5], aoct=-1, avoi=6, b=[4], bvoi=4, blev=0.5, ft=1, cut=900, res=0.1, atk=1, sus=0.9, rel=2.5, vol=0.45)
p("PAD", "MORPH CLOUD", a=[0, 7, 8, 3], am=0.5, avoi=4, lrate=0.08, lwave=1, lfo={"am": 0.5}, cut=2500, atk=1.5, rel=3, vol=0.45)
p("PAD", "HEARTBEAT PAD", a=[9], b=[1], boct=1, blev=0.4, atk=0.8, rel=2, cut=1500, lrate=1.17, lfo={"vol": 0.15}, vol=0.5)
p("PAD", "STRING PAD", a=[3], avoi=8, b=[2], boct=1, bvoi=4, blev=0.35, cut=3000, res=0.1, atk=0.6, dec=1, sus=0.85, rel=1.5, vol=0.4)
p("PAD", "DARK MATTER", a=[10], aoct=-1, avoi=5, ft=1, cut=400, res=0.5, lrate=0.07, lwave=1, lfo={"cut": 0.25}, atk=2, rel=4, vol=0.5)
p("PAD", "SHIMMER", a=[6], aoct=1, avoi=4, b=[0], boct=2, blev=0.3, ft=2, cut=800, lrate=3, lwave=4, lfo={"afine": 0.1}, atk=1.2, rel=3, vol=0.45)
p("PAD", "BREATH PAD", a=[8], avoi=3, ft=3, cut=700, res=0.4, lrate=0.2, lfo={"cut": 0.3}, atk=1, rel=2.5, vol=0.9, noise=0.15)
p("PAD", "NEBULA", [NEBULA], a=["c0"], avoi=5, cut=2200, atk=1.5, rel=3, vol=0.45)

# ---------------- KEYS
p("KEYS", "SCOPE EP", a=[1], b=[0], boct=1, blev=0.3, cut=2500, env={"cut": 0.3}, atk=0.002, dec=1.2, sus=0.35, rel=0.4, lrate=4.5, lfo={"vol": 0.1}, vol=0.5)
p("KEYS", "TERMINAL ORGAN", a=[5], b=[5], boct=1, blev=0.6, cut=5000, atk=0.005, dec=0.1, sus=1, rel=0.08, vol=0.35)
p("KEYS", "DRAWBAR", a=[5], aoct=-1, b=[5], bsemi=7, blev=0.6, atk=0.005, sus=1, rel=0.06, lrate=6, lfo={"vol": 0.08}, vol=0.4)
p("KEYS", "CLAV", a=[10], ft=3, cut=1500, res=0.5, env={"cut": 0.3}, dec=0.25, sus=0.2, rel=0.08)
p("KEYS", "WURLY", a=[1], b=[4], bsemi=12, blev=0.25, cut=1800, env={"cut": 0.25}, dec=1.5, sus=0.3, rel=0.3, lrate=5, lfo={"vol": 0.12}, vol=0.55)
p("KEYS", "TOY PIANO", a=[2], aoct=1, ft=2, cut=600, dec=0.6, sus=0, rel=0.4)
p("KEYS", "HONKY TONK", a=[0], avoi=2, b=[0], bfine=18, blev=0.7, cut=3500, env={"cut": 0.2}, dec=1.8, sus=0.15, rel=0.5, vol=0.5)
p("KEYS", "BRIGHT KEYS", a=[3], b=[6], boct=2, blev=0.2, env={"blev": 0.4}, dec=1, sus=0.3, cut=4000, vol=0.55)
p("KEYS", "DIGI PIANO", a=[4], b=[0], boct=1, blev=0.35, cut=2500, env={"cut": 0.3}, dec=2.0, sus=0.1, rel=0.6, vol=0.55)
p("KEYS", "SOFT KEYS", a=[5], avoi=2, cut=1200, atk=0.02, dec=1.5, sus=0.4, rel=0.8)
p("KEYS", "CHORD ORGAN", a=[5], b=[5], bsemi=4, blev=0.5, cut=4000, atk=0.005, sus=1, rel=0.1, vol=0.35)

# ---------------- BELLS
p("BELLS", "CRT BELL", a=[0], b=[3], boct=2, bsemi=4, blev=0.5, cut=8000, atk=0.001, dec=2.5, sus=0, rel=2, vol=0.55)
p("BELLS", "GLASS CHIME", a=[2], aoct=1, b=[0], boct=2, bsemi=7, blev=0.4, ft=2, cut=300, atk=0.001, dec=1.8, sus=0, rel=1.8)
p("BELLS", "TUBULAR", a=[0], b=[3], boct=1, bsemi=2, bfine=14, blev=0.4, cut=6000, dec=3, sus=0, rel=3, vol=0.55)
p("BELLS", "MUSIC BOX", a=[2], aoct=2, b=[0], boct=3, blev=0.25, ft=2, cut=500, dec=1.2, sus=0, rel=1)
p("BELLS", "GAMELAN", a=[10], b=[6], bsemi=6, bfine=30, blev=0.5, cut=5000, dec=2, sus=0, rel=2, vol=0.55)
p("BELLS", "TEMPLE", a=[1], b=[1], boct=2, bsemi=7, bfine=-20, blev=0.4, ft=3, cut=1500, res=0.3, dec=3.5, sus=0, rel=3.5)
p("BELLS", "ICE BELL", a=[6], aoct=1, avoi=3, b=[4], boct=2, blev=0.3, ft=2, cut=1000, dec=1.5, sus=0, rel=2)
p("BELLS", "VIBRAPHONE", a=[5], b=[5], boct=2, blev=0.2, dec=2, sus=0, rel=1.5, lrate=5, lfo={"vol": 0.15}, vol=0.6)
p("BELLS", "SPIRO CHIME", a=[3], aoct=1, b=[8], boct=1, bsemi=5, blev=0.4, dec=2.2, sus=0, rel=2, vol=0.55)
p("BELLS", "COWBELL", a=[10], aoct=1, b=[10], boct=1, bsemi=8, blev=0.8, ft=3, cut=900, res=0.4, dec=0.35, sus=0, rel=0.3)
p("BELLS", "SPINNING BELL", a=[0], env={"arot": 0.4}, b=[1], boct=2, blev=0.3, dec=2.5, sus=0, rel=2)

# ---------------- ARP: the LFO in DRAW mode, synced, linked to SEMI at +50% so each step of 1/12 is a semitone
def arp(name, steps, div=4, both=False, cust=(), **k):
    lf = dict(k.pop("lfo", {}))
    lf["asemi"] = 0.5
    if both: lf["bsemi"] = 0.5
    p("ARP", name, cust, draw=1, sync=1, div=div, steps=len(steps), st=steps, lfo=lf, **k)
arp("ARP BLIP", [0, 3, 7, 12], a=[4], aoct=1, cut=3000, res=0.4, env={"cut": 0.3}, atk=0.001, dec=0.3, sus=0.8, rel=0.08, vol=0.55)
arp("ARP SPARKLE", [0, 4, 7, 11, 12, 11, 7, 4], both=True, a=[0], b=[1], boct=1, blev=0.4, sus=0.7, dec=0.2, rel=0.2, vol=0.6)
arp("MINOR RUN", [0, 3, 7, 10, 12, 10, 7, 3], a=[3], avoi=2, ft=1, cut=1500, res=0.3, env={"cut": 0.3}, sus=0.7, vol=0.6)
arp("OCTAVE BOUNCE", [0, 12], a=[5], aoct=-1, cut=900, res=0.3, sus=0.9, poly=0)
arp("TRIPLET CASCADE", [12, 7, 4, 0, 4, 7], div=8, both=True, a=[0], b=[1], boct=1, blev=0.3, sus=0.8, vol=0.6)
arp("DORIAN SEQ", [0, 2, 3, 5, 7, 9, 10, 12, 10, 9, 7, 5, 3, 2, 0, -2], a=[4], cut=2500, sus=0.8)
arp("TRANCE ARP", [0, 7, 12, 7], a=[3], avoi=6, ft=1, cut=2500, res=0.35, sus=0.8, vol=0.5)
arp("FALLING ARP", [12, 10, 7, 5, 3, 0, -2, -5], a=[1], cut=3000, sus=0.8)
arp("SLOW STEPS", [0, 5, 7, 12, 7, 5, 0, -5], div=3, a=[8], avoi=3, atk=0.02, sus=0.8, cut=2000)
arp("DOTTED ECHO", [0, 7, 12], div=10, a=[2], cut=3000, res=0.2, sus=0.8)
arp("CHIP ARP", [0, 4, 7], div=5, cust=[PULSE], a=["c0"], aoct=1, sus=0.9, atk=0.001, rel=0.05, vol=0.5)

# ---------------- FX
p("FX", "RADAR SWEEP", a=[6], atk=0.6, dec=3, sus=0.6, rel=4, env={"asemi": 0.5}, lrate=8, lwave=4, lfo={"cut": 0.4}, ft=3, cut=1500, res=0.5)
p("FX", "TUBE NOISE", a=[10], lrate=12, lwave=4, lfo={"asemi": 0.6, "arot": 1}, ft=2, cut=400, res=0.3, atk=0.01, dec=1, sus=0.5, rel=1.5, noise=0.25)
p("FX", "RISER", a=[10], avoi=6, env={"asemi": 0.5, "cut": 0.6}, atk=4, dec=0.1, sus=1, rel=1, ft=1, cut=300, vol=0.55)
p("FX", "DOWNLIFTER", a=[6], avoi=4, env={"asemi": 0.5, "cut": 0.3}, atk=0.001, dec=3, sus=0, rel=0.5, cut=4000)
p("FX", "ALARM", a=[4], lrate=3, lwave=3, lfo={"asemi": 0.25}, sus=1, ft=3, cut=1500, res=0.3)
p("FX", "SCI-FI ZAP", a=[0], env={"asemi": 0.5}, atk=0.001, dec=0.15, sus=0, rel=0.1)
p("FX", "WIND", [NOISE], a=["c0"], aoct=2, avoi=8, ft=3, cut=800, res=0.6, lrate=0.5, lwave=4, lfo={"cut": 0.3}, atk=1, rel=2, vol=0.5, noise=0.6)
p("FX", "HELICOPTER", a=[10], aoct=-2, lrate=11, lwave=3, lfo={"vol": 0.4}, cut=600, sus=1)
p("FX", "UFO", a=[6], avoi=2, lrate=4, lfo={"asemi": 0.2}, cut=3000, res=0.4, sus=1)
p("FX", "GLITCH", a=[10, 2, 6], am=0.5, sync=1, div=5, lwave=4, lfo={"am": 0.5}, ft=1, cut=3000, res=0.5, sus=1)
p("FX", "SONAR PING", a=[5], aoct=2, ft=3, cut=1000, res=0.8, dec=2.5, sus=0, rel=2.5, vol=0.9)

# ---------------- DRONE
p("DRONE", "DEEP SPACE", a=[5], aoct=-2, avoi=6, b=[4], boct=-1, blev=0.5, bvoi=4, ft=1, cut=300, res=0.4, lrate=0.05, lwave=1, lfo={"cut": 0.2}, atk=3, rel=5, vol=0.5, sub=0.5)
p("DRONE", "SINGING TUBES", a=[0], aoct=-1, avoi=4, b=[0], bsemi=7, blev=0.5, bvoi=4, ft=3, cut=900, res=0.7, lrate=0.07, lfo={"cut": 0.25}, atk=2, rel=4)
p("DRONE", "MACHINE HUM", a=[10], aoct=-2, b=[10], boct=-2, bfine=6, blev=0.8, cut=500, lrate=0.5, lfo={"vol": 0.1}, atk=1, rel=3, vol=0.5)
p("DRONE", "FIFTH DRONE", a=[3], aoct=-1, avoi=3, b=[3], boct=-1, bsemi=7, blev=0.7, bvoi=3, cut=1200, atk=2, rel=4, vol=0.5)
p("DRONE", "TIDE", a=[8], aoct=-1, avoi=5, lrate=0.1, lfo={"vol": 0.3}, cut=800, res=0.3, atk=3, rel=5)
p("DRONE", "STATIC FIELD", [NOISE], a=["c0"], aoct=-1, avoi=8, ft=1, cut=1500, res=0.2, lrate=2, lwave=4, lfo={"cut": 0.3}, atk=2, rel=4, vol=0.45, noise=0.3)
p("DRONE", "CATHEDRAL", a=[5], avoi=4, b=[5], boct=1, blev=0.4, cut=2000, res=0.2, lrate=0.05, lfo={"arot": 0.3}, atk=2.5, rel=5, vol=0.45)
p("DRONE", "SPIRAL DRONE", [SPIRAL], a=["c0"], aoct=-2, avoi=4, cut=900, atk=2, rel=4)
p("DRONE", "MORPHING DRONE", a=[5, 7, 10, 8], am=0.5, aoct=-1, avoi=4, lrate=0.05, lwave=1, lfo={"am": 0.5}, cut=1500, atk=2, rel=4)
p("DRONE", "HEART DRONE", a=[9], aoct=-2, avoi=3, b=[1], boct=-1, blev=0.4, ft=1, cut=600, res=0.3, atk=2, rel=4)

# ---------------- PERC: drums from pitch-dropping envelopes and noisy shapes
p("PERC", "SCOPE KICK", a=[5], aoct=-2, env={"asemi": 0.5}, atk=0.001, dec=0.25, sus=0, rel=0.1, cut=2000, poly=0, vol=0.85, sub=0.5)
p("PERC", "808 BOOM", a=[5], aoct=-2, env={"asemi": 0.3}, atk=0.001, dec=1.2, sus=0, rel=0.3, cut=600, poly=0, vol=0.85, sub=0.7)
p("PERC", "TOM", a=[0], aoct=-1, env={"asemi": 0.2}, atk=0.001, dec=0.4, sus=0, rel=0.2, cut=1500)
p("PERC", "SNARE", [NOISE], a=["c0"], aoct=2, avoi=8, b=[5], blev=0.5, atk=0.001, dec=0.18, sus=0, rel=0.12, ft=2, cut=300, noise=0.7, env={"asemi": 0.15, "bsemi": 0.15, "noise": -0.3})
p("PERC", "CLOSED HAT", [NOISE], a=["c0"], aoct=3, avoi=8, ft=2, cut=6000, res=0.2, atk=0.001, dec=0.05, sus=0, rel=0.05, vol=0.5, noise=0.8)
p("PERC", "OPEN HAT", [NOISE], a=["c0"], aoct=3, avoi=8, ft=2, cut=5000, atk=0.001, dec=0.4, sus=0, rel=0.3, vol=0.5, noise=0.8)
p("PERC", "CLAP", [NOISE], a=["c0"], aoct=2, avoi=8, ft=3, cut=1500, res=0.4, atk=0.001, dec=0.15, sus=0, rel=0.2, lrate=20, lwave=3, lfo={"vol": 0.4}, noise=0.8)
p("PERC", "RIM", a=[10], aoct=1, ft=3, cut=2500, res=0.6, atk=0.001, dec=0.04, sus=0, rel=0.04)
p("PERC", "CONGA", a=[1], env={"asemi": 0.1}, atk=0.001, dec=0.3, sus=0, rel=0.15, cut=3000)
p("PERC", "METAL HIT", a=[10], b=[10], bsemi=6, bfine=40, blev=0.8, ft=2, cut=800, res=0.5, atk=0.001, dec=0.5, sus=0, rel=0.4)
p("PERC", "ZAP DRUM", a=[6], aoct=-1, env={"asemi": 0.6}, atk=0.001, dec=0.15, sus=0, rel=0.1)

# ---------------- BRASS
p("BRASS", "SCOPE BRASS", a=[2], avoi=3, b=[1], boct=-1, blev=0.5, ft=1, cut=800, res=0.2, env={"cut": 0.45}, atk=0.06, dec=0.6, sus=0.7, rel=0.3)
p("BRASS", "BRASS SECTION", a=[3], avoi=5, b=[2], bvoi=3, blev=0.5, ft=1, cut=700, res=0.2, env={"cut": 0.5}, atk=0.08, dec=0.5, sus=0.75, rel=0.25, vol=0.5)
p("BRASS", "FANFARE", a=[10], avoi=3, b=[10], bsemi=7, blev=0.5, ft=1, cut=900, env={"cut": 0.45}, atk=0.05, dec=0.4, sus=0.8, rel=0.3, vol=0.55)
p("BRASS", "SYNTH HORN", a=[1], avoi=2, cut=600, res=0.3, env={"cut": 0.4}, atk=0.12, dec=0.6, sus=0.7, rel=0.3, lrate=5, lfo={"afine": 0.04})
p("BRASS", "STAB BRASS", a=[2], avoi=4, b=[3], bsemi=12, blev=0.3, ft=1, cut=500, env={"cut": 0.55}, atk=0.01, dec=0.2, sus=0.3, rel=0.15)
p("BRASS", "TUBA", a=[5], aoct=-1, ft=1, cut=400, env={"cut": 0.3}, atk=0.06, sus=0.9, poly=0, glide=0.03, sub=0.4)
p("BRASS", "SWELL BRASS", a=[3], avoi=4, ft=1, cut=300, res=0.2, env={"cut": 0.6}, atk=1.2, dec=0.5, sus=0.9, rel=0.6)
p("BRASS", "80S POLY BRASS", a=[10], avoi=4, ft=1, cut=1000, res=0.25, env={"cut": 0.35}, atk=0.04, dec=0.5, sus=0.6)
p("BRASS", "MUTED TRUMPET", a=[1], aoct=1, ft=3, cut=1200, res=0.45, env={"cut": 0.2}, atk=0.04, sus=0.8, poly=0, glide=0.02)
p("BRASS", "BRASS PAD", a=[3], avoi=6, b=[1], boct=-1, blev=0.5, bvoi=3, cut=1200, env={"cut": 0.25}, atk=0.4, dec=1, sus=0.8, rel=1, vol=0.45)

# ---------------- SCOPE ART: made to be watched (one note at a time, held)
ART = dict(poly=0, sus=1, rel=0.5)
p("SCOPE ART", "SPINNING CUBE", a=[10], lrate=0.1, lwave=2, lfo={"arot": 0.5}, **ART)
p("SCOPE ART", "BLOOMING ROSE", a=[7], **ART)
p("SCOPE ART", "STARFIELD", [("cos(t) + 0.5cos(4t + u)", "sin(t) - 0.5sin(4t + u)")], a=["c0"], lrate=0.05, lwave=2, lfo={"arot": 0.5}, **ART)
p("SCOPE ART", "DNA", [("sin(t)", "0.6sin(3t + u)")], a=["c0"], **ART)
p("SCOPE ART", "HEART TO CUBE", a=[1, 10], am=0.5, lrate=0.1, lwave=1, lfo={"am": 0.5}, **ART)
p("SCOPE ART", "ATOM", [("cos(t)cos(3t + u)", "sin(t)cos(3t + 1.3u)")], a=["c0"], **ART)
p("SCOPE ART", "SPIRO SPIN", a=[8], lrate=0.07, lwave=2, lfo={"arot": 0.5}, **ART)
p("SCOPE ART", "KALEIDOSCOPE", a=[0, 2, 3, 7, 8], am=0.5, lrate=0.05, lwave=1, lfo={"am": 0.5}, **ART)
p("SCOPE ART", "EYE", [("cos(t)", "0.5sin(t)^3 + 0.2cos(9t + 2u)")], a=["c0"], **ART)
p("SCOPE ART", "SPIRAL GALAXY", [("(t/(2pi))cos(8t + u)", "(t/(2pi))sin(8t + u)")], a=["c0"], lrate=0.05, lwave=2, lfo={"arot": 0.5}, **ART)
p("SCOPE ART", "DIAMOND", [DIAMOND], a=["c0"], lrate=0.15, lwave=1, lfo={"arot": 0.125}, **ART)

# ---------------------------------------------------------------- C++ out
SIMPLE = dict(am="kAShape", aoct="kAOct", asemi="kASemi", afine="kAFine", arot="kARot", alev="kALevel", avoi="kAVoices",
              bm="kBShape", boct="kBOct", bsemi="kBSemi", bfine="kBFine", brot="kBRot", blev="kBLevel", bvoi="kBVoices",
              atk="kAttack", dec="kDecay", sus="kSustain", rel="kRelease", lrate="kLfoRate", lwave="kLfoWave",
              poly="kPoly", leg="kLegato", glide="kGlide", vol="kVolume", ft="kFilterType", cut="kCutoff", res="kRes",
              sync="kLfoSync", div="kLfoDiv", draw="kLfoDraw", steps="kLfoSteps",
              sub="kSubLevel", suboct="kSubOct", noise="kNoiseLevel")
DESTS = dict(am="kAShape", asemi="kASemi", afine="kAFine", arot="kARot", alev="kALevel", bm="kBShape", bsemi="kBSemi",
             bfine="kBFine", brot="kBRot", blev="kBLevel", lrate="kLfoRate", vol="kVolume", cut="kCutoff", res="kRes",
             sub="kSubLevel", noise="kNoiseLevel")

def num(v):
    return repr(float(v)) + "f"

def shape(v):
    return 11 + int(v[1:]) if isinstance(v, str) else v

def entry(cat, name, cust, k):
    out = []
    for key, v in k.items():
        if key in ("a", "b"):
            slots, first = ("kASlots", "kASlot0") if key == "a" else ("kBSlots", "kBSlot0")
            if len(v) > 1: out.append("{%s, %d}" % (slots, len(v)))
            for i, s in enumerate(v): out.append("{%s, %d}" % (first if i == 0 else "%s + %d" % (first, i), shape(s)))
        elif key == "st":
            for i, s in enumerate(v): out.append("{kLfoStep0 + %d, %s}" % (i, num(s / 12)))
        elif key in ("env", "lfo"):
            for d, a in v.items(): out.append("{%s(%s), %s}" % (key, DESTS[d], num(a)))
        else:
            out.append("{%s, %s}" % (SIMPLE[key], num(v)))
    shapes = ", ".join('{"%s", "%s"}' % c for c in cust)
    return '        {"%s", "%s", {%s},\n         {%s}},' % (cat, name, ", ".join(out), shapes)

names = set()
for cat, name, cust, k in P:
    assert name not in names, name
    names.add(name)
here = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(here, "..", "Presets.hpp"), "w") as f:
    f.write('''// The factory presets. Made by tools/make_presets.py (edit the table there,
// then run it). Each one only lists what it changes from the defaults
// (Params.hpp); its typed-in shapes, if any, are numbered after the built-in
// ones and replace whatever shapes were typed before, like loading a preset
// on any synth replaces its wavetables.
#pragma once

#include "Params.hpp"
#include "Shapes.hpp"
#include <utility>
#include <vector>

namespace ss {

struct Preset {
    const char* category;
    const char* name;
    std::vector<std::pair<int, float>> set;
    std::vector<std::pair<const char*, const char*>> shapes;  // typed-in shapes: x, y

    // every parameter's value for this preset
    void values(float* out) const
    {
        for (int p = 0; p < kParamCount; ++p)
            out[p] = paramDefault(p);
        for (const auto& kv : set)
            out[kv.first] = kv.second;
    }
    // its typed-in shapes, in place of the ones there were
    void shapesInto(ShapeSet& s) const
    {
        s.customs.clear();
        for (const auto& xy : shapes) {
            CustomShape c;
            c.xs = xy.first;
            c.ys = xy.second;
            c.compile();
            c.build();
            s.customs.push_back(c);
        }
    }
};

inline int env(int p) { return modParam(kSrcEnv, destIndex(p)); }
inline int lfo(int p) { return modParam(kSrcLfo, destIndex(p)); }

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        {"", "INIT", {}, {}},
''')
    for e in P:
        f.write(entry(*e) + "\n")
    f.write('''    };
    return list;
}

} // namespace ss
''')
cats = []
for c, *_ in P:
    if c not in cats: cats.append(c)
print(len(P), "presets:", ", ".join("%s %d" % (c, sum(1 for x in P if x[0] == c)) for c in cats))
