# Tempest Algorithm Reference

Detailed sonic characteristics for all 36 distortion algorithms.

## Parameter Overview

All algorithms respond to these parameters:

| Parameter | Control | Effect |
|-----------|---------|--------|
| **Drive** | Warp knob + CV | Input gain (1x-100x). Higher = more saturation/harmonics |
| **MOD** | Blur knob + CV | Algorithm-specific modulation (0-1) |
| **BIAS** | Reflect knob + CV | DC offset before distortion. Creates asymmetry |

---

## Bank 1: Analog (Orange)

Classic analog-style saturation. Warm, musical distortion that adds harmonics without harshness.

### Soft Clip (SftClp)

**Sonic Character**: Warm, tube-like saturation with smooth compression. The softest algorithm in the bank.

**Technical**: Logarithmic waveshaping with asymmetric positive/negative response.

| MOD Position | Effect |
|--------------|--------|
| 0% | Symmetric soft clipping |
| 50% | Slight asymmetry, adds even harmonics |
| 100% | Strong asymmetry, tube-like second harmonic emphasis |

**Use Cases**:
- Warming up digital synths
- Gentle bass saturation
- Mastering-style glue

**Pairs Well With**: Low drive (2-5x), post-filter around 8kHz

---

### Hard Clip (HardClp)

**Sonic Character**: Aggressive, buzzy distortion with defined edges. More harmonics than SftClp.

**Technical**: Asymmetric hard clipping with variable positive/negative thresholds.

| MOD Position | Effect |
|--------------|--------|
| 0% | Symmetric hard clip at ±1 |
| 50% | Positive clips earlier, adds grit |
| 100% | Strong asymmetry, transistor-like buzz |

**Use Cases**:
- Aggressive lead sounds
- Drum transient destruction
- Lo-fi character

**Pairs Well With**: Medium drive (5-20x), pre-filter to remove sub-bass

---

### Stronger Curve (StrgrC)

**Sonic Character**: Smooth but present saturation. Sits between soft and hard clipping.

**Technical**: Arctangent waveshaping with asymmetric gain structure.

| MOD Position | Effect |
|--------------|--------|
| 0% | Symmetric arctan curve |
| 50% | Moderate asymmetry |
| 100% | Strong asymmetry, positive peaks compress more |

**Use Cases**:
- Guitar amp simulation
- Synth leads needing presence
- Parallel distortion blending

**Pairs Well With**: Medium-high drive (10-30x)

---

### Valve (Valve)

**Sonic Character**: Classic tube amplifier response. Asymmetric with natural compression.

**Technical**: Soft limiting with different positive/negative gain coefficients.

| MOD Position | Effect |
|--------------|--------|
| 0% | Maximum asymmetry (tube-like) |
| 50% | Moderate asymmetry |
| 100% | Nearly symmetric (solid-state feel) |

**Use Cases**:
- Tube preamp emulation
- Warming cold digital sources
- Bass amp simulation

**Pairs Well With**: Low-medium drive (2-10x), BIAS slightly positive

---

### Antipolar (Antipol)

**Sonic Character**: Hard clipping that transitions to wavefolding at high levels.

**Technical**: Hard clip with excess signal folded back based on MOD.

| MOD Position | Effect |
|--------------|--------|
| 0% | Pure hard clipping |
| 50% | Neutral (no folding) |
| 100% | Aggressive folding of peaks |

**Use Cases**:
- Transitioning from clean to destroyed
- Complex harmonic generation
- Experimental textures

**Pairs Well With**: High drive (20-50x) to engage folding

---

### Distortion Limiter (Disto)

**Sonic Character**: Dynamic limiter with analog feel. Responds to input dynamics.

**Technical**: Envelope-following gain reduction with soft limiting.

| MOD Position | Effect |
|--------------|--------|
| 0% | Slow attack/release (pumping, breathing) |
| 50% | Moderate dynamics |
| 100% | Fast attack/release (tight, controlled) |

**Use Cases**:
- Drums (parallel compression + saturation)
- Dynamic bass lines
- Sidechain-style pumping effects

**Pairs Well With**: Rhythmic material, medium drive

---

## Bank 2: Fold (Purple)

Wavefolding algorithms that create complex harmonic spectra. Can range from subtle to extreme.

### Wavefold (WavFld)

**Sonic Character**: Classic sine wavefolder. Smooth, bell-like harmonics.

**Technical**: Input passed through sine function with variable frequency.

| MOD Position | Effect |
|--------------|--------|
| 0% | Single fold (subtle harmonics) |
| 50% | 2x folding (richer spectrum) |
| 100% | 3x folding (dense, metallic) |

**Use Cases**:
- West Coast synthesis
- Adding shimmer to pads
- Metallic percussion

**Pairs Well With**: Sine/triangle inputs, moderate drive

---

### Crest (Crest)

**Sonic Character**: Soft folding near the peaks. Less aggressive than WavFld.

**Technical**: Non-bijective curve that folds back above threshold.

| MOD Position | Effect |
|--------------|--------|
| 0% | Symmetric folding |
| 50% | Asymmetric threshold |
| 100% | Strong asymmetry, one-sided folding |

**Use Cases**:
- Subtle harmonic enhancement
- Taming harsh transients while adding color
- Gentle wavefolding for pads

**Pairs Well With**: Complex waveforms, low-medium drive

---

### Staircase Ripple (Sstrgi)

**Sonic Character**: Waveshaper with ripple harmonics. Creates multiple fold points.

**Technical**: Soft limiting plus sine-based ripples modulated by signal level.

| MOD Position | Effect |
|--------------|--------|
| 0% | Symmetric ripples |
| 50% | Moderate asymmetry |
| 100% | Strong asymmetry, uneven harmonic distribution |

**Use Cases**:
- Complex timbres from simple sources
- Evolving textures with drive modulation
- Experimental sound design

**Pairs Well With**: LFO on drive for animated textures

---

### Fuzz (Fezz)

**Sonic Character**: Asymmetric fuzz with positive bias. Germanium transistor feel.

**Technical**: Different soft limiting curves for positive/negative half-cycles.

| MOD Position | Effect |
|--------------|--------|
| 0% | Mild asymmetry |
| 50% | Moderate fuzz character |
| 100% | Strong asymmetry, sputtery fuzz |

**Use Cases**:
- Vintage fuzz tones
- Bass with growl
- Synth leads with character

**Pairs Well With**: Guitar/bass inputs, medium drive

---

### Fuss (Fuss)

**Sonic Character**: Asymmetric with positive peak folding. Unique harmonic structure.

**Technical**: Positive peaks fold back above 0.8, negative side soft clips.

| MOD Position | Effect |
|--------------|--------|
| 0% | Mild folding |
| 50% | Moderate fold + asymmetry |
| 100% | Aggressive positive folding |

**Use Cases**:
- Unique distortion textures
- One-sided harmonic emphasis
- Experimental bass tones

**Pairs Well With**: Asymmetric waveforms, BIAS modulation

---

### Crossover (Cross)

**Sonic Character**: Crossover distortion. Dead zone around zero crossing.

**Technical**: Quadratic response near zero, soft limiting above threshold.

| MOD Position | Effect |
|--------------|--------|
| 0% | Small dead zone |
| 50% | Moderate crossover distortion |
| 100% | Large dead zone, broken amp sound |

**Use Cases**:
- Class B amplifier emulation
- Broken/dying equipment sounds
- Adding grit to clean signals

**Pairs Well With**: Low-medium drive, sine waves show effect clearly

---

## Bank 3: Geometric (Yellow)

Mathematical waveshaping with variable curves. Precise control over harmonic content.

### Variable Waveshape (VariWS)

**Sonic Character**: Smooth variable curve from soft to hard. Very controllable.

**Technical**: Power function waveshaping with variable exponent.

| MOD Position | Effect |
|--------------|--------|
| 0% | Linear (no distortion) |
| 50% | Moderate curve (soft saturation) |
| 100% | Sharp curve (hard limiting) |

**Use Cases**:
- Morphing between clean and distorted
- Precise saturation control
- Automation-friendly distortion

**Pairs Well With**: Envelope on MOD for dynamic shaping

---

### Smart Edge (SmartE)

**Sonic Character**: Intelligent waveshaper that morphs between soft, hard, and folded.

**Technical**: Blends soft limit, hard limit, and sine fold based on MOD.

| MOD Position | Effect |
|--------------|--------|
| 0% | Pure soft limiting |
| 33% | Soft/hard blend |
| 66% | Hard limiting |
| 100% | Hard + sine folding |

**Use Cases**:
- All-in-one distortion
- Exploring different characters quickly
- Complex automation curves

**Pairs Well With**: Full MOD sweeps, medium-high drive

---

### V-Shape (Vshpe)

**Sonic Character**: Full-wave rectifier effect. Doubles fundamental frequency.

**Technical**: Absolute value with asymmetric scaling.

| MOD Position | Effect |
|--------------|--------|
| 0% | Symmetric V-shape |
| 50% | Moderate asymmetry |
| 100% | Strong asymmetry, uneven octave effect |

**Use Cases**:
- Octave-up effects
- Sub-bass to bass conversion
- Ring modulator-like tones

**Pairs Well With**: Sine waves, bass sounds

---

### Square (Square)

**Sonic Character**: Squared waveshaping. Emphasizes peaks, crushes quiet parts.

**Technical**: x² function preserving sign.

| MOD Position | Effect |
|--------------|--------|
| — | MOD has no effect |

**Use Cases**:
- Extreme compression effect
- Noise gate-like behavior
- Experimental dynamics

**Pairs Well With**: Dynamic sources, high drive

---

### Cubic (Cubic)

**Sonic Character**: Classic cubic soft clipper. Odd harmonics only.

**Technical**: x - x³/3 transfer function.

| MOD Position | Effect |
|--------------|--------|
| — | MOD has no effect |

**Use Cases**:
- Clean odd-harmonic saturation
- Symmetric distortion
- Textbook soft clipping

**Pairs Well With**: Any source, moderate drive

---

### Antipolar Plus (Anti+)

**Sonic Character**: Antipol algorithm with additional harmonic content from sine shaping.

**Technical**: Folding clipper plus sine waveshaper.

| MOD Position | Effect |
|--------------|--------|
| 0% | Hard clip only |
| 50% | Neutral fold point |
| 100% | Aggressive folding + sine harmonics |

**Use Cases**:
- Complex harmonic generation
- Transitional distortion
- Sound design

**Pairs Well With**: High drive, complex sources

---

## Bank 4: LoFi (Cyan)

Digital degradation effects. Bit crushing, sample rate reduction, and floating-point manipulation.

### Bitcrush (Bitcrsh)

**Sonic Character**: Classic bit depth reduction. Staircase quantization.

**Technical**: Reduces bit depth from 16 to 1 bit.

| MOD Position | Effect |
|--------------|--------|
| 0% | 16-bit (transparent) |
| 50% | 8-bit (retro gaming) |
| 100% | 1-bit (extreme, square wave) |

**Use Cases**:
- Retro/chiptune sounds
- Lo-fi texture
- Extreme digital destruction

**Pairs Well With**: Low drive (let bitcrush do the work)

---

### Downsample (DwnSpl)

**Sonic Character**: Sample rate reduction. Aliasing and staircase artifacts.

**Technical**: Sample-and-hold with variable hold time.

| MOD Position | Effect |
|--------------|--------|
| 0% | Full sample rate (transparent) |
| 50% | ~1.5kHz effective rate |
| 100% | ~750Hz effective rate (extreme aliasing) |

**Use Cases**:
- Vintage sampler emulation
- Aliasing as effect
- Telephone/radio sounds

**Pairs Well With**: Pre-filter to control aliasing character

---

### Low Fidelity (Lowfi)

**Sonic Character**: Slew rate limiting. Rounds off transients.

**Technical**: Maximum rate of change limiting.

| MOD Position | Effect |
|--------------|--------|
| 0% | Extreme slew limiting (very rounded) |
| 50% | Moderate limiting |
| 100% | Mild limiting (subtle) |

**Use Cases**:
- Softening harsh transients
- Tape-like high frequency loss
- Smoothing digital harshness

**Pairs Well With**: Percussive sources, high frequencies

---

### Lower Fidelity (Lowrfi)

**Sonic Character**: Non-linear slew limiting. Signal-dependent smoothing.

**Technical**: Slew rate varies with signal amplitude.

| MOD Position | Effect |
|--------------|--------|
| 0% | Extreme non-linear slew |
| 50% | Moderate effect |
| 100% | Mild effect |

**Use Cases**:
- Dynamic smoothing
- Amplitude-dependent filtering
- Unique lo-fi character

**Pairs Well With**: Dynamic sources

---

### Chaste Bits (ChastB)

**Sonic Character**: Floating-point mantissa corruption. Subtle to extreme digital artifacts.

**Technical**: Zeroes least significant mantissa bits.

| MOD Position | Effect |
|--------------|--------|
| 0% | Transparent |
| 50% | Subtle quantization |
| 100% | Extreme mantissa truncation |

**Use Cases**:
- Unique digital artifacts
- Subtle degradation
- Experimental textures

**Pairs Well With**: Clean sources to hear effect clearly

---

### Suspect Bits (SuspB)

**Sonic Character**: Floating-point exponent manipulation. Gain and dynamic changes.

**Technical**: Modifies IEEE 754 exponent bits.

| MOD Position | Effect |
|--------------|--------|
| 0% | -4 exponent (quieter, compressed) |
| 50% | No change |
| 100% | +4 exponent (louder, expanded) |

**Use Cases**:
- Unique gain structures
- Experimental dynamics
- Glitch effects

**Pairs Well With**: Careful gain staging

---

## Bank 5: Chaos (Red)

Chaotic systems, noise, and physics simulations. Unpredictable and experimental.

### Fractal (Fractl)

**Sonic Character**: Chaotic feedback equation. Self-similar noise patterns.

**Technical**: Iterated quadratic map with input injection.

| MOD Position | Effect |
|--------------|--------|
| 0% | Single iteration (mild) |
| 50% | 2-3 iterations |
| 100% | 5 iterations (chaotic) |

**Use Cases**:
- Noise generation
- Chaotic modulation source
- Experimental textures

**Pairs Well With**: Low input levels, high drive

---

### Zippy (Zippy)

**Sonic Character**: Lorenz attractor modulation. Chaotic but deterministic.

**Technical**: Lorenz system modulates distortion amount.

| MOD Position | Effect |
|--------------|--------|
| 0% | Slow chaos |
| 50% | Medium chaos rate |
| 100% | Fast, jittery chaos |

**Use Cases**:
- Evolving textures
- Unpredictable modulation
- Experimental sound design

**Pairs Well With**: Sustained sounds, pads

---

### Bounce (Bounce)

**Sonic Character**: Ball-in-cylinder physics. Bouncing, percussive response.

**Technical**: Simulated physics with gravity and friction.

| MOD Position | Effect |
|--------------|--------|
| 0% | Low gravity, slow bounces |
| 50% | Medium physics |
| 100% | High gravity, fast bounces, less friction |

**Use Cases**:
- Percussive effects
- Physical modeling textures
- Experimental rhythms

**Pairs Well With**: Transient-rich sources

---

### Noise E (NoizE)

**Sonic Character**: Triangular wavefolder that becomes noise at high gain.

**Technical**: Cyclic triangular folding based on amplitude.

| MOD Position | Effect |
|--------------|--------|
| — | MOD has no effect |

**Use Cases**:
- Noise generation from any source
- Extreme harmonic density
- White noise synthesis

**Pairs Well With**: Very high drive for noise, low drive for folding

---

### Brown Noise (BrwnNz)

**Sonic Character**: Brownian motion / random walk. Wandering, drifting output.

**Technical**: Random walk with input influence.

| MOD Position | Effect |
|--------------|--------|
| 0% | Slow random walk |
| 50% | Medium rate |
| 100% | Fast random walk |

**Use Cases**:
- Noise generation
- Random modulation source
- Experimental textures

**Pairs Well With**: Any input (used as seed)

---

### Attractor (Attrac)

**Sonic Character**: Chaotic attractor pulling signal toward ±1. Dynamic asymmetry.

**Technical**: Envelope-following attractor with chaos scaling.

| MOD Position | Effect |
|--------------|--------|
| 0% | Fast envelope, tight attraction |
| 50% | Medium response |
| 100% | Slow envelope, loose attraction |

**Use Cases**:
- Dynamic saturation
- Envelope-following distortion
- Experimental dynamics

**Pairs Well With**: Dynamic sources, percussive material

---

## Bank 6: Temporal (Green)

Time-based and experimental algorithms. Filters, delays, and envelope followers.

### Filter (Filter)

**Sonic Character**: Resonant low-pass filter with distortion. Filter sweep sounds.

**Technical**: Soft-clipped input through resonant 1-pole filter.

| MOD Position | Effect |
|--------------|--------|
| 0% | Low cutoff (dark, subby) |
| 50% | Medium cutoff |
| 100% | High cutoff (brighter) |

**Note**: Drive increases resonance automatically.

**Use Cases**:
- Filter sweeps with saturation
- Acid-style sounds
- Resonant bass

**Pairs Well With**: Sawtooth waves, bass sounds

---

### Time Freeze (TimeFr)

**Sonic Character**: Envelope-triggered sample freeze. Glitchy sustain.

**Technical**: Holds sample when envelope exceeds threshold.

| MOD Position | Effect |
|--------------|--------|
| 0% | Fast envelope, frequent freezes |
| 50% | Medium response |
| 100% | Slow envelope, rare freezes |

**Use Cases**:
- Glitch effects
- Stuttering textures
- Experimental rhythms

**Pairs Well With**: Dynamic sources, percussive material

---

### Vinyl (Vinyl)

**Sonic Character**: Vinyl record emulation. RIAA EQ curves and stylus saturation.

**Technical**: Pre-emphasis, soft saturation, de-emphasis.

| MOD Position | Effect |
|--------------|--------|
| 0% | Clean vinyl (just EQ curves) |
| 50% | Moderate degradation |
| 100% | Heavy degradation, worn record |

**Use Cases**:
- Vintage vinyl character
- Lo-fi warmth
- Retro production

**Pairs Well With**: Full mixes, drums, samples

---

### Clicks (Clicks)

**Sonic Character**: Random vinyl clicks and pops.

**Technical**: Random impulses added to signal.

| MOD Position | Effect |
|--------------|--------|
| 0% | No clicks |
| 50% | Moderate click density |
| 100% | Heavy clicking |

**Use Cases**:
- Vinyl emulation
- Lo-fi texture
- Percussive additions

**Pairs Well With**: Vinyl algorithm, lo-fi productions

---

### Jelly (Jelly)

**Sonic Character**: Envelope-following distortion. Squishes with dynamics.

**Technical**: Envelope controls both drive and output level.

| MOD Position | Effect |
|--------------|--------|
| 0% | Fast envelope (tight response) |
| 50% | Medium response |
| 100% | Slow envelope (squishy, pumping) |

**Use Cases**:
- Dynamic saturation
- Pumping/breathing effects
- Responsive distortion

**Pairs Well With**: Dynamic sources, drums

---

### Accumulator (Accum)

**Sonic Character**: Low-frequency accumulator. Adds sub-harmonic content.

**Technical**: Integrates input signal with decay.

| MOD Position | Effect |
|--------------|--------|
| 0% | Slow accumulation |
| 50% | Medium rate |
| 100% | Fast accumulation |

**Use Cases**:
- Sub-bass enhancement
- Experimental low-end
- Drone textures

**Pairs Well With**: Bass sounds, low-frequency content

---

## Algorithm Selection Guide

### By Use Case

| Goal | Recommended Algorithms |
|------|------------------------|
| Warm saturation | SftClp, Valve, Cubic |
| Aggressive distortion | HardClp, Fezz, Fuss |
| Complex harmonics | WavFld, Sstrgi, SmartE |
| Lo-fi/retro | Bitcrsh, DwnSpl, Vinyl |
| Experimental | Fractl, Zippy, Bounce |
| Dynamic response | Disto, Jelly, Attrac |
| Octave effects | Vshpe, NoizE |
| Filter + distortion | Filter, StrgrC |

### By Input Type

| Input | Recommended Algorithms |
|-------|------------------------|
| Sine waves | WavFld, Vshpe, Cross |
| Sawtooth | SftClp, Filter, Valve |
| Square waves | Bitcrsh, Cubic, HardClp |
| Drums | Disto, Jelly, Bounce |
| Bass | Valve, Fezz, Filter |
| Pads | Sstrgi, Zippy, Vinyl |
| Full mix | SftClp, Cubic, Vinyl |
