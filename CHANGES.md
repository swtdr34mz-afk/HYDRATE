# HYDRATE – changes

The newest first. When the version in CMakeLists.txt goes up and main builds, the release is made by itself with
the notes of that version from here.

## 1.2.2
After saving a sound of your own, the plugin now stays on it (before, Cubase, FL Studio and other VST3 hosts switched to another patch, so the new one had to be loaded again).

## 1.2.1
Ten new showcase sounds at the top of the list: Hydra Dawn, Glass Cathedral, Abyss Bass, Neon Cascade, Silk Orchestra,
Sync Viper, Vox Machina, Harmonic Tides, Brass Monument and Phase Keys. Each one has four macros, plays with mod wheel,
aftertouch and velocity, and has its own effects. All of them play at the same loudness.
MPC: at the start you now hear the sound whose name is shown (before, it said Liquid Glass but played INIT).

## 1.2.0
New: HYDRATE for the MPC (MPC XL, Live III and the 32-bit MPCs, OS 3.10.1 or newer). The same engine as the plugin,
six touch pages in the HYDRATE look (OSC, MUTANT, FILTER, ENV, MOD, FX/ARP) with the Q-Links on every page, the 31
factory patches as programs, 8 or 16 voices, ECO or HIGH.

## 1.1.5
New: HYDRATE for Linux - VST3, LV2 and a standalone app (64-bit, built on Ubuntu 22.04). Checked with pluginval like the Windows version.

## 1.1.4
The window opens as large as you left it - also after closing and reopening the DAW (before, it always came
back at the smallest size). A new instance starts at the size you used last.
New: HYDRATE for the Mac - VST3, AU and a standalone app (Apple Silicon and Intel).

## 1.1.3
User patches: SAVE fixed. The save window now opens inside the plugin (before, it could open hidden behind the
DAW's plugin window, so nothing got saved). If Windows keeps apps out of the Documents folder, patches are saved in
AppData/HYDRATE/Patches instead. The display shows "SAVED: name" (or "SAVE FAILED").
The Windows download now holds only the VST3 plugin (no standalone .exe), against false Windows Defender alarms.

## 1.1.2
RANDOM button fixed: every press gives a new, playable sound (SHIFT + RANDOM on a module page: only that module).
INIT, BROWSE and the value menus now open inside the plugin window.

## 1.1.1
Right click on any control opens your DAW's menu for it (MIDI Learn, automation); a right click never changes a value.

## 1.1.0
New VOICE SPREAD knob (stereo width, slight detune and filter variation per voice) and LOW SPLIT (play a key to set
the split: below it a mono bass voice, above it the patch). Main Systems section tidied up.

## 1.0.0
First release.
