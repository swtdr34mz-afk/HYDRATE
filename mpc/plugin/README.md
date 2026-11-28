# HYDRATE for the MPC

The HYDRATE synth as a plugin for Akai MPC standalone hardware: the same engine as the HYDRATE VST3, built with Akai's
MPC Plugin SDK. A wave-morphing polysynth in the manner of the ASM Hydrasynth: three oscillators with 219 waves and
WaveScan, four Mutants, ring mod and noise, two filters, five envelopes, five LFOs, a 32-slot mod matrix, eight
macros, arp with phrases, effects, delay and reverb. 8 or 16 voices.

Unofficial: not made, endorsed or supported by Ashun Sound Machines.

## Installieren (kurz auf Deutsch)

1. MPC OS **3.10.1 oder neuer**. Auf der MPC: **Preferences → Enhancements (Beta) → Enable Enhancements** und
   **External Plugin Support** einschalten.
2. Den Ordner **`swtdr34mz - VST - HYDRATE`** (aus dem Download `HYDRATE-…-mpc-sdk.zip`) auf einen USB-Stick, eine
   SD-Karte oder das interne Laufwerk der MPC kopieren, in einen Ordner namens **`VST2 Plugins`** ganz oben auf dem
   Laufwerk: `VST2 Plugins/swtdr34mz - VST - HYDRATE/`.
3. Die MPC neu starten. HYDRATE steht dann im Plugin-Browser unter **External Plugins**.

Die MPC XL, Live III (64 Bit) und die 32-Bit-Geräte (X, X SE, Live, Live II, One, One+, Key 61, Key 37) nehmen
denselben Ordner; jedes Gerät lädt seine eigene Datei darin.

## Die Seiten

| Taste | Was drauf ist |
| --- | --- |
| **OSC** | OSC 1, 2, 3: die Welle im OLED (mit dem Finger ziehen), Semi, Cents, WaveScan, Keytrack, BitRedux; MIXER: Osc 1-3, Ring Mod, Noise, Filter Route |
| **MUTANT** | MUTANT 1-4: der Modus im OLED, Ratio, Depth, Feedback, Dry/Wet |
| **FILTER** | FILTER 1: Typ, Cutoff (groß), Resonance, Drive, Env 1, LFO 1, Keytrack, Vel Env, Formant/Ctrl; FILTER 2: Typ, Morph, Cutoff, Resonance, Env 1, Keytrack, LFO 1 |
| **ENV** | ENV 1 (Filter) und ENV 2 (Amp): Attack, Decay, Sustain, Release, Delay, Hold, Kurven / Velocity, Level |
| **MOD** | LFO 1 und 2: Welle, Rate, Level, Delay, Fade In; die acht MACROS; VOICES (8/16), QUALITY, INIT |
| **FX/ARP** | PRE-FX, DELAY, REVERB: Typ, je vier Regler; ARPEGGIATOR: Modus, Division, Octave, Gate, Swing, On, Latch |

Ein Knopf wird mit dem Finger gezogen oder mit seinem Q-Link gedreht; zweimal tippen öffnet den großen Regler der MPC
(bei einer Auswahl die Liste). Alle anderen Parameter (Mod-Matrix, ENV 3-5, LFO 3-5, Voice …) hat die MPC in ihrer
Parameterliste; die Werks-Sounds nutzen sie voll.

**VOICES / QUALITY**: 8 Stimmen und ECO sind voreingestellt – HYDRATE rechnet viel. 16 Stimmen oder HIGH nur, wenn
deine MPC Luft hat.

Die Programme: die 31 Werks-Sounds von HYDRATE.

## Licence

MPL-2.0. Fonts: Fira Sans and Anonymous Pro, SIL Open Font License.
