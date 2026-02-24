# Tempest User Acceptance Testing Protocol

Live testing protocol for validating Tempest firmware on Aurora hardware.

**Reference:** [Distortion Topologies](distortion_topologies.md)

---

## Test Setup

### Equipment Required

| Item | Purpose |
|------|---------|
| Aurora module | Device under test |
| Oscilloscope | Waveform verification |
| VCO module | Test signal source |
| Audio interface | Recording/analysis |
| Headphones/monitors | Listening tests |

### Test Signals

| Signal | Frequency | Use |
|--------|-----------|-----|
| Sine | 100 Hz | Harmonic analysis, low frequency |
| Sine | 1 kHz | General testing |
| Sine | 5 kHz | Aliasing detection |
| Triangle | 200 Hz | Wavefolder response |
| Sawtooth | 200 Hz | Filter/saturation tests |
| Percussive | Envelope + VCO | Transient/envelope tests |

---

## Test Matrix

### By Topology

Each topology has specific behaviors to verify. Tests are grouped by the topology they validate.

---

## Test 1: Soft Clipping Verification

**Topology:** A (Soft Clipping)  
**Algorithms:** SftClp, Valve, Cubic, StrgrC

### 1.1 Gradual Onset

**Procedure:**
1. Input 100 Hz sine at moderate level
2. Start with Drive at minimum
3. Slowly increase Drive to 50x
4. Observe waveform on oscilloscope

**Expected:**
- Peaks round off gradually
- No abrupt transitions
- Waveform remains smooth

**Pass:** ☐ | **Fail:** ☐

### 1.2 Harmonic Development

**Procedure:**
1. Input 100 Hz sine
2. Set Drive to 10x
3. Listen to harmonic content

**Expected:**
- Predominantly low-order harmonics
- Warm, musical character
- No harsh artifacts

**Pass:** ☐ | **Fail:** ☐

### 1.3 Asymmetry Control

**Procedure:**
1. Input 100 Hz sine, Drive at 10x
2. Set BIAS to center, observe symmetry
3. Move BIAS left, observe waveform
4. Move BIAS right, observe waveform

**Expected:**
- Center: symmetric waveform
- Off-center: one half-cycle more compressed

**Pass:** ☐ | **Fail:** ☐

### 1.4 MOD Effect (SftClp, StrgrC)

**Procedure:**
1. Input 100 Hz sine, Drive at 10x
2. Sweep MOD from 0% to 100%
3. Observe waveform asymmetry change

**Expected:**
- MOD 0%: symmetric
- MOD 100%: asymmetric (different pos/neg response)

**Pass:** ☐ | **Fail:** ☐

---

## Test 2: Hard Clipping Verification

**Topology:** B (Hard Clipping)  
**Algorithms:** HardClp, Antipol

### 2.1 Abrupt Onset

**Procedure:**
1. Input 100 Hz sine
2. Increase Drive until clipping begins
3. Observe transition on oscilloscope

**Expected:**
- Sharp transition from linear to clipped
- Flat tops/bottoms on waveform
- Buzzy, aggressive sound

**Pass:** ☐ | **Fail:** ☐

### 2.2 Square Wave Approximation

**Procedure:**
1. Input 100 Hz sine
2. Set Drive to 50x
3. Observe waveform

**Expected:**
- Nearly square waveform
- Rich harmonic content
- Buzzy character

**Pass:** ☐ | **Fail:** ☐

### 2.3 Antipol Fold Mode

**Procedure:**
1. Select Antipol algorithm
2. Input 100 Hz sine, Drive at 20x
3. Set MOD to 0%, observe clipping
4. Set MOD to 100%, observe folding

**Expected:**
- MOD 0%: hard clipping (flat tops)
- MOD 100%: folding (peaks fold back)

**Pass:** ☐ | **Fail:** ☐

---

## Test 3: Wavefolding Verification

**Topology:** C (Wavefolding)  
**Algorithms:** WavFld, Crest, Sstrgi, Anti+

### 3.1 Harmonic Bloom

**Procedure:**
1. Select WavFld
2. Input 200 Hz triangle wave
3. Slowly increase Drive from 1x to 30x
4. Listen for harmonic sweep

**Expected:**
- Harmonics "bloom" as drive increases
- Metallic, bell-like character
- Complex spectrum

**Pass:** ☐ | **Fail:** ☐

### 3.2 MOD Fold Amount

**Procedure:**
1. Input 200 Hz triangle, Drive at 10x
2. Set MOD to 0%, count fold points on scope
3. Set MOD to 100%, count fold points

**Expected:**
- MOD 0%: single fold
- MOD 100%: multiple folds (2-3)

**Pass:** ☐ | **Fail:** ☐

### 3.3 Sstrgi Ripples

**Procedure:**
1. Select Sstrgi
2. Input 100 Hz sine, Drive at 15x
3. Observe waveform

**Expected:**
- Visible ripples on waveform
- Multiple harmonic components
- Complex timbre

**Pass:** ☐ | **Fail:** ☐

---

## Test 4: Geometric/Rectification Verification

**Topology:** D (Rectification & Geometric)  
**Algorithms:** Vshpe, Square, VariWS, SmartE

### 4.1 Vshpe Octave Effect

**Procedure:**
1. Select Vshpe
2. Input 100 Hz sine, Drive at 5x
3. Listen for pitch

**Expected:**
- Output sounds one octave higher (200 Hz)
- Full-wave rectification visible on scope

**Pass:** ☐ | **Fail:** ☐

### 4.2 Square Peak Emphasis

**Procedure:**
1. Select Square
2. Input 100 Hz sine, Drive at 10x
3. Observe waveform

**Expected:**
- Peaks emphasized
- Quiet portions crushed
- Unusual harmonic structure

**Pass:** ☐ | **Fail:** ☐

### 4.3 SmartE Morphing

**Procedure:**
1. Select SmartE
2. Input 100 Hz sine, Drive at 10x
3. Sweep MOD from 0% to 100%
4. Listen for character change

**Expected:**
- MOD 0%: soft clipping character
- MOD ~50%: hard clipping character
- MOD 100%: folding character

**Pass:** ☐ | **Fail:** ☐

---

## Test 5: Lo-Fi Verification

**Topology:** E (Sample/Bit Reduction)  
**Algorithms:** Bitcrsh, DwnSpl, Lowfi, Lowrfi, ChastB, SuspB

### 5.1 Bitcrush Resolution

**Procedure:**
1. Select Bitcrsh
2. Input 1 kHz sine, Drive at 2x
3. Set MOD to 0%, listen (should be clean)
4. Set MOD to 50%, listen (8-bit character)
5. Set MOD to 100%, listen (1-bit, square)

**Expected:**
- Progressive bit reduction
- Staircase artifacts visible on scope
- Retro/gaming character

**Pass:** ☐ | **Fail:** ☐

### 5.2 Downsample Aliasing

**Procedure:**
1. Select DwnSpl
2. Input 5 kHz sine, Drive at 2x
3. Set MOD to 100%
4. Listen for aliasing artifacts

**Expected:**
- Audible aliasing (metallic, inharmonic)
- Sample-and-hold staircase on scope

**Pass:** ☐ | **Fail:** ☐

### 5.3 Lowfi Transient Softening

**Procedure:**
1. Select Lowfi
2. Input percussive signal
3. Set MOD to 0% (extreme slew)
4. Listen for attack softening

**Expected:**
- Rounded, softened transients
- Tape-like character
- Slowed edges visible on scope

**Pass:** ☐ | **Fail:** ☐

---

## Test 6: Envelope Follower Verification

**Topology:** F (Envelope-Following)  
**Algorithms:** Disto, Jelly, Attrac

### 6.1 Disto Envelope Speed

**Procedure:**
1. Select Disto
2. Input percussive signal with long decay
3. Set MOD to 0% (slow envelope)
4. Listen for pumping/breathing
5. Set MOD to 100% (fast envelope)
6. Listen for tight limiting

**Expected:**
- MOD 0%: audible pumping, slow recovery
- MOD 100%: tight, controlled limiting

**Pass:** ☐ | **Fail:** ☐

### 6.2 Disto Compression Behavior

**Procedure:**
1. Input sustained signal at varying levels
2. Set Drive to 10x, MOD to 50%
3. Vary input level, listen for compression

**Expected:**
- Loud input: gain reduction applied
- Quiet input: unity gain
- Dynamic range reduced

**Pass:** ☐ | **Fail:** ☐

### 6.3 Jelly Squash Effect

**Procedure:**
1. Select Jelly
2. Input percussive signal
3. Set MOD to 100% (slow envelope)
4. Listen for squishy, pumping character

**Expected:**
- Envelope-dependent distortion amount
- Squashy, dynamic response

**Pass:** ☐ | **Fail:** ☐

---

## Test 7: Chaotic Systems Verification

**Topology:** G (Chaotic Systems)  
**Algorithms:** Fractl, Zippy, Bounce, NoizE, BrwnNz

### 7.1 Zero Input Behavior

**Procedure:**
1. Disconnect input
2. Test each chaotic algorithm
3. Listen for output

**Expected:**
- Fractl: may produce low-level chaos
- BrwnNz: produces noise
- Others: should be silent or near-silent

**Pass:** ☐ | **Fail:** ☐

### 7.2 Chaos Intensity

**Procedure:**
1. Select Fractl
2. Input low-level signal
3. Set MOD to 0%, listen
4. Set MOD to 100%, listen

**Expected:**
- MOD 0%: mild chaos, recognizable input
- MOD 100%: noise-like, chaotic

**Pass:** ☐ | **Fail:** ☐

### 7.3 Bounce Physics

**Procedure:**
1. Select Bounce
2. Input percussive signal
3. Listen for bouncing character

**Expected:**
- Percussive, bouncing response
- Physics-like behavior
- MOD affects gravity/friction

**Pass:** ☐ | **Fail:** ☐

---

## Test 8: Filter-Based Verification

**Topology:** H (Filter-Based)  
**Algorithms:** Filter, Vinyl

### 8.1 Filter Cutoff Sweep

**Procedure:**
1. Select Filter
2. Input sawtooth wave, Drive at 10x
3. Sweep MOD from 0% to 100%

**Expected:**
- MOD 0%: dark, filtered
- MOD 100%: bright, more resonant
- Classic filter sweep sound

**Pass:** ☐ | **Fail:** ☐

### 8.2 Filter Resonance with Drive

**Procedure:**
1. Input sawtooth, MOD at 50%
2. Increase Drive from 5x to 30x
3. Listen for resonance behavior

**Expected:**
- Higher drive: more saturation, tamed resonance
- Interaction between saturation and resonance

**Pass:** ☐ | **Fail:** ☐

### 8.3 Vinyl Character

**Procedure:**
1. Select Vinyl
2. Input full-range signal
3. Set MOD to 0%, listen (clean EQ)
4. Set MOD to 100%, listen (degraded)

**Expected:**
- MOD 0%: RIAA EQ curves, warm
- MOD 100%: worn record character

**Pass:** ☐ | **Fail:** ☐

---

## Test 9: FAM Mode Verification

### 9.1 Standard Mode (S)

**Procedure:**
1. Set FAM to S (Standard)
2. Input 100 Hz sine, Drive at 10x
3. Sweep BIAS, observe effect

**Expected:**
- BIAS adds subtle DC offset (15% range)
- Creates asymmetry in distortion

**Pass:** ☐ | **Fail:** ☐

### 9.2 Tube Mode (X)

**Procedure:**
1. Set FAM to X (Tube)
2. Input 100 Hz sine, Drive at 10x
3. Set BIAS to -1 (starved), listen
4. Set BIAS to 0 (triode), listen
5. Set BIAS to +1 (power tube), listen

**Expected:**
- BIAS -1: gritty, dying amp, dead zone
- BIAS 0: warm, classic tube
- BIAS +1: aggressive, saturated

**Pass:** ☐ | **Fail:** ☐

### 9.3 Odd Mode (O)

**Procedure:**
1. Set FAM to O (Odd)
2. Input 100 Hz sine, Drive at 5x
3. Set BIAS to -1, listen for pitch shift
4. Set BIAS to +1, listen for opposite shift

**Expected:**
- BIAS controls frequency shift (±100 Hz)
- Inharmonic, detuned character

**Pass:** ☐ | **Fail:** ☐

---

## Test 10: System Stability

### 10.1 Extreme Parameters

**Procedure:**
1. Set Drive to maximum (100x)
2. Set MOD to maximum
3. Set BIAS to maximum
4. Input moderate signal
5. Run for 60 seconds

**Expected:**
- No audio dropouts
- No clicks or pops
- Output remains bounded
- No runaway behavior

**Pass:** ☐ | **Fail:** ☐

### 10.2 Algorithm Switching

**Procedure:**
1. Input continuous signal
2. Rapidly switch algorithms (Freeze/Reverse)
3. Listen for glitches

**Expected:**
- Clean transitions
- No loud clicks
- State resets appropriately

**Pass:** ☐ | **Fail:** ☐

### 10.3 Zero Input Stability

**Procedure:**
1. Disconnect all inputs
2. Cycle through all 36 algorithms
3. Listen for unwanted output

**Expected:**
- Most algorithms: silence
- Noise generators (Fractl, BrwnNz): expected output
- No unexpected noise or oscillation

**Pass:** ☐ | **Fail:** ☐

---

## Issue Reporting Template

```
Algorithm: [name]
FAM Mode: [S/X/O]
Drive: [approximate position]
MOD: [approximate position]
BIAS: [approximate position]
Input Signal: [type, frequency, level]

Expected Behavior:
[description]

Actual Behavior:
[description]

Reproducibility: [always / sometimes / rare]

Notes:
[additional observations]
```

---

## Test Summary

| Test | Description | Pass | Fail |
|------|-------------|------|------|
| 1.1 | Soft clip gradual onset | ☐ | ☐ |
| 1.2 | Soft clip harmonics | ☐ | ☐ |
| 1.3 | Soft clip asymmetry | ☐ | ☐ |
| 1.4 | Soft clip MOD effect | ☐ | ☐ |
| 2.1 | Hard clip abrupt onset | ☐ | ☐ |
| 2.2 | Hard clip square wave | ☐ | ☐ |
| 2.3 | Antipol fold mode | ☐ | ☐ |
| 3.1 | Wavefold harmonic bloom | ☐ | ☐ |
| 3.2 | Wavefold MOD amount | ☐ | ☐ |
| 3.3 | Sstrgi ripples | ☐ | ☐ |
| 4.1 | Vshpe octave effect | ☐ | ☐ |
| 4.2 | Square peak emphasis | ☐ | ☐ |
| 4.3 | SmartE morphing | ☐ | ☐ |
| 5.1 | Bitcrush resolution | ☐ | ☐ |
| 5.2 | Downsample aliasing | ☐ | ☐ |
| 5.3 | Lowfi transient softening | ☐ | ☐ |
| 6.1 | Disto envelope speed | ☐ | ☐ |
| 6.2 | Disto compression | ☐ | ☐ |
| 6.3 | Jelly squash effect | ☐ | ☐ |
| 7.1 | Chaos zero input | ☐ | ☐ |
| 7.2 | Chaos intensity | ☐ | ☐ |
| 7.3 | Bounce physics | ☐ | ☐ |
| 8.1 | Filter cutoff sweep | ☐ | ☐ |
| 8.2 | Filter resonance | ☐ | ☐ |
| 8.3 | Vinyl character | ☐ | ☐ |
| 9.1 | FAM Standard mode | ☐ | ☐ |
| 9.2 | FAM Tube mode | ☐ | ☐ |
| 9.3 | FAM Odd mode | ☐ | ☐ |
| 10.1 | Extreme parameters | ☐ | ☐ |
| 10.2 | Algorithm switching | ☐ | ☐ |
| 10.3 | Zero input stability | ☐ | ☐ |

---

**Tester:** _______________________

**Date:** _______________________

**Firmware Version:** _______________________

**Hardware Serial:** _______________________

**Overall Result:** ☐ Pass ☐ Fail

**Notes:**
