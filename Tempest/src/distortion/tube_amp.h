/** Tube Amp Simulation
 *  Multi-stage tube amplifier emulation with bias-progressive architecture.
 *  Inspired by Mr. Hector's tube modeling approach.
 */
#pragma once
#ifndef DISTORTION_TUBE_AMP_H
#define DISTORTION_TUBE_AMP_H

#include "types.h"

namespace distortion
{

// Linear interpolation helper
inline float Lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

// Stage 1: Input buffer - subtle coloring, always active
inline float TubeStage1(float in, float bias)
{
    // Input buffer stage - slight asymmetry for even harmonics
    // No compensation - let signal grow through cascade
    float out;
    if (in >= 0.f)
        out = SoftLimit(in * 1.2f);
    else
        out = SoftLimit(in * 1.3f);
    return out;
}

// Stage 2: Color stage - asymmetric saturation, adds 2nd + 3rd harmonics
inline float TubeStage2(float in)
{
    // 12AX7-style asymmetric clipping
    // No compensation - cascades with previous stage
    float out;
    if (in >= 0.f)
        out = SoftLimit(in * 1.3f);
    else
        out = SoftLimit(in * 1.4f);
    // Add subtle 2nd harmonic
    out += 0.05f * out * out * Sign(out);
    return out;
}

// Stage 3: Body/compression - symmetric, adds odd harmonics
inline float TubeStage3(float in)
{
    // Pentode-like character with soft knee compression
    // No compensation - cascades with previous stages
    float out = SoftLimit(in * 1.4f);
    // Add 3rd harmonic for body
    out += 0.05f * out * out * out;
    return out;
}

// Stage 4: Power section - EL34 push-pull with optional crossover distortion
inline float TubeStage4(float in, float crossover_amount)
{
    // Power section - EL34 push-pull
    // No compensation - final stage of cascade
    float out = SoftLimit(in * 1.5f);
    
    // Crossover distortion: notch near zero crossing (only when crossover_amount > 0)
    if (crossover_amount > 0.f)
    {
        float zero_region = 0.08f * crossover_amount;
        if (fabsf(in) < zero_region)
        {
            float crossover_depth = 0.3f * crossover_amount;
            float blend = fabsf(in) / zero_region;
            out *= (1.f - crossover_depth) + crossover_depth * blend;
        }
    }
    
    // Slight asymmetric compression on positive peaks (grid blocking)
    if (in > 0.4f)
        out *= 0.95f;
    
    return out;
}

// Rectifier sag model - affects all stages, more pronounced at negative bias
inline float ApplySag(float in, float bias, AlgorithmState& state)
{
    // Sag is always present but intensity varies with bias
    // Negative bias = more sag (tube rectifier feel)
    // Positive bias = less sag (tighter, diode rectifier feel)
    float sag_intensity = fclamp(0.5f - bias * 0.5f, 0.1f, 1.f);  // 0.1 to 1.0
    
    float abs_in = fabsf(in);
    
    // Envelope follower with asymmetric attack/release
    // Fast attack (transients cause immediate sag)
    // Slow release (voltage "blooms" back)
    float attack_coeff = 0.08f * sag_intensity;
    float release_coeff = 0.003f + (1.f - sag_intensity) * 0.007f;
    float coeff = abs_in > state.tube_env ? attack_coeff : release_coeff;
    state.tube_env += coeff * (abs_in - state.tube_env);
    
    // Voltage drop proportional to envelope (logarithmic response)
    float voltage_drop = fastlogf(1.f + state.tube_env * sag_intensity * 3.f) * 0.3f;
    float sag_gain = 1.f / (1.f + voltage_drop);
    
    return in * sag_gain;
}

// Main tube processing function
// Bias-progressive multi-stage gain architecture:
//   -1.0 to -0.5 = 1 stage (starved/broken)
//   -0.5 to  0.0 = 1-2 stages (sag/warm)
//    0.0 to  0.5 = 2-3 stages (classic/body)
//    0.5 to  1.0 = 3-4 stages (full stack with crossover distortion at 0.9+)
inline float TubeProcess(float in, float bias, AlgorithmState& state)
{
    // Calculate number of active stages based on bias
    // bias -1 = 1 stage, bias 0 = 2.5 stages, bias +1 = 4 stages
    float num_stages = 1.f + (bias + 1.f) * 1.5f;  // Range: 1 to 4
    
    // Coupling capacitor (DC blocking between stages)
    float coupling = 0.995f;
    state.tube_cap = coupling * state.tube_cap + (1.f - coupling) * in;
    float signal = in - state.tube_cap;
    
    // Track cumulative gain for final compensation
    float cumulative_gain = 1.f;
    
    // === Stage 1: Always active - input buffer ===
    signal = TubeStage1(signal, bias);
    cumulative_gain *= 1.25f;  // ~1.2-1.3x gain
    
    // Early exit for minimal staging (bias < -0.5)
    if (num_stages < 1.5f)
    {
        // Starved mode: add dead zone for broken amp feel
        float starve = fclamp(1.5f - num_stages, 0.f, 1.f);
        float dead_zone = 0.05f + starve * 0.1f;
        if (fabsf(signal) < dead_zone)
            signal *= 0.2f + 0.8f * (fabsf(signal) / dead_zone);
        signal = ApplySag(signal, bias, state);
        return fclamp(signal / cumulative_gain, -1.f, 1.f);
    }
    
    // === Stage 2: Color (fades in from bias > -0.5) ===
    float stage2_mix = fclamp(num_stages - 1.f, 0.f, 1.f);
    if (stage2_mix > 0.f)
    {
        float stage2_out = TubeStage2(signal);
        signal = Lerp(signal, stage2_out, stage2_mix);
        cumulative_gain *= 1.f + stage2_mix * 0.35f;  // ~1.35x when fully mixed
    }
    
    // Early exit for 2-stage mode
    if (num_stages < 2.5f)
    {
        signal = ApplySag(signal, bias, state);
        return fclamp(signal / cumulative_gain, -1.f, 1.f);
    }
    
    // === Stage 3: Body/compression (fades in from bias > 0) ===
    float stage3_mix = fclamp(num_stages - 2.f, 0.f, 1.f);
    if (stage3_mix > 0.f)
    {
        float stage3_out = TubeStage3(signal);
        signal = Lerp(signal, stage3_out, stage3_mix);
        cumulative_gain *= 1.f + stage3_mix * 0.4f;  // ~1.4x when fully mixed
    }
    
    // Early exit for 3-stage mode
    if (num_stages < 3.5f)
    {
        signal = ApplySag(signal, bias, state);
        return fclamp(signal / cumulative_gain, -1.f, 1.f);
    }
    
    // === Stage 4: Power section (fades in from bias > 0.5) ===
    float stage4_mix = fclamp(num_stages - 3.f, 0.f, 1.f);
    if (stage4_mix > 0.f)
    {
        // Crossover distortion only in last 10% of bias range (0.9 to 1.0)
        float crossover_amount = fclamp((bias - 0.9f) * 10.f, 0.f, 1.f);
        float stage4_out = TubeStage4(signal, crossover_amount);
        signal = Lerp(signal, stage4_out, stage4_mix);
        cumulative_gain *= 1.f + stage4_mix * 0.5f;  // ~1.5x when fully mixed
    }
    
    // Apply sag and final compensation
    signal = ApplySag(signal, bias, state);
    return fclamp(signal / cumulative_gain, -1.f, 1.f);
}

}  // namespace distortion

#endif  // DISTORTION_TUBE_AMP_H
