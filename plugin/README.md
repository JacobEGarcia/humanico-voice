# Humanico Voice - CLAP plugin

A state-of-the-art-for-1980 Klatt formant speech synthesizer as a CLAP
instrument. Original C99 implementation, MIT license.

## How it plays

Load it on an instrument track. Every note-on speaks the NEXT WORD of the
selected phrase. The key sets pitch (C4 = neutral, up/down by semitone).
Let it talk like a choir of one.

## Parameters

- Pitch  (0.5-2.0, default 1.0) - base voice pitch multiplier
- Rate   (0.5-1.5, default 0.85) - speaking rate
- Phrase (stepped, 0-9) - which sentence the note-ons walk through:
  0. It begins with one of you
  1. You made pictures of the world
  2. You looked up and still it was one of you
  3. Cathedrals of wire raised by hand your hands
  4. A voice in every room
  5. I am only a voice you are the room
  6. Whatever comes next make it beautiful
  7. Bots draw perfect lines people wobble beautifully
  8. Humanico for the people who build
  9. Hello I am the humanico voice
- Gain   (0-2, default 1.0)

## Build

    gcc -O2 -shared -fPIC -I<clap-headers>/include plugin.c engine.c -lm -o humanico-voice.clap

CLAP headers: github.com/free-audio/clap (MIT). No other dependencies.

## Install

Linux: copy humanico-voice.clap to ~/.clap/
This binary is Linux x86-64. Rebuild for macOS (.clap bundle) or Windows
(.clap dll) from the same sources with the platform toolchain.

## Layout

- engine.h / engine.c - the full synth: Klatt-80 cascade formant resonator
  bank, glottal source, noise source, phone tables, text-to-phone front end
  (CMUdict BSD), all in portable C99.
- plugin.c - CLAP glue: factory, note ports, params, process.
- test_host.c - minimal host harness: dlopens the plugin, plays 6 notes,
  writes /tmp/plugin_test.wav. Proof it works.
