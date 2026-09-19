"""
klatt80.py - an original implementation of Dennis H. Klatt's 1980
cascade/parallel formant speech synthesizer, as published in:

    Klatt, D. H. (1980). "Software for a cascade/parallel formant
    synthesizer." J. Acoust. Soc. Am. 67(3), 971-995.

Written from the published algorithm (resonator difference equations,
cascade/parallel topology, frame-based parameter updates). No code was
copied from any existing synthesizer. Phoneme acoustic targets derive
from published acoustic-phonetic measurements (Peterson & Barney 1952
adult-male vowel formants and standard references).

License: MIT (c) 2026 - Humanico Voice project.
"""
import math, random

FS = 10000               # classic 1980 sample rate: 10 kHz, 5 kHz bandwidth
FRAME_MS = 5             # parameter update rate
FRAME = FS * FRAME_MS // 1000

def _res_coeffs(f, bw, fs=FS):
    """2-pole resonator coefficients with unity DC gain (Klatt 1980 eq.)."""
    r = math.exp(-math.pi * bw / fs)
    c = -(r * r)
    b = 2.0 * r * math.cos(2.0 * math.pi * f / fs)
    a = 1.0 - b - c
    return a, b, c

def _antires_coeffs(f, bw, fs=FS):
    """Anti-resonator (spectral zero): exact inverse of the resonator."""
    a, b, c = _res_coeffs(f, bw, fs)
    a0 = 1.0 / a
    return a0, -b * a0, -c * a0

class Resonator:
    __slots__ = ("y1", "y2")
    def __init__(self):
        self.y1 = 0.0; self.y2 = 0.0
    def run(self, x, a, b, c):
        y = a * x + b * self.y1 + c * self.y2
        self.y2 = self.y1; self.y1 = y
        return y

class AntiResonator:
    """Spectral zero: y[n] = a*x[n] + b*x[n-1] + c*x[n-2] (feed-forward)."""
    __slots__ = ("x1", "x2")
    def __init__(self):
        self.x1 = 0.0; self.x2 = 0.0
    def run(self, x, a, b, c):
        y = a * x + b * self.x1 + c * self.x2
        self.x2 = self.x1; self.x1 = x
        return y

class Voice:
    """One synthesis run. Parameters are plain attributes updated per frame."""
    def __init__(self, fs=FS):
        self.fs = fs
        # source
        self.f0 = 100.0          # Hz
        self.av = 0.0            # voicing amplitude
        self.ah = 0.0            # aspiration amplitude (noise into cascade)
        self.af = 0.0            # frication amplitude (noise into parallel)
        # cascade formants
        self.f1, self.b1 = 500.0, 60.0
        self.f2, self.b2 = 1500.0, 90.0
        self.f3, self.b3 = 2500.0, 150.0
        self.f4, self.b4 = 3900.0, 250.0
        self.f5, self.b5 = 4500.0, 300.0
        # nasal pole / zero
        self.fnp, self.bnp = 270.0, 270.0
        self.fnz, self.bnz = 270.0, 270.0   # == fnp -> cancels for non-nasals
        # parallel branch (frication)
        self.a2, self.a3, self.a4, self.a5, self.a6, self.ab = 0,0,0,0,0,0
        self.f6, self.b6 = 4800.0, 400.0
        self.b2f, self.b3f, self.b4f, self.b5f, self.b6f = 250.0, 300.0, 400.0, 500.0, 600.0
        # glottal source shaping
        self.bgl = 1200.0        # glottal low-pass bandwidth
        self.bgl2 = 100000.0     # second-stage lowpass (off by default)
        # state
        self._rnp = Resonator(); self._rnz = AntiResonator()
        self._r = [Resonator() for _ in range(5)]
        self._p = [Resonator() for _ in range(6)]   # parallel amps a1? use a2..a6 in slots 1..5
        self._rg = Resonator()   # glottal lowpass
        self._rg2 = Resonator()  # second lowpass: steeper glottal rolloff
        self._t0 = 1.0
        self._noise = random.Random(1234)
        self._prev_y = 0.0
        self._radiate = True

    def frame(self, n):
        """Synthesize n samples with current parameters; returns list[float]."""
        fs = self.fs
        rg = _res_coeffs(0.0, self.bgl, fs)
        rg2 = _res_coeffs(0.0, self.bgl2, fs)
        rnp = _res_coeffs(self.fnp, self.bnp, fs)
        rnz = _antires_coeffs(self.fnz, self.bnz, fs)
        rc = [_res_coeffs(f, b, fs) for f, b in (
            (self.f1, self.b1), (self.f2, self.b2), (self.f3, self.b3),
            (self.f4, self.b4), (self.f5, self.b5))]
        rp = [
            _res_coeffs(self.f2, self.b2f, fs), _res_coeffs(self.f3, self.b3f, fs),
            _res_coeffs(self.f4, self.b4f, fs), _res_coeffs(self.f5, self.b5f, fs),
            _res_coeffs(self.f6, self.b6f, fs)]
        pa = (self.a2, self.a3, self.a4, self.a5, self.a6)
        out = []
        rng = self._noise
        av, ah, af, ab = self.av, self.ah, self.af, self.ab
        f0 = self.f0
        for _ in range(n):
            # ---- voicing source: impulse train through glottal low-pass ----
            self._t0 += 1.0
            period = fs / max(f0, 20.0)
            glot = 0.0
            if self._t0 >= period:
                self._t0 -= period
                glot = 1.0
            if av > 0:
                voice_src = self._rg2.run(self._rg.run(glot, *rg), *rg2) * av
            else:
                voice_src = 0.0
            # ---- aspiration + voice share the cascade ----
            noise = rng.uniform(-1.0, 1.0)
            casc_in = voice_src + noise * ah
            x = self._rnp.run(casc_in, *rnp)
            x = self._rnz.run(x, *rnz)
            for i in range(5):
                x = self._r[i].run(x, *rc[i])
            cascade = x
            # ---- frication through the parallel branch ----
            fr = noise * af
            par = ab * fr
            for i in range(5):
                if pa[i] > 0:
                    par += self._p[i].run(fr, *rp[i]) * pa[i]
            y = cascade + par
            out.append(y - self._prev_y if self._radiate else y)
            self._prev_y = y
        return out

def synthesize(track, fs=FS):
    """
    track: list of frames; each frame is a dict of Voice parameter updates.
    Returns a float list of samples.
    """
    v = Voice(fs)
    pcm = []
    for fr in track:
        for k, val in fr.items():
            setattr(v, k, val)
        pcm.extend(v.frame(FRAME))
    return pcm
