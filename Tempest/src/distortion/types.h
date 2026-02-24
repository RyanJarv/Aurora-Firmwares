/** Distortion Types and Utilities
 *  Core types, enums, and utility functions for the distortion engine.
 */
#pragma once
#ifndef DISTORTION_TYPES_H
#define DISTORTION_TYPES_H

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "Utility/dsp.h"

namespace distortion
{

// Use DaisySP optimized functions (ARM assembly on Cortex-M7)
using daisysp::fclamp;
using daisysp::pow10f;
using daisysp::fastlog2f;
using daisysp::SoftClip;
using daisysp::SoftLimit;

// Fast natural log using fastlog2f: ln(x) = log2(x) * ln(2)
inline float fastlogf(float x)
{
    return daisysp::fastlog2f(x) * 0.6931472f;
}

inline float Sign(float x)
{
    return x >= 0.f ? 1.f : -1.f;
}

// Categories (matches CATEGORIES array and ALGO_NAMES)
enum Category
{
    CAT_ANALOG = 0,    // Orange - Analog Saturation & Soft Clipping
    CAT_FOLD,          // Purple - Wavefolding & Complex Shaping
    CAT_GEOMETRIC,     // Yellow - Geometric & Variable Shaping
    CAT_LOFI,          // Cyan - Digital Degradation (Lo-Fi)
    CAT_CHAOS,         // Red - Chaos, Noise & Physics
    CAT_TEMPORAL,      // Green - Temporal, Filtered & Experimental
    CAT_LAST
};

// FAM modes
enum FamMode
{
    FAM_STANDARD = 0,  // S - Raw algorithm with DC offset bias
    FAM_XXX,           // X - Tube amp simulation
    FAM_ODD,           // O - Inharmonic distortion
    FAM_LAST
};

// Max algorithms per category
static constexpr int MAX_ALGOS_PER_CAT = 6;

// State for stateful algorithms
struct AlgorithmState
{
    float sample_hold = 0.f;
    float slew_state = 0.f;
    float filter_state = 0.f;
    float filter_lp = 0.f;
    int sample_count = 0;
    float bounce_pos = 0.f;
    float bounce_vel = 0.f;
    float lorenz_x = 0.1f;
    float lorenz_y = 0.f;
    float lorenz_z = 0.f;
    
    // Disto limiter state
    float disto_env = 0.f;     // Envelope for limiter (start at zero, will track input)
    float disto_sc_low = 0.f;  // Sidechain SVF low state
    float disto_sc_band = 0.f; // Sidechain SVF band state
    
    // FAM Xxx (tube) state
    float tube_cap = 0.f;      // Capacitor state for tube warmth
    float tube_env = 0.f;      // Envelope for rectifier sag simulation
    
    // FAM Odd (frequency shifter) state
    float hilbert_x1 = 0.f;
    float hilbert_x2 = 0.f;
    float hilbert_y1 = 0.f;
    float hilbert_y2 = 0.f;
    float osc_phase = 0.f;
    
    // DC blocker state for FAM modes
    float dc_block_x1 = 0.f;
    float dc_block_y1 = 0.f;
    
    // Per-state PRNG (xorshift32) for thread-safe random generation
    uint32_t prng_state = 0x12345678;
};

// Per-state xorshift32 PRNG - returns float in [0, 1)
inline float RandomFloat(AlgorithmState& state)
{
    uint32_t x = state.prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state.prng_state = x;
    return static_cast<float>(x) / 4294967296.f;
}

// DC blocking filter (high-pass at ~10Hz for 48kHz sample rate)
inline float DCBlock(float in, AlgorithmState& state)
{
    const float R = 0.9995f;  // ~10Hz cutoff at 48kHz
    float out = in - state.dc_block_x1 + R * state.dc_block_y1;
    state.dc_block_x1 = in;
    state.dc_block_y1 = out;
    return out;
}

}  // namespace distortion

#endif  // DISTORTION_TYPES_H
