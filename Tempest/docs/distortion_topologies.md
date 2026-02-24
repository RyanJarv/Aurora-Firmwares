# Distortion Algorithm Topologies

Technical reference for distortion algorithm design patterns used in Tempest.

## Table of Contents

1. [Fundamentals](#fundamentals)
2. [Topology A: Soft Clipping](#topology-a-soft-clipping)
3. [Topology B: Hard Clipping](#topology-b-hard-clipping)
4. [Topology C: Wavefolding](#topology-c-wavefolding)
5. [Topology D: Rectification & Geometric](#topology-d-rectification--geometric)
6. [Topology E: Sample/Bit Reduction](#topology-e-samplebit-reduction)
7. [Topology F: Envelope-Following](#topology-f-envelope-following)
8. [Topology G: Chaotic Systems](#topology-g-chaotic-systems)
9. [Topology H: Filter-Based](#topology-h-filter-based)

---

## Fundamentals

### Transfer Functions

A distortion algorithm applies a **transfer function** that maps input amplitude to output amplitude. The shape of this curve determines the harmonic content added to the signal.

```
Output
   │
  1├────────────────╮
   │               ╱ 
   │             ╱   
   │           ╱     ← Soft clip (gradual)
   │         ╱       
   │       ╱         ← Linear (no distortion)
   │     ╱           
   │   ╱    ┌────────← Hard clip (abrupt)
   │ ╱      │
───┼╱───────┼──────── Input
   │╲       │
   │  ╲     │
  -1├───────┴────────
```

**Key relationships:**
- Slope near zero = gain
- Flatness near ±1 = compression amount
- Curve smoothness = harmonic distribution

### Symmetry and Harmonics

The symmetry of the transfer function determines which harmonics are generated:

| Transfer Function | Mathematical Property | Harmonics Generated | Sonic Character |
|-------------------|----------------------|---------------------|-----------------|
| **Symmetric** | f(-x) = -f(x) | Odd only (3rd, 5th, 7th...) | Hollow, square-wave-like |
| **Asymmetric** | f(-x) ≠ -f(x) | Even + odd (2nd, 3rd, 4th...) | Warm, tube-like, fuller |

**Asymmetry sources in Tempest:**
- BIAS parameter adds DC offset before distortion
- MOD parameter often controls asymmetric gain curves
- Some algorithms have inherent asymmetry (Valve, Fezz)

### Signal Flow

All Tempest algorithms follow this pattern:

```
Input → [+ BIAS×0.5] → [× Gain] → [Transfer Function] → [Clamp ±1] → Output
```

State variables (for stateful algorithms):
```
Input → [+ BIAS×0.5] → [× Gain] → [Transfer Function + State Update] → [Clamp ±1] → Output
                                          ↑
                                    Previous State
```

---

## Topology A: Soft Clipping

### Theory

Soft clipping uses smooth, continuous transfer functions that gradually compress signal peaks. The transition from linear to compressed is gradual, producing lower-order harmonics with decreasing amplitude.

**Mathematical forms:**

| Function | Formula | Character |
|----------|---------|-----------|
| Hyperbolic tangent | tanh(x) | Symmetric, smooth |
| Arctangent | atan(x) / (π/2) | Symmetric, wider knee |
| Soft limit | x(27 + x²) / (27 + 9x²) | Symmetric, DaisySP default |
| Logarithmic | sign(x) × log(1 + |x|) / log(2) | Can be asymmetric |
| Cubic | x - x³/3 | Symmetric, odd harmonics only |

**Transfer curve shape:**

```
Output
   │      _______________
  1├─────╱
   │    ╱
   │   ╱   ← Gradual transition (soft knee)
   │  ╱
   │ ╱
   │╱
───┼─────────────────────── Input
   │╲
   │ ╲
   │  ╲
   │   ╲
  -1├────╲_______________
```

### Tempest Implementations

| Algorithm | Transfer Function | Asymmetry Source |
|-----------|-------------------|------------------|
| **SftClp** | Logarithmic: log(1 + x×(1+mod)) | MOD scales positive/negative differently |
| **Valve** | SoftLimit with asymmetric gain | MOD reduces negative gain |
| **Cubic** | x - x³/3 | None (pure symmetric) |
| **StrgrC** | atan(x×k) / (π/2 + k×0.5) | MOD changes k for pos/neg |

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Low drive (1-3x) | Nearly clean, subtle warmth |
| Medium drive (5-15x) | Audible compression, rounded peaks |
| High drive (20-50x) | Strong saturation, preserved dynamics |
| MOD at 0% | Symmetric (SftClp, StrgrC) or max asymmetry (Valve) |
| MOD at 100% | Asymmetric (SftClp, StrgrC) or symmetric (Valve) |
| BIAS off-center | Even harmonics appear |

---

## Topology B: Hard Clipping

### Theory

Hard clipping abruptly limits the signal at a threshold. The sharp transition creates a discontinuity in the derivative, generating high-order harmonics.

**Mathematical form:**

```
        ⎧ threshold    if x > threshold
f(x) =  ⎨ x            if -threshold ≤ x ≤ threshold
        ⎩ -threshold   if x < -threshold
```

**Transfer curve shape:**

```
Output
   │      _______________
  1├─────┤
   │     │  ← Abrupt transition (hard knee)
   │     │
   │    ╱
   │   ╱
   │  ╱
   │ ╱
   │╱
───┼─────────────────────── Input
   │╲
   │ ╲
   │  ╲
   │   │
  -1├──┴─────────────────
```

### Tempest Implementations

| Algorithm | Threshold Behavior | Asymmetry Source |
|-----------|-------------------|------------------|
| **HardClp** | Asymmetric thresholds | MOD shifts pos/neg thresholds |
| **Antipol** | Fixed at ±1, optional fold | MOD enables folding above threshold |

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Low drive | Clean until signal exceeds threshold |
| At threshold | Abrupt onset of distortion |
| High drive | Square-wave-like, buzzy, rich harmonics |
| MOD at 0% | Symmetric thresholds |
| MOD at 100% | Asymmetric thresholds (HardClp) or folding (Antipol) |
| High frequencies | Potential aliasing artifacts |

---

## Topology C: Wavefolding

### Theory

Wavefolding reflects the signal back when it exceeds a threshold, creating complex harmonic spectra. Unlike clipping, the output continues to change as input increases.

**Mathematical forms:**

| Type | Formula | Character |
|------|---------|-----------|
| Sine folder | sin(x × k) | Smooth, bell-like |
| Triangle folder | Piecewise linear reflection | Sharper harmonics |
| Serge-style | Multiple fold points | Complex spectrum |

**Transfer curve shape:**

```
Output
   │      ╱╲
  1├─────╱  ╲
   │    ╱    ╲
   │   ╱      ╲
   │  ╱        ╲
   │ ╱          ╲
   │╱            ╲
───┼──────────────╲────── Input
   │               ╲
   │                ╲  ╱
   │                 ╲╱
  -1├
```

**Key property:** As input amplitude increases, output sweeps through harmonics rather than saturating.

### Tempest Implementations

| Algorithm | Fold Type | MOD Effect |
|-----------|-----------|------------|
| **WavFld** | sin(x × fold_amt × π/2) | MOD increases fold_amt (1 to 3) |
| **Crest** | Linear fold near peaks | MOD shifts fold threshold |
| **Sstrgi** | SoftLimit + sin ripples | MOD controls ripple asymmetry |
| **Anti+** | Hard clip + sin shaping | MOD controls fold amount |

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Low drive | Subtle harmonic addition |
| Increasing drive | Harmonics "bloom" and sweep |
| High drive | Complex, metallic timbre |
| MOD at 0% | Single fold (WavFld) |
| MOD at 100% | Multiple folds, dense spectrum |
| Triangle input | Classic "West Coast" timbre |
| Sine input | Bell-like, FM-ish harmonics |

---

## Topology D: Rectification & Geometric

### Theory

These algorithms apply mathematical operations that change the fundamental relationship between input and output, often creating non-harmonic or unusual spectral content.

**Types:**

| Operation | Formula | Effect |
|-----------|---------|--------|
| Full-wave rectification | \|x\| | Doubles frequency (octave up) |
| Half-wave rectification | max(x, 0) | Adds DC + harmonics |
| Squaring | x² × sign(x) | Emphasizes peaks |
| Power curve | sign(x) × \|x\|^n | Variable compression |

**Full-wave rectification:**

```
Input:    ╱╲    ╱╲    ╱╲
         ╱  ╲  ╱  ╲  ╱  ╲
        ╱    ╲╱    ╲╱    ╲
       ╱                  ╲
      ╱                    ╲

Output:   ╱╲  ╱╲  ╱╲  ╱╲  ╱╲  (frequency doubled)
         ╱  ╲╱  ╲╱  ╲╱  ╲╱  ╲
```

### Tempest Implementations

| Algorithm | Operation | MOD Effect |
|-----------|-----------|------------|
| **Vshpe** | Asymmetric absolute value | MOD controls pos/neg scaling |
| **Square** | x² × sign(x) | None |
| **VariWS** | 1 - (1 - \|x\|)^curve | MOD controls curve exponent |
| **SmartE** | Blend of soft/hard/fold | MOD morphs between types |

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Vshpe + sine | Octave-up effect |
| Square + sine | Peak emphasis, quiet parts crushed |
| VariWS MOD 0% | Linear (no effect) |
| VariWS MOD 100% | Hard limiting |
| SmartE MOD sweep | Morphs soft → hard → fold |

---

## Topology E: Sample/Bit Reduction

### Theory

Digital degradation effects that quantize the signal in amplitude (bit reduction) or time (sample rate reduction).

**Bit reduction:**

```
Continuous input    →    Quantized output
       ╱╲                    ┌─┐
      ╱  ╲                 ┌─┘ └─┐
     ╱    ╲              ┌─┘     └─┐
    ╱      ╲           ┌─┘         └─┐
```

Formula: `floor(x × levels + 0.5) / levels` where `levels = 2^bits`

**Sample rate reduction:**

```
Input:   ╱╲╱╲╱╲╱╲╱╲╱╲
Output:  ┌┐  ┌┐  ┌┐    (sample & hold)
         └┘  └┘  └┘
```

**Slew rate limiting:**

```
Input:   ┌───┐     (fast edge)
         │   │
Output:  ╱───╲     (slowed edge)
        ╱     ╲
```

### Tempest Implementations

| Algorithm | Type | MOD Effect |
|-----------|------|------------|
| **Bitcrsh** | Bit depth reduction | MOD: 16-bit → 1-bit |
| **DwnSpl** | Sample & hold | MOD: full rate → 64× reduction |
| **Lowfi** | Linear slew limiting | MOD: extreme → mild slew |
| **Lowrfi** | Non-linear slew limiting | MOD: extreme → mild, amplitude-dependent |
| **ChastB** | Float mantissa truncation | MOD: bits zeroed |
| **SuspB** | Float exponent modification | MOD: exponent shift ±4 |

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Bitcrsh MOD 0% | Clean (16-bit) |
| Bitcrsh MOD 50% | 8-bit, retro gaming sound |
| Bitcrsh MOD 100% | 1-bit, square wave |
| DwnSpl MOD 100% | Heavy aliasing, metallic |
| DwnSpl + high freq | Aliased artifacts |
| Lowfi + transients | Rounded, softened attacks |
| ChastB/SuspB | Glitchy, unpredictable |

---

## Topology F: Envelope-Following

### Theory

Dynamic distortion where the effect amount or gain is controlled by an envelope follower tracking the input level.

**Signal flow:**

```
Input ──┬──→ [Distortion] ──→ Output
        │         ↑
        │    Gain/Amount
        │         │
        └──→ [Envelope Follower]
                  │
             Attack/Release
```

**Envelope follower:**

```cpp
if (|input| > envelope)
    envelope += attack_coeff × (|input| - envelope)
else
    envelope += release_coeff × (|input| - envelope)
```

**One-pole coefficient:** `coeff = 1 / (sample_rate × time_seconds)`

### Tempest Implementations

| Algorithm | Envelope Controls | MOD Effect |
|-----------|-------------------|------------|
| **Disto** | Gain reduction (limiter) | MOD: slow (pumping) → fast (tight) |
| **Jelly** | Drive amount + output level | MOD: fast → slow envelope |
| **Attrac** | Attractor strength | MOD: fast → slow envelope |

**Disto behavior:**

```
Loud input → High envelope → Gain reduction → Compressed output
Quiet input → Low envelope → Unity gain → Clean output
```

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Disto MOD 0% | Slow attack/release, audible pumping |
| Disto MOD 100% | Fast attack/release, tight limiting |
| Jelly MOD 0% | Tight, responsive |
| Jelly MOD 100% | Squishy, slow response |
| Percussive input | Attack/release behavior audible |
| Sustained input | Compression/limiting audible |

---

## Topology G: Chaotic Systems

### Theory

Nonlinear dynamical systems that produce complex, often unpredictable output. These can be driven by input signal or run autonomously.

**Types:**

| System | Behavior |
|--------|----------|
| Iterated maps | Feedback equation applied repeatedly |
| Strange attractors | Deterministic chaos (Lorenz, etc.) |
| Physics simulation | Bouncing, collision, etc. |
| Random walk | Brownian motion |

**Lorenz attractor:**

```cpp
dx/dt = σ(y - x)
dy/dt = x(ρ - z) - y
dz/dt = xy - βz
```

### Tempest Implementations

| Algorithm | System Type | Input Coupling |
|-----------|-------------|----------------|
| **Fractl** | Iterated quadratic map | Input injected each iteration |
| **Zippy** | Lorenz attractor | Input modulates ρ parameter |
| **Bounce** | Ball physics | Input defines boundaries |
| **NoizE** | Triangular folding | Amplitude-based |
| **BrwnNz** | Random walk | Input biases walk direction |

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Zero input | Some algorithms produce output (Fractl, BrwnNz) |
| Low drive | Subtle chaotic modulation |
| High drive | Noise-like, unpredictable |
| Fractl MOD high | More iterations, more chaos |
| Zippy MOD high | Faster chaos rate |
| Bounce | Percussive, bouncing character |

---

## Topology H: Filter-Based

### Theory

Combines distortion with filtering, either before, after, or within a feedback loop.

**Configurations:**

```
Pre-filter:   Input → [Filter] → [Distortion] → Output

Post-filter:  Input → [Distortion] → [Filter] → Output

Feedback:     Input → [+] → [Distortion] → [Filter] → Output
                      ↑                        │
                      └────────────────────────┘
```

**Resonant filter with saturation:**

The filter's resonance creates peaks that are then saturated, producing characteristic "acid" tones.

### Tempest Implementations

| Algorithm | Configuration | MOD Effect |
|-----------|---------------|------------|
| **Filter** | Saturate → Resonant LPF | MOD: cutoff frequency |
| **Vinyl** | Pre-emphasis → Saturate → De-emphasis | MOD: degradation amount |

**Filter resonance interaction:**

- Higher drive → more saturation → resonance tamed
- Lower drive → less saturation → resonance more prominent

### Expected Behavior

| Condition | Expected Result |
|-----------|-----------------|
| Filter MOD 0% | Dark, subby |
| Filter MOD 100% | Brighter, more resonant |
| Filter + high drive | Resonance + saturation |
| Vinyl MOD 0% | Clean RIAA EQ |
| Vinyl MOD 100% | Degraded, worn record |
| Sawtooth input | Classic filter sweep sound |

---

## Summary Table

| Topology | Algorithms | Key Characteristic | Harmonic Content |
|----------|------------|-------------------|------------------|
| A. Soft Clip | SftClp, Valve, Cubic, StrgrC | Gradual compression | Low-order, decreasing |
| B. Hard Clip | HardClp, Antipol | Abrupt limiting | High-order, rich |
| C. Wavefold | WavFld, Crest, Sstrgi, Anti+ | Reflection at threshold | Complex, sweeping |
| D. Geometric | Vshpe, Square, VariWS, SmartE | Mathematical shaping | Non-standard |
| E. Lo-Fi | Bitcrsh, DwnSpl, Lowfi, etc. | Quantization | Aliasing, staircase |
| F. Envelope | Disto, Jelly, Attrac | Dynamic response | Varies with level |
| G. Chaotic | Fractl, Zippy, Bounce, etc. | Unpredictable | Noise-like |
| H. Filter | Filter, Vinyl | Frequency shaping | Resonant peaks |
