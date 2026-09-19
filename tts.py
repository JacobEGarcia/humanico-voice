"""
tts.py - text -> klatt80 parameter frames -> PCM.

Front end: CMUdict (BSD 2-clause, (c) Carnegie Mellon University) for
pronunciation, simple OOV letter rules, digit expansion, and a small
declination/accent prosody model in the 1980 formant-synth style.
MIT license - Humanico Voice project.
"""
import re, math, wave, struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import klatt80
from phones import (VOWELS, DIPHTHONGS, VOWEL_SET, AV_VOWEL, STOPS, FRICATIVES,
                    NASALS, LIQUIDS, GLIDES, AFFRICATES)

FS = klatt80.FS
FRAME_MS = klatt80.FRAME_MS

# ---------------------------------------------------------------- dictionary
def load_cmudict(path):
    d = {}
    with open(path, encoding='latin-1') as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith(';;;'):
                continue
            parts = line.split()
            word = re.sub(r'\(\d+\)$', '', parts[0]).lower()
            if word not in d:
                d[word] = parts[1:]
    return d

_DICT = None
def cmu():
    global _DICT
    if _DICT is None:
        _DICT = load_cmudict(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cmudict.dict'))
    return _DICT

DIGITS = {'0':'zero','1':'one','2':'two','3':'three','4':'four','5':'five',
          '6':'six','7':'seven','8':'eight','9':'nine'}

# crude letter-to-sound fallback for out-of-vocabulary words
LTS = {'a':'AH','b':'B','c':'K','d':'D','e':'EH','f':'F','g':'G','h':'HH',
       'i':'IH','j':'JH','k':'K','l':'L','m':'M','n':'N','o':'AA','p':'P',
       'q':'K','r':'R','s':'S','t':'T','u':'AH','v':'V','w':'W','x':'K S',
       'y':'Y','z':'Z'}

def word_to_phones(word):
    """-> list of (phone, stress) where stress in {0,1,2}; vowels only."""
    w = word.lower()
    if w.isdigit():
        out = []
        for ch in w:
            out += word_to_phones(DIGITS[ch])
        return out
    entry = cmu().get(w)
    if entry is None:
        entry = []
        for ch in w:
            if ch in LTS:
                entry += LTS[ch].split()
        if not entry:
            return []
    out = []
    for p in entry:
        m = re.match(r'^([A-Z]+)([012])?$', p)
        ph, stress = m.group(1), int(m.group(2) or 0)
        if ph == 'AXR':
            ph, stress = 'ER', stress
        if ph in VOWEL_SET:
            out.append((ph, stress))
        else:
            out.append((ph, -1))
    return out


# ------------------------------------------------------------- auto-calibration
# Balance source amplitudes so every segment lands at a target loudness
# relative to a reference vowel, whatever the resonator gains do.
_RMS_CACHE = {}

def _unit_rms(params, source):
    """Steady-state RMS with `source` amplitude = 1, others 0."""
    key = (source, tuple(sorted((k, round(v, 1)) for k, v in params.items()
                                if k != source and isinstance(v, (int, float)))))
    if key in _RMS_CACHE:
        return _RMS_CACHE[key]
    v = klatt80.Voice()
    for k, val in params.items():
        setattr(v, k, val)
    for s in ('av', 'ah', 'af'):
        setattr(v, s, 0.0)
    setattr(v, source, 1.0)
    if not params.get('f0'):
        v.f0 = 110.0
    pcm = []
    for _ in range(70):
        pcm.extend(v.frame(klatt80.FRAME))
    import numpy as _np
    x = _np.array(pcm[1500:])
    rms = float(_np.sqrt((x ** 2).mean())) + 1e-9
    _RMS_CACHE[key] = rms
    return rms

_VREF = {}

def _vowel_ref():
    if 'r' not in _VREF:
        _VREF['r'] = _unit_rms(dict(f1=730.0, b1=85.0, f2=1090.0, b2=110.0,
                                    f3=2440.0, b3=170.0), 'av')
    return _VREF['r']

# desired levels relative to reference vowel (dB)
LEVEL = dict(vowel=0.0, nasal=-4.0, liquid=-4.0, glide=-2.0,
             fric=-7.0, fric_weak=-10.0, burst=-5.0, aspir=-12.0,
             voicebar=-20.0, fric_voice=-16.0)

def _set_level(target, source, kind):
    db = LEVEL[kind]
    desired = _vowel_ref() * (10 ** (db / 20.0))
    unit = _unit_rms(target, source)
    target[source] = desired / unit

def balance(target, kind):
    t = dict(target)
    if kind == 'vowel':
        _set_level(t, 'av', 'vowel')
    elif kind in ('nasal', 'liquid'):
        _set_level(t, 'av', kind)
    elif kind == 'glide':
        _set_level(t, 'av', 'glide')
    elif kind == 'fric':
        _set_level(t, 'af', 'fric_weak' if t.get('ab') and not (t.get('a4') or t.get('a5') or t.get('a6')) else 'fric')
        if t.get('av'):
            _set_level(t, 'av', 'fric_voice')
    elif kind == 'burst':
        _set_level(t, 'af', 'burst')
    elif kind == 'aspir':
        _set_level(t, 'ah', 'aspir')
    elif kind == 'voicebar':
        _set_level(t, 'av', 'voicebar')
    return t

# ------------------------------------------------------------------- segments
SIL = dict(av=0.0, ah=0.0, af=0.0, ab=0.0, a2=0.0, a3=0.0, a4=0.0, a5=0.0, a6=0.0)

def tgt(**kw):
    t = dict(SIL); t.update(kw); return t

def vowel_tgt(v, av=AV_VOWEL):
    f1, f2, f3, b1, b2, b3 = VOWELS[v]
    return tgt(av=av, f1=f1, f2=f2, f3=f3, b1=b1, b2=b2, b3=b3)

def phones_to_segments(phseq):
    """phseq: list of (phone, stress). -> list of dicts:
       {target: {...}, dur_ms, trans_ms, kind, stress}"""
    segs = []
    n = len(phseq)
    for i, (ph, stress) in enumerate(phseq):
        nxt = phseq[i + 1][0] if i + 1 < n else None
        stressed = stress == 1
        if ph in VOWELS:
            dur = 150 if stressed else (100 if stress == 2 else 85)
            segs.append(dict(target=balance(vowel_tgt(ph), 'vowel'), dur_ms=dur, trans_ms=50,
                             kind='vowel', stress=stress))
        elif ph in DIPHTHONGS:
            a, b = DIPHTHONGS[ph]
            dur = 165 if stressed else 115
            t1 = balance(vowel_tgt(a), 'vowel'); t2 = balance(vowel_tgt(b), 'vowel')
            segs.append(dict(target=t1, dur_ms=int(dur * 0.45), trans_ms=55,
                             kind='vowel', stress=stress))
            segs.append(dict(target=t2, dur_ms=int(dur * 0.55), trans_ms=int(dur * 0.5),
                             kind='vowel', stress=stress))
        elif ph in STOPS:
            sp = STOPS[ph]
            closure = dict(SIL)
            if sp['voiced']:
                closure.update(av=0.18, f1=200.0, b1=250.0, f2=1000.0, f3=2200.0)
            bt = 'voicebar' if sp['voiced'] else None
            if bt:
                closure = balance(tgt(**closure), 'voicebar')
            segs.append(dict(target=tgt(**closure), dur_ms=sp['closure'], trans_ms=3,
                             kind='sil', stress=-1))
            bp = dict(af=sp['burst'].get('af', 0.6))
            for k in ('a2', 'a3', 'a4', 'a5', 'a6', 'ab'):
                if k in sp['burst']:
                    bp[k] = sp['burst'][k]
            segs.append(dict(target=balance(tgt(**bp), 'burst'), dur_ms=14, trans_ms=3,
                             kind='burst', stress=-1))
            if sp['aspir'] > 0:
                ah = dict(ah=sp['ah'])
                if nxt in VOWEL_SET:
                    v = vowel_tgt(DIPHTHONGS.get(nxt, (nxt,))[0] if nxt in DIPHTHONGS else nxt)
                    ah.update(f1=v['f1'], f2=v['f2'], f3=v['f3'])
                segs.append(dict(target=balance(tgt(**ah), 'aspir'), dur_ms=sp['aspir'], trans_ms=12,
                                 kind='aspir', stress=-1))
        elif ph in AFFRICATES:
            st, fr = AFFRICATES[ph]
            sub = phones_to_segments([(st, -1), (fr, -1)])
            sub[0]['dur_ms'] = int(sub[0]['dur_ms'] * 0.7)
            sub = [s for s in sub if s['kind'] != 'aspir']
            segs += sub
        elif ph in FRICATIVES:
            dur, prm = FRICATIVES[ph]
            t = tgt(**{k: v for k, v in prm.items()})
            if ph == 'HH' and nxt in VOWEL_SET:
                v = vowel_tgt(DIPHTHONGS.get(nxt, (nxt,))[0] if nxt in DIPHTHONGS else nxt)
                t.update(f1=v['f1'], f2=v['f2'], f3=v['f3'])
            segs.append(dict(target=balance(t, 'fric'), dur_ms=dur, trans_ms=20, kind='fric', stress=-1))
        elif ph in NASALS:
            dur, f2, fnz = NASALS[ph]
            segs.append(dict(target=balance(tgt(av=0.75, f1=270.0, b1=250.0, f2=f2, b2=200.0,
                                        f3=2400.0, b3=250.0, fnz=fnz, bnz=250.0), 'nasal'),
                             dur_ms=dur, trans_ms=35, kind='nasal', stress=-1))
        elif ph in LIQUIDS:
            dur, f1, f2, f3, b1, b2 = LIQUIDS[ph]
            segs.append(dict(target=balance(tgt(av=0.72, f1=f1, f2=f2, f3=f3, b1=b1, b2=b2, b3=200.0), 'liquid'),
                             dur_ms=dur, trans_ms=45, kind='liquid', stress=-1))
        elif ph in GLIDES:
            dur, f1, f2, f3 = GLIDES[ph]
            segs.append(dict(target=balance(tgt(av=0.80, f1=f1, f2=f2, f3=f3, b1=90.0, b2=130.0, b3=180.0), 'glide'),
                             dur_ms=dur, trans_ms=45, kind='glide', stress=-1))
    return segs

# ------------------------------------------------------------------- prosody
PAUSE = {',': 200, ';': 220, ':': 220, '-': 160, '.': 340, '!': 340, '?': 340}

def tokenize(text):
    text = text.replace('\u2014', ' - ').replace('\u2013', ' - ').replace('\u2026', '...')
    raw = re.findall(r"[A-Za-z0-9']+|[.,!?;:\-]", text)
    out = []
    for r in raw:
        if r in '.,!?;:-':
            out.append(('punct', r))
        else:
            out.append(('word', r.strip("'")))
    return out

def f0_track(sent_words, total_ms):
    """Declination contour with accent bumps; sent_words = list of word dicts."""
    f0 = []
    for wd in sent_words:
        base0, base1 = wd['f0a'], wd['f0b']
        for seg in wd['segs']:
            n = max(1, int(seg['dur_ms'] / FRAME_MS))
            for k in range(n):
                frac = k / max(1, n - 1)
                f = base0 + (base1 - base0) * frac
                if seg.get('stress') == 1 and seg['kind'] == 'vowel':
                    bump = math.sin(math.pi * min(1.0, frac * 1.4))
                    f *= 1.0 + 0.14 * bump
                if wd.get('final') and seg['kind'] == 'vowel':
                    f *= 1.0 - 0.13 * frac * frac
                f0.append(f)
    return f0

def utterance_frames(text, rate=1.0, pitch=1.0):
    """text -> (frames, total_ms). Frames carry klatt80 params incl. f0."""
    tokens = tokenize(text)
    sentences, cur = [], []
    for kind, val in tokens:
        if kind == 'punct':
            if cur:
                cur[-1]['pause_after'] = PAUSE.get(val, 200)
                if val in '.!?':
                    sentences.append(cur); cur = []
                else:
                    cur[-1]['comma'] = True
        elif val:
            ph = word_to_phones(val)
            if ph:
                cur.append(dict(word=val, phones=ph, pause_after=0, comma=False))
    if cur:
        sentences.append(cur)

    frames = []
    for sent in sentences:
        # collect segments per word first (for durations + total length)
        total_ms = 0
        for wi, wd in enumerate(sent):
            segs = phones_to_segments(wd['phones'])
            # phrase-final lengthening
            if wi == len(sent) - 1:
                for s in reversed(segs):
                    if s['kind'] in ('vowel', 'nasal', 'liquid'):
                        s['dur_ms'] = int(s['dur_ms'] * 1.55)
                        break
                wd['final'] = True
            wd['segs'] = segs
            wd['dur_ms'] = sum(s['dur_ms'] for s in segs)
            total_ms += wd['dur_ms'] + wd['pause_after']
        # declination endpoints
        f_start, f_end = 118.0 * pitch, 84.0 * pitch
        t = 0.0
        for wd in sent:
            a = f_start + (f_end - f_start) * (t / max(1, total_ms))
            t += wd['dur_ms']
            b = f_start + (f_end - f_start) * (t / max(1, total_ms))
            wd['f0a'], wd['f0b'] = a, b
            t += wd['pause_after']

        # render segments + f0
        f0s = f0_track(sent, total_ms)
        fi = 0
        prev = None
        for wd in sent:
            for seg in wd['segs']:
                dur = seg['dur_ms'] / rate
                trans = min(seg['trans_ms'], dur * 0.6)
                n_frames = max(1, round(dur / FRAME_MS))
                n_trans = max(0, round(trans / FRAME_MS))
                tgt_params = seg['target']
                if prev is None:
                    prev = tgt_params
                for k in range(n_frames):
                    fr = dict(tgt_params)
                    if k < n_trans and prev is not tgt_params:
                        mix = (k + 1) / (n_trans + 1)
                        for key in set(prev) | set(tgt_params):
                            p0 = prev.get(key, 0.0); p1 = tgt_params.get(key, 0.0)
                            fr[key] = p0 + (p1 - p0) * mix
                    if fi < len(f0s):
                        fr['f0'] = f0s[fi]
                    fi += 1
                    frames.append(fr)
                prev = tgt_params
            # word pause
            if wd['pause_after']:
                for _ in range(round(wd['pause_after'] / rate / FRAME_MS)):
                    frames.append(dict(SIL, f0=95.0 * pitch))
                prev = None
    return frames

# ------------------------------------------------------------------- rendering
def render(text, rate=1.0, pitch=1.0):
    frames = utterance_frames(text, rate=rate, pitch=pitch)
    pcm = klatt80.synthesize(frames)
    nf = int(0.008 * FS)
    for i in range(min(nf, len(pcm))):
        pcm[i] *= i / nf
        pcm[-1 - i] *= i / nf
    peak = max(1e-6, max(abs(x) for x in pcm))
    g = 0.89 / peak
    return [max(-1.0, min(1.0, x * g)) for x in pcm]

def write_wav(path, pcm, fs=FS):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(fs)
        w.writeframes(b''.join(struct.pack('<h', int(x * 32767)) for x in pcm))

if __name__ == '__main__':
    text = ' '.join(sys.argv[2:]) if len(sys.argv) > 2 else 'It begins with one of us.'
    pcm = render(text)
    write_wav(sys.argv[1], pcm)
    print(f'{sys.argv[1]}: {len(pcm)/FS:.2f}s')
