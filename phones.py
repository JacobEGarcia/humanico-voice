"""
phones.py - phoneme segment tables for the klatt80 engine.

Vowel formant targets follow published adult-male measurements
(Peterson & Barney, 1952). Durations and source amplitudes are
standard 1980-era formant-synthesis practice. Original table work
for this project; MIT license.
"""

# vowel targets: F1 F2 F3 (Hz); bandwidths B1 B2 B3
VOWELS = {
    'IY': (270, 2290, 3010,  60,  90, 150),
    'IH': (390, 1990, 2550,  65, 100, 160),
    'EH': (530, 1840, 2480,  70, 100, 160),
    'AE': (660, 1720, 2410,  80, 110, 170),
    'AA': (730, 1090, 2440,  85, 110, 170),
    'AO': (570,  840, 2410,  80, 110, 170),
    'UH': (440, 1020, 2240,  70, 100, 160),
    'UW': (300,  870, 2240,  60,  95, 150),
    'AH': (640, 1190, 2390,  80, 110, 170),
    'AX': (500, 1500, 2500,  75, 110, 170),
    'ER': (490, 1350, 1690,  70, 100, 140),
}
DIPHTHONGS = {   # start -> end vowel
    'EY': ('EH', 'IY'), 'AY': ('AA', 'IY'), 'OY': ('AO', 'IY'),
    'OW': ('AX', 'UH'), 'AW': ('AA', 'UH'),
}

AV_VOWEL = 0.92

# consonant specs ------------------------------------------------------------
STOPS = {  # closure_ms, burst {af, a2..a6, ab}, aspir_ms, aspir_amp, voiced
 'P': dict(closure=70, burst=dict(af=0.75, ab=0.45),            aspir=35, ah=0.55, voiced=False),
 'T': dict(closure=65, burst=dict(af=0.85, a4=0.55, a5=0.45),   aspir=35, ah=0.60, voiced=False),
 'K': dict(closure=70, burst=dict(af=0.80, a3=0.65),            aspir=35, ah=0.55, voiced=False),
 'B': dict(closure=55, burst=dict(af=0.45, ab=0.30),            aspir=12, ah=0.20, voiced=True),
 'D': dict(closure=50, burst=dict(af=0.55, a4=0.40, a5=0.30),   aspir=12, ah=0.20, voiced=True),
 'G': dict(closure=55, burst=dict(af=0.50, a3=0.45),            aspir=12, ah=0.20, voiced=True),
}
FRICATIVES = {  # dur_ms, params
 'S':  (115, dict(af=0.85, a5=0.55, a6=0.95, f6=4650.0, b6=380.0)),
 'Z':  ( 90, dict(af=0.60, a5=0.45, a6=0.75, f6=4650.0, b6=380.0, av=0.42)),
 'SH': (110, dict(af=0.80, a3=0.30, a4=0.80, a5=0.35, f4=2600.0, b4=350.0)),
 'ZH': ( 85, dict(af=0.55, a3=0.25, a4=0.60, a5=0.30, f4=2600.0, b4=350.0, av=0.42)),
 'F':  ( 95, dict(af=0.40, ab=0.50)),
 'V':  ( 75, dict(af=0.28, ab=0.38, av=0.48)),
 'TH': ( 85, dict(af=0.32, ab=0.42)),
 'DH': ( 70, dict(af=0.22, ab=0.32, av=0.50)),
 'HH': ( 70, dict(ah=0.60)),
}
NASALS = {  # dur_ms, f2, fnz
 'M':  ( 95, 1000.0, 1000.0),
 'N':  ( 90, 1700.0, 1900.0),
 'NG': (100, 2600.0, 2900.0),
}
LIQUIDS = {  # dur_ms, f1, f2, f3, b1, b2
 'L':  ( 85, 380.0, 1350.0, 2600.0, 130, 220),
 'R':  ( 85, 460.0, 1380.0, 1720.0, 110, 180),
}
GLIDES = {   # dur_ms, f1, f2, f3
 'W':  ( 65, 300.0,  610.0, 2200.0),
 'Y':  ( 65, 270.0, 2290.0, 3010.0),
}
AFFRICATES = { # -> closure spec, fricative phoneme
 'CH': ('T', 'SH'), 'JH': ('D', 'ZH'),
}

VOWEL_SET = set(VOWELS) | set(DIPHTHONGS)

def is_vowel(ph):
    return ph in VOWEL_SET
