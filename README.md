# HUMANICO VOICE

A state-of-the-art-for-1980 speech synthesizer. An original implementation of
Dennis H. Klatt's cascade/parallel formant synthesizer (Klatt 1980, JASA 67(3),
971-995), written from the published paper. 10 kHz, 5 kHz of pure retro future.

No samples. No neural nets. No code copied from any existing synthesizer.
Phoneme targets derive from published acoustic-phonetic measurements
(Peterson & Barney 1952 adult-male vowels). Pronunciation dictionary:
CMUdict (BSD 2-clause, Carnegie Mellon University).

Inspired by klattsch (tgies.github.io/klattsch) and its not-yet-released
CLAP/VST3 plugin. This is our own engine - and our own plugin, in `plugin/`.

## Live playground

https://jacobegarcia.github.io/humanico-voice/

The same engine ported to JS, running fully in your browser
(`index.html` + `bundle.js`, dictionary embedded, no network calls).

## Python CLI

    python3 tts.py out.wav "It begins with one of you."

## CLAP plugin

`plugin/` is a working CLAP instrument: load it on a track and every
note-on speaks the next word of the selected phrase; the key sets pitch.
Original C99, no dependencies beyond the CLAP headers. Build:

    gcc -O2 -shared -fPIC -I<clap-headers>/include plugin.c engine.c -lm -o humanico-voice.clap

The included `humanico-voice.clap` binary is Linux x86-64; rebuild for
macOS/Windows from the same sources. `test_host.c` is a minimal host
harness that dlopens the plugin, plays six notes, and writes a WAV.

## Layout

- `klatt80.py` / `tts.py` / `phones.py` - the reference engine and CLI (Python)
- `klatt80.js` / `tts.js` / `bundle.js` - the JS port used by the playground
- `cmudict.dict` - CMU pronunciation dictionary (BSD 2-clause)
- `plugin/` - the CLAP instrument (C99)

## License

MIT (engine, front ends, and plugin). CMUdict retains its BSD 2-clause
license. All output audio is original synthesis - no licensing encumbrance.
