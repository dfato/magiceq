# Style curves: first drafts for your ears

**Status: every number on this page is a guess.** They come from general vocal-mixing practice,
not from measuring reference records yet. Detect is only as good as these curves, so they need your
ear before anyone else hears MagicEQ.

## How a style works

Each style is a *target tone curve*: what the long-term average spectrum of a finished vocal in
that genre should look like. It is built in two layers:

1. **A shared "polished vocal" baseline** (in `Source/core/StyleLibrary.cpp`). Measured as energy
   per 1/6 octave, it is roughly flat from 180 to 500 Hz, then falls about 3 dB per octave
   up to 4 kHz and faster above 8 kHz.
2. **Per-style moves on top**, listed below. "+2 at 5 kHz" means "this genre wants about 2 dB more
   energy around 5 kHz than the baseline". Values between points blend smoothly.

When you press Detect, MagicEQ measures your vocal, lines it up with the target, and places EQ bands
to close the gap, scaled by **Strength** (75% by default) and limited per style.

The compressor column is the style's starting point. Detect then adapts it to your performance:
the threshold follows your level, and the ratio and amount go up for a very dynamic vocal and down
for one that is already even. The "GR" figure is roughly how much the loud lines get turned down
at 100% Strength.

## The 21 draft styles

| Genre | Style | Character | Low cut | Tone moves vs. baseline (dB) | Compressor (ratio, attack / release, target GR) |
|---|---|---|---|---|---|
| Pop | **Modern Pop** | Bright, forward and polished with controlled lows and lifted air. | 90 Hz | 250 Hz -1.5, 400 Hz -1, 1 kHz +0, 3 kHz +1.5, 5 kHz +2, 10 kHz +3, 14 kHz +3 | 4.0:1, 5 / 60 ms, ~6 dB GR |
| Pop | **Bedroom Pop** | Soft and intimate, warm body with gentle sparkle. | 80 Hz | 150 Hz +1, 300 Hz +0, 2.5 kHz -0.5, 6 kHz +1, 12 kHz +2 | 3.0:1, 15 / 120 ms, ~4 dB GR |
| Pop | **Dance Pop** | Thin low mids, strong presence to cut through dense tracks. | 110 Hz | 300 Hz -2, 2.5 kHz +2, 5 kHz +2.5, 12 kHz +3.5 | 5.0:1, 3 / 50 ms, ~8 dB GR |
| Hip-Hop | **Trap** | Clear and crisp on top of heavy 808s, low mids tucked away. | 100 Hz | 200 Hz +0.5, 400 Hz -2, 800 Hz -1, 3 kHz +2, 6 kHz +3, 12 kHz +4 | 6.0:1, 2 / 40 ms, ~8 dB GR |
| Hip-Hop | **Boom Bap** | Chesty and punchy with a slightly darker top. | 90 Hz | 150 Hz +1.5, 300 Hz +0.5, 1 kHz +1, 2.5 kHz +1.5, 8 kHz +0, 12 kHz -1 | 4.0:1, 10 / 80 ms, ~6 dB GR |
| Hip-Hop | **Melodic Rap** | Smooth and present, airy on the sung parts. | 100 Hz | 300 Hz -1.5, 3 kHz +1.5, 8 kHz +2.5, 12 kHz +3 | 4.0:1, 5 / 60 ms, ~6 dB GR |
| Hip-Hop | **Drill** | Aggressive upper mids, tight and controlled. | 110 Hz | 250 Hz -1, 500 Hz -1.5, 1.5 kHz +1, 4 kHz +3, 8 kHz +2, 12 kHz +2 | 6.0:1, 1 / 40 ms, ~9 dB GR |
| R&B | **Contemporary R&B** | Silky top, warm low end, scooped boxiness. | 80 Hz | 150 Hz +1, 300 Hz -1, 1 kHz +0, 3 kHz +1, 6 kHz +1.5, 12 kHz +3.5 | 3.0:1, 10 / 100 ms, ~5 dB GR |
| R&B | **Neo-Soul** | Warm, round and natural, little extra top. | 70 Hz | 150 Hz +1.5, 300 Hz +0.5, 800 Hz +0.5, 3 kHz +0, 6 kHz -0.5, 10 kHz +1 | 3.0:1, 20 / 150 ms, ~4 dB GR |
| Rock | **Alt Rock** | Mid-forward and gritty so it sits inside guitars. | 110 Hz | 200 Hz -0.5, 500 Hz +0.5, 1.2 kHz +1.5, 2.5 kHz +2, 5 kHz +1, 12 kHz +1 | 5.0:1, 8 / 80 ms, ~7 dB GR |
| Rock | **Pop Punk** | Bright, upfront and tightly compressed. | 120 Hz | 300 Hz -1.5, 1.5 kHz +1.5, 3 kHz +2.5, 6 kHz +1.5, 12 kHz +2 | 6.0:1, 3 / 50 ms, ~9 dB GR |
| Rock | **Indie** | Natural and a little lo-fi, mild presence and air. | 90 Hz | 200 Hz +0.5, 400 Hz +0, 1 kHz +1, 3 kHz +0.5, 8 kHz +1, 12 kHz +1.5 | 3.0:1, 15 / 120 ms, ~4 dB GR |
| Country | **Modern Country** | Clear storytelling vocal, present and shiny. | 100 Hz | 250 Hz -1, 1 kHz +0.5, 3 kHz +2, 6 kHz +2, 12 kHz +2.5 | 4.0:1, 8 / 80 ms, ~6 dB GR |
| Country | **Folk / Acoustic** | Honest and open, light touch. | 75 Hz | 150 Hz +1, 400 Hz +0, 2 kHz +0.5, 5 kHz +0.5, 10 kHz +1 | 2.5:1, 20 / 150 ms, ~3 dB GR |
| Gospel | **Gospel / Worship** | Big and full, with room for dynamic peaks. | 70 Hz | 150 Hz +1, 350 Hz -0.5, 1 kHz +0.5, 3 kHz +1.5, 8 kHz +1.5, 12 kHz +2.5 | 3.0:1, 15 / 120 ms, ~5 dB GR |
| World | **Afrobeats** | Smooth, bright and relaxed, clean low mids. | 100 Hz | 300 Hz -1.5, 2 kHz +1, 5 kHz +2, 10 kHz +3, 14 kHz +3 | 4.0:1, 5 / 60 ms, ~6 dB GR |
| World | **Reggaeton / Latin** | Upfront and crisp, tight low mids. | 110 Hz | 300 Hz -2, 1.5 kHz +1, 4 kHz +2.5, 10 kHz +3 | 5.0:1, 3 / 50 ms, ~7 dB GR |
| Electronic | **EDM Topline** | Thin, bright and very present to ride over big drops. | 130 Hz | 250 Hz -2.5, 500 Hz -1, 3 kHz +2, 6 kHz +3, 12 kHz +4.5 | 6.0:1, 2 / 40 ms, ~9 dB GR |
| Electronic | **Lo-Fi** | Warm and dark, rolled-off top. | 100 Hz | 120 Hz +1, 300 Hz +1, 2 kHz -1, 5 kHz -3, 10 kHz -6, 14 kHz -8 | 4.0:1, 20 / 150 ms, ~6 dB GR |
| Spoken | **Podcast / Spoken** | Clear, intelligible speech with gentle warmth. | 80 Hz | 120 Hz +1, 300 Hz -1, 2.5 kHz +2, 5 kHz +1, 10 kHz -0.5, 14 kHz -1 | 3.0:1, 10 / 150 ms, ~6 dB GR |
| Spoken | **Voiceover / Broadcast** | Close, deep and authoritative. | 70 Hz | 120 Hz +2, 250 Hz -0.5, 3 kHz +2.5, 6 kHz +1.5, 12 kHz +1 | 4.0:1, 5 / 100 ms, ~8 dB GR |

## How to review them

The fastest way to tune these is by ear, in your DAW:

1. Load MagicEQ on a vocal you know well. Pick a style, press **Detect**, and A/B against the vocal
   with MagicEQ bypassed.
2. Ask: does it move the vocal toward records in that genre? Too bright, too thin, too boxy?
3. Tell me in plain words, for example "Trap is too harsh around 4k" or "R&B needs more low-end
   warmth", and I will adjust the numbers.

A more precise option for later: send me a few reference tracks per genre (or acapellas) and I can
measure their vocal spectra and build the curves from real data.

## Other guesses worth checking

- **Default Strength of 75%.** Detect moves three quarters of the way toward the target, to fine-tune
  rather than overhaul. Easy to change.
- **Detect uses at most 7 tone bands plus a low cut**, leaving 4 bands free for your own moves.
- **Maximum boost and cut per style** are 4 to 7 dB and 6 to 9 dB. Detect never goes further.
- **Compressor sits after the EQ**, the usual vocal chain order. A pre/post switch is easy to add.
- **Detector style.** The compressor reacts to peaks (fast and transparent for vocals) with a 6 dB
  soft knee by default. Some engineers prefer an RMS-style, slower feel for certain genres.
- **Top-end safety.** If a vocal has almost nothing above 8 kHz (a dull mic, or heavy de-essing),
  Detect limits its air boost to 3 dB so it does not just raise hiss.
