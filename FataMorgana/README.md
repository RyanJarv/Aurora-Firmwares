# FataMorgana - 3D Wavetable Synthesizer

A dual-oscillator wavetable synthesizer for the Qu-Bit Aurora, inspired by the Industrial Music Electronics Piston Honda MKIII. Features 3D morphing across an 8×8×8 wave cube with thru-zero FM and multi-effect processing.

## Features

- **8×8×8 Wave Cube**: 512 waveforms arranged in a 3D morphable space
- **Thru-Zero FM**: Phase accumulator supports negative frequencies for classic TZFM sounds
- **Trilinear Interpolation**: Smooth morphing across X, Y, Z axes (can be disabled for glitchy transitions)
- **Dual Oscillators**: OSC A (Left Out) and OSC B (Right Out) with independent wavetable positions
- **Multi-Effect System**: 5 effect modes (FM, Ring Mod, Bitcrush, Wavefold, Overdrive)
- **Sub-Oscillators**: Wavetable sub-oscillators for OSC A and B add low-end weight (-1 octave)
- **CV-Controlled VCA**: CV_MIX input controls amplitude with soft clipping
- **External Input Mode**: Use as a nonlinear waveshaper for external audio
- **Unison Mode**: Detuned secondary oscillator for thicker sounds
- **Tone Degradation**: Orthodox (clean) or Degraded (12-bit with dithering) modes
- **Gate Randomization**: GATE_REVERSE triggers random XYZ positions
- **USB Wavetable Loading**: Load custom waveforms from USB drive

## Controls

| Control | Function |
|---------|----------|
| **Warp** (knob + CV) | Base frequency (V/Oct, 0V = C0 = 16.35Hz) |
| **Blur** (knob + CV) | Effect depth (mode-dependent) |
| **Time** (knob + CV) | X-axis morph position (0-7) for selected oscillator |
| **Reflect** (knob + CV) | Y-axis morph position (0-7) for selected oscillator |
| **Atmosphere** (knob + CV) | Z-axis morph position (0-7) for selected oscillator |
| **Mix** (knob) | Unison detune spread (0-5%) |
| **CV_MIX** | VCA control (0V = unity, +V = boost with soft clip, -V = attenuate) |

### Shift + Knob

| Control | Function |
|---------|----------|
| **Shift + Warp** | Osc B semitone offset (±24 semitones) |
| **Shift + Blur** | Effect mode select (5 modes) |
| **Shift + Time** | Sub A level (0-100%) |
| **Shift + Reflect** | Sub B level (0-100%) |
| **Shift + Mix** | Output volume trim (0-100%) |

## Effect Modes

Select with **Shift + Blur** knob. Effect depth controlled by **Blur** knob + CV.

| Mode | Effect | Description | LED Color |
|------|--------|-------------|-----------|
| 0 | **FM** | Cross-modulation between oscillators | Green |
| 1 | **Ring Mod** | Multiply A × B with wet/dry mix | Yellow |
| 2 | **Bitcrush** | Bit depth + sample rate reduction | Red |
| 3 | **Wavefold** | Wavefolding distortion | Magenta |
| 4 | **Overdrive** | Warm saturation/distortion | Orange |

## Buttons

| Button | Action |
|--------|--------|
| **Freeze** | Toggle morph interpolation (smooth ↔ glitchy) |
| **Reverse** (tap) | Cycle oscillator selection (A → B → Both) |
| **Reverse** (hold 5s) | Sync Osc B wavetable position to Osc A |
| **Shift + Freeze** | Toggle external input mode (waveshaper) |
| **Shift + Reverse** | Cycle tone mode (Orthodox ↔ Degraded) |

## Gate Inputs

| Gate | Function |
|------|----------|
| **GATE_FREEZE** | (Reserved for future use) |
| **GATE_REVERSE** | Randomize XYZ positions for both oscillators |

## Audio I/O

| Jack | Function |
|------|----------|
| **Left Out** | Oscillator A output |
| **Right Out** | Oscillator B output |
| **Left In** | FM input for OSC A (normalled to OSC B output in FM mode) |
| **Right In** | FM input for OSC B |

## LED Indicators

### Normal Mode
- **LED 1**: Osc A X position (red)
- **LED 2**: Osc B X position (red, dimmed when A selected)
- **LED 3**: Osc A Y position (green)
- **LED 4**: Osc B Y position (green, dimmed when A selected)
- **LED 5**: Osc A Z position (blue)
- **LED 6**: Osc B Z position (blue, dimmed when A selected)
- **LED_BOT_1**: Morph mode (green = smooth, red = glitchy)
- **LED_BOT_2**: Unison indicator (blue = on)
- **LED_BOT_3**: External mode (purple) / Tone mode (orange = degraded)
- **Freeze LED**: Morph state (green = smooth, red = glitchy) / Loading status
- **Reverse LED**: Oscillator selection (orange = A, cyan = B, purple = Both)

### Shift Mode
- **LED 1-2**: Sub A level (cyan gradient)
- **LED 3-4**: Effect mode indicator (color-coded)
- **LED 5-6**: Sub B level (magenta gradient)
- **LED_BOT_1**: Sub A active indicator
- **LED_BOT_2**: Sub B active indicator
- **LED_BOT_3**: Output trim level (brightness)

### Visual Feedback
- **White flash (all LEDs)**: Octave crossing on Warp knob
- **White flash (all LEDs)**: Gate randomization triggered
- **White flash (all LEDs)**: Osc B synced to Osc A

## USB Wavetable Format

Place 8 WAV files in the root of a USB drive:

```
1.wav  (Z=0)
2.wav  (Z=1)
3.wav  (Z=2)
4.wav  (Z=3)
5.wav  (Z=4)
6.wav  (Z=5)
7.wav  (Z=6)
8.wav  (Z=7)
```

Each file must be:
- **Format**: 16-bit mono PCM WAV (uncompressed)
- **Sample rate**: Any (44.1kHz, 48kHz, etc. - sample rate is ignored)
- **Samples per waveform**: 256
- **Waveforms per file**: 64 (arranged as 8×8 grid for X and Y axes)
- **Total samples**: 16,384 (256 × 64)
- **Minimum file size**: ~32KB of audio data

If no USB drive is present, a default harmonically-generated wave cube is used.

### Loading Error Codes (Freeze LED Color)

| LED Color | Error |
|-----------|-------|
| **Green** | Success - wavetables loaded |
| **Yellow** | Loading in progress |
| **Red** | File not found (missing 1.wav - 8.wav) |
| **Orange** | Not a valid WAV file |
| **Magenta** | Compressed audio (must be PCM) |
| **Blue** | Stereo file (must be mono) |
| **White** | Wrong bit depth (must be 16-bit) |
| **Cyan** | File too short (need 16,384 samples) |

### Creating Compatible Wavetables

Using Audacity or similar:
1. Create/import 64 single-cycle waveforms (256 samples each)
2. Concatenate them into one file (16,384 samples total)
3. Export as: **WAV (Microsoft) signed 16-bit PCM, Mono**
4. Name files 1.wav through 8.wav

## Building

```bash
cd Firmware/FataMorgana
make
```

Output: `build/FataMorgana.bin`

### Experimental RAM build

From the same directory, run `make ram` to build with `APP_TYPE=BOOT_SRAM`.
The RAM binary is written to `build-ram/FataMorgana.bin`. If the SDK is in a
separate checkout, pass `AURORA_SDK_PATH=/path/to/Aurora-SDK` to `make ram`.

Both build modes place the USB filesystem and file sector buffers in
DMA-accessible SRAM. To confirm that FataMorgana loaded the custom wavetables,
check for `Result: SUCCESS` in `wavetable_log.txt` on the USB drive.

The installation steps below apply to the default QSPI build.

## Installation

Copy `FataMorgana.bin` to a USB drive and insert into the Aurora module while holding the appropriate button combination (see Aurora manual for firmware update procedure).

## Architecture

```
Firmware/FataMorgana/
├── Makefile
├── README.md
├── src/
│   ├── FataMorgana.cpp      # Main firmware
│   └── wavetable/
│       ├── wave_cube.h      # Wave cube data structures and USB loading
│       └── oscillator.h     # Oscillator class with TZFM support
├── docs/
│   ├── CODE_REVIEW.md       # Code review findings
│   ├── IMPLEMENTATION_PLAN.md
│   └── SPEC_VCA_AND_EFFECTS.md
├── tests/
│   ├── wav_parser_test.cpp  # WAV parsing tests
│   └── dsp_test.cpp         # DSP function tests
└── build/
    └── FataMorgana.bin      # Compiled firmware
```

## Signal Flow

```
                    ┌─────────────────────────────────────────┐
                    │              WAVE CUBE                  │
                    │           (8×8×8 in SDRAM)              │
                    └─────────────────────────────────────────┘
                                      │
                    ┌─────────────────┴─────────────────┐
                    │                                   │
              ┌─────▼─────┐                       ┌─────▼─────┐
              │   OSC A   │◄──────────────────────│   OSC B   │
              │  (TZFM)   │    Cross-Mod (FM)     │  (TZFM)   │
              └─────┬─────┘                       └─────┬─────┘
                    │                                   │
              ┌─────▼─────┐                       ┌─────▼─────┐
              │  + SUB A  │                       │  + SUB B  │
              │ (-1 oct)  │                       │ (-1 oct)  │
              └─────┬─────┘                       └─────┬─────┘
                    │                                   │
                    └─────────────┬─────────────────────┘
                                  │
                           ┌──────▼──────┐
                           │   EFFECT    │
                           │ (FM/Ring/   │
                           │ Crush/Fold/ │
                           │  Overdrive) │
                           └──────┬──────┘
                                  │
                           ┌──────▼──────┐
                           │ Degradation │
                           │ (optional)  │
                           └──────┬──────┘
                                  │
                           ┌──────▼──────┐
                           │    VCA      │
                           │  (CV_MIX)   │
                           └──────┬──────┘
                                  │
                           ┌──────▼──────┐
                           │ Soft Clip   │
                           └──────┬──────┘
                                  │
                           ┌──────▼──────┐
                           │ Volume Trim │
                           │ (Shift+Mix) │
                           └──────┬──────┘
                                  │
                           ┌──────▼──────┐
                           │  DC Block   │
                           └──────┬──────┘
                                  │
                    ┌─────────────┴─────────────────────┐
                    │                                   │
                    ▼                                   ▼
               LEFT OUT                            RIGHT OUT


    GATE_REVERSE ──────► Randomize X/Y/Z (both oscillators)
```

## Sub-Oscillators

The sub-oscillators add low-end weight to compensate for the often thin sound of wavetable synthesis. Each oscillator (A and B) has an independent sub-oscillator that reads from the **same wavetable position** at half frequency (-1 octave).

**Key characteristics:**
- Sub follows the same X/Y/Z morph position as the main oscillator
- Sub uses the same wavetable waveform, preserving timbral coherence
- Sub runs at half frequency (one octave down)
- No FM applied to sub (clean low-end)
- No unison on sub (avoids muddiness)

**Usage tips:**
- Start with 20-40% sub level for subtle weight
- Higher levels (60-100%) create a more synth-bass character
- Morphing affects both main and sub together for cohesive sound
- Sub is most effective with harmonically rich wavetables

## Effect Modes Details

### FM (Mode 0)
Classic cross-modulation between oscillators. Osc B output modulates Osc A phase. External inputs override cross-mod when patched.

### Ring Mod (Mode 1)
Multiplies Osc A and Osc B outputs together, creating sum and difference frequencies. Blur controls wet/dry mix.

### Bitcrush (Mode 2)
Reduces bit depth (16 to 2 bits) and sample rate. Creates aliasing artifacts and lo-fi digital character. Uses DaisySP Bitcrush.

### Wavefold (Mode 3)
Folds the waveform back on itself when it exceeds thresholds, adding harmonics. Creates complex timbres from simple waves. Uses DaisySP Fold.

### Overdrive (Mode 4)
Warm saturation/distortion effect. Adds harmonics and compression. Uses DaisySP Overdrive (ported from Mutable Instruments Plaits).
