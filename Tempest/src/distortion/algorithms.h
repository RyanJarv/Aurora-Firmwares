/** Distortion Algorithms
 *  36 distortion algorithms organized by category.
 */
#pragma once
#ifndef DISTORTION_ALGORITHMS_H
#define DISTORTION_ALGORITHMS_H

#include "types.h"

namespace distortion
{

// ============================================================================
// CAT_ANALOG (Orange) - Analog Saturation & Soft Clipping
// ============================================================================

inline float Valve(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = 1.f - mod;
    float pos_gain = 1.f + asymm * 0.5f;
    float neg_gain = 1.f - asymm * 0.3f;
    if (in >= 0.f)
        return fclamp(SoftLimit(in * pos_gain), -1.f, 1.f);
    else
        return fclamp(SoftLimit(in * neg_gain), -1.f, 1.f);
}

inline float SftClp(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = fclamp(mod * 0.8f, 0.f, 0.9f);
    float pos = in >= 0.f ? in : 0.f;
    float neg = in < 0.f ? -in : 0.f;
    float out_pos = fastlogf(1.f + pos * (1.f + asymm)) / fastlogf(2.f + asymm);
    float out_neg = fastlogf(1.f + neg * (1.f - asymm * 0.5f)) / fastlogf(2.f - asymm * 0.5f);
    return fclamp(out_pos - out_neg, -1.f, 1.f);
}

inline float Cubic(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float out = in - (in * in * in) / 3.f;
    return fclamp(out * 1.5f, -1.f, 1.f);
}

inline float Disto(float x, float gain, float mod, float bias, float sample_rate, AlgorithmState& state)
{
    (void)sample_rate;  // Fixed at 48kHz
    float in = (x + bias * 0.5f) * gain;
    
    // Bandpass sidechain filter (500Hz center, Q≈0.7)
    // Removes sub-bass pumping and HF ripple from envelope detection
    // Pre-calculated for 48kHz: f = 2*sin(π*500/48000) ≈ 0.0654
    constexpr float SC_F = 0.0654f;
    constexpr float SC_Q = 1.4f;  // 1/Q where Q=0.7
    
    state.disto_sc_low += SC_F * state.disto_sc_band;
    float sc_high = in - state.disto_sc_low - SC_Q * state.disto_sc_band;
    state.disto_sc_band += SC_F * sc_high;
    float sc_signal = state.disto_sc_band;
    
    // MOD controls envelope speed: 0 = slow (pumping), 1 = fast (tight)
    // attack: 1ms (mod=1) to 50ms (mod=0) at 48kHz
    // release: 10ms (mod=1) to 200ms (mod=0) at 48kHz
    float attack_samples = 48.f + (1.f - mod) * 2352.f;   // 48 to 2400 samples
    float release_samples = 480.f + (1.f - mod) * 9120.f; // 480 to 9600 samples
    float attack_coeff = 1.f / attack_samples;
    float release_coeff = 1.f / release_samples;
    
    // Envelope follower on bandpassed sidechain signal
    float abs_sc = fabsf(sc_signal);
    if (abs_sc > state.disto_env)
        state.disto_env += attack_coeff * (abs_sc - state.disto_env);
    else
        state.disto_env += release_coeff * (abs_sc - state.disto_env);
    
    // Clamp envelope and SVF states to prevent accumulation
    state.disto_env = fclamp(state.disto_env, 0.f, 100.f);
    state.disto_sc_low = fclamp(state.disto_sc_low, -100.f, 100.f);
    state.disto_sc_band = fclamp(state.disto_sc_band, -100.f, 100.f);
    
    // Compute gain reduction: louder input = less gain
    float threshold = 0.3f;
    float env = daisysp::fmax(state.disto_env, 0.01f);
    float gain_reduction;
    
    if (env > threshold)
        gain_reduction = threshold / env;
    else
        gain_reduction = 1.f;
    
    // Apply gain reduction and soft limit
    float limited = in * gain_reduction;
    return fclamp(SoftLimit(limited), -1.f, 1.f);
}

inline float AntiPlus(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float fold = (mod - 0.5f) * 2.f;
    float out = in;
    if (fabsf(in) > 1.f)
    {
        float excess = fabsf(in) - 1.f;
        float folded = 1.f - excess * fold;
        out = folded * Sign(in);
    }
    out = SoftLimit(out * 1.5f) + 0.1f * sinf(out * 3.14159f);
    return fclamp(out, -1.f, 1.f);
}

inline float HardClp(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = mod * 0.7f;
    float thresh_pos = 1.f - asymm;
    float thresh_neg = 1.f + asymm * 0.5f;
    float out;
    if (in > thresh_pos)
        out = thresh_pos;
    else if (in < -thresh_neg)
        out = -thresh_neg;
    else
    {
        float norm = in >= 0.f ? in / thresh_pos : in / thresh_neg;
        out = SoftLimit(norm * 1.5f) * (in >= 0.f ? thresh_pos : thresh_neg);
    }
    return fclamp(out, -1.f, 1.f);
}

// ============================================================================
// CAT_FOLD (Purple) - Wavefolding & Complex Shaping
// ============================================================================

inline float WavFld(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float fold_amt = 1.f + mod * 2.f;
    return sinf(in * fold_amt * 1.57f);
}

inline float Crest(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = mod * 0.8f;
    float abs_in = fabsf(in);
    float sign = in >= 0.f ? 1.f : -1.f;
    float thresh = 0.7f + asymm * 0.2f * sign;
    if (abs_in < thresh)
        return in;
    float excess = abs_in - thresh;
    float folded = thresh - excess * 0.8f;
    return fclamp(sign * folded, -1.f, 1.f);
}

inline float Antipol(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float fold = (mod - 0.5f) * 2.f;
    if (fabsf(in) > 1.f)
    {
        float excess = fabsf(in) - 1.f;
        float folded = 1.f - excess * fold;
        return fclamp(folded * Sign(in), -1.f, 1.f);
    }
    return in;
}

inline float Sstrgi(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = mod * 0.8f;
    float ripples = sinf(in * 4.f) * 0.3f;
    float shaped = SoftLimit(in * 1.5f) + ripples * (1.f - fabsf(SoftLimit(in)));
    if (in >= 0.f)
        shaped *= (1.f + asymm * 0.3f);
    else
        shaped *= (1.f - asymm * 0.2f);
    return fclamp(shaped, -1.f, 1.f);
}

inline float StrgrC(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = mod * 0.7f;
    float shaped;
    if (in >= 0.f)
        shaped = atanf(in * (2.f + asymm)) / (1.57f + asymm * 0.5f);
    else
        shaped = atanf(in * (2.f - asymm * 0.5f)) / (1.57f - asymm * 0.25f);
    return fclamp(shaped, -1.f, 1.f);
}

inline float Cross(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = mod * 0.8f;
    float abs_in = fabsf(in);
    float sign = Sign(in);
    float crossover = 0.1f + asymm * 0.2f;
    if (abs_in < crossover)
    {
        float factor = abs_in / crossover;
        return sign * factor * factor * crossover;
    }
    return fclamp(SoftLimit(in), -1.f, 1.f);
}

// ============================================================================
// CAT_GEOMETRIC (Yellow) - Geometric & Variable Shaping
// ============================================================================

inline float Fezz(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = 0.3f + mod * 0.5f;
    if (in >= 0.f)
        return fclamp(SoftLimit(in * (1.f + asymm)), -1.f, 1.f);
    else
        return fclamp(SoftLimit(in * (1.f - asymm * 0.5f)) * (1.f + asymm * 0.3f), -1.f, 1.f);
}

inline float Fuss(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = 0.2f + mod * 0.6f;
    if (in >= 0.f)
    {
        float shaped = SoftLimit(in * 2.f);
        if (in > 0.8f)
            shaped = 0.8f - (in - 0.8f) * asymm;
        return fclamp(shaped, -1.f, 1.f);
    }
    return fclamp(SoftLimit(in * (1.5f - asymm * 0.3f)), -1.f, 1.f);
}

inline float VariWS(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float curve = 1.f + mod * 3.f;
    float sign = Sign(in);
    float abs_in = fabsf(in);
    float shaped = 1.f - powf(1.f - std::min(abs_in, 1.f), curve);
    return fclamp(shaped * sign, -1.f, 1.f);
}

inline float SmartE(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float shape = mod * 3.f;
    float sign = Sign(in);
    float abs_in = fabsf(in);
    float soft = SoftLimit(abs_in);
    float hard = std::min(abs_in, 1.f);
    float shaped = soft * (1.f - shape / 3.f) + hard * (shape / 3.f);
    if (shape > 1.f)
    {
        float fold_amt = (shape - 1.f) / 2.f;
        shaped = shaped * (1.f - fold_amt) + sinf(abs_in * 1.57f) * fold_amt;
    }
    return fclamp(shaped * sign, -1.f, 1.f);
}

inline float Vshpe(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float asymm = mod * 0.8f;
    float abs_in = fabsf(in);
    float v_shape = abs_in;
    if (in >= 0.f)
        v_shape *= (1.f + asymm * 0.5f);
    else
        v_shape *= (1.f - asymm * 0.3f);
    return fclamp(v_shape * Sign(in), -1.f, 1.f);
}

inline float Square(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    float squared = in * in;
    float out = squared * Sign(in);
    return fclamp(out, -1.f, 1.f);
}

// ============================================================================
// CAT_LOFI (Cyan) - Digital Degradation
// ============================================================================

inline float Bitcrsh(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float mod_clamped = fclamp(mod, 0.f, 1.f);
    float bits = 16.f - mod_clamped * 15.f;
    float levels = powf(2.f, bits);
    float quantized = floorf(in * levels + 0.5f) / levels;
    return fclamp(quantized, -1.f, 1.f);
}

inline float DwnSpl(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    int hold = static_cast<int>(1.f + mod * 63.f);
    state.sample_count++;
    if (state.sample_count >= hold)
    {
        state.sample_hold = fclamp(SoftLimit(in), -1.f, 1.f);
        state.sample_count = 0;
    }
    return state.sample_hold;
}

inline float Lowfi(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float slew_rate = 0.001f + mod * 0.5f;
    float diff = in - state.slew_state;
    if (fabsf(diff) > slew_rate)
        state.slew_state += slew_rate * Sign(diff);
    else
        state.slew_state = in;
    return fclamp(state.slew_state, -1.f, 1.f);
}

inline float Lowrfi(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float slew_rate = 0.001f + mod * 0.3f;
    float diff = in - state.slew_state;
    float nonlin_slew = slew_rate * (1.f + fabsf(diff) * 2.f);
    if (fabsf(diff) > nonlin_slew)
        state.slew_state += nonlin_slew * Sign(diff);
    else
        state.slew_state = in;
    float shaped = SoftLimit(state.slew_state * 1.5f);
    return fclamp(shaped, -1.f, 1.f);
}

inline float ChastB(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    union { float f; uint32_t i; } u;
    u.f = in;
    uint32_t mantissa_mask = 0x007FFFFF;
    uint32_t mantissa = u.i & mantissa_mask;
    uint32_t shift = static_cast<uint32_t>(mod * 16.f);
    mantissa = (mantissa >> shift) << shift;
    u.i = (u.i & ~mantissa_mask) | mantissa;
    float out = u.f;
    if (!std::isfinite(out)) out = 0.f;
    return fclamp(out, -1.f, 1.f);
}

inline float SuspB(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    union { float f; uint32_t i; } u;
    u.f = in;
    uint32_t exp_mask = 0x7F800000;
    uint32_t exp_bits = (u.i & exp_mask) >> 23;
    int exp_mod = static_cast<int>(mod * 8.f) - 4;
    int new_exp = static_cast<int>(exp_bits) + exp_mod;
    new_exp = std::max(0, std::min(254, new_exp));
    u.i = (u.i & ~exp_mask) | (static_cast<uint32_t>(new_exp) << 23);
    float out = u.f;
    if (!std::isfinite(out)) out = 0.f;
    return fclamp(out, -1.f, 1.f);
}

// ============================================================================
// CAT_CHAOS (Red) - Chaos, Noise & Physics
// ============================================================================

inline float Filter(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float shaped = SoftLimit(in * 2.f);
    float cutoff = 0.05f + mod * 0.4f;
    float res = 0.5f + std::min(gain / 10.f, 0.45f);
    float fb = res * (state.filter_state - state.filter_lp);
    state.filter_lp += cutoff * (shaped - state.filter_lp + fb);
    state.filter_state = state.filter_lp;
    return fclamp(state.filter_lp, -1.f, 1.f);
}

inline float Bounce(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float gravity = 0.01f + mod * 0.1f;
    float friction = 0.98f - mod * 0.1f;
    float top = in;
    float bottom = -in;
    state.bounce_vel *= friction;
    state.bounce_vel += gravity * Sign(in);
    state.bounce_pos += state.bounce_vel;
    if (state.bounce_pos > top)
    {
        state.bounce_pos = top;
        state.bounce_vel = -state.bounce_vel * 0.8f;
    }
    if (state.bounce_pos < bottom)
    {
        state.bounce_pos = bottom;
        state.bounce_vel = -state.bounce_vel * 0.8f;
    }
    return fclamp(state.bounce_pos, -1.f, 1.f);
}

inline float Zippy(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float sigma = 10.f;
    float rho = 28.f;
    float beta = 8.f / 3.f;
    float dt = 0.0005f + mod * 0.002f;
    float drive = fabsf(in) * 5.f;
    float rho_mod = rho + drive * 10.f;
    float dx = sigma * (state.lorenz_y - state.lorenz_x);
    float dy = state.lorenz_x * (rho_mod - state.lorenz_z) - state.lorenz_y;
    float dz = state.lorenz_x * state.lorenz_y - beta * state.lorenz_z;
    state.lorenz_x += dx * dt;
    state.lorenz_y += dy * dt;
    state.lorenz_z += dz * dt;
    state.lorenz_x = fclamp(state.lorenz_x, -30.f, 30.f);
    state.lorenz_y = fclamp(state.lorenz_y, -30.f, 30.f);
    state.lorenz_z = fclamp(state.lorenz_z, 0.f, 60.f);
    float mod_amount = fclamp(mod, 0.f, 1.f);
    float attractor_mod = state.lorenz_x / 30.f;
    float distorted = SoftLimit(in * 1.5f);
    float modulated = distorted * (1.f + attractor_mod * mod_amount * 0.5f);
    return fclamp(modulated, -1.f, 1.f);
}

inline float Fractl(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    int passes = 1 + static_cast<int>(mod * 4.f);
    float out = in;
    for (int i = 0; i < passes; i++)
    {
        out = out * out - 0.5f + in * 0.3f;
        out = fclamp(out, -2.f, 2.f);
    }
    return fclamp(SoftLimit(out), -1.f, 1.f);
}

inline float NoizE(float x, float gain, float mod, float bias)
{
    float in = (x + bias * 0.5f) * gain;
    if (fabsf(in) < 0.0001f) return 0.f;
    float phase = fmodf(fabsf(in), 2.f);
    float tri;
    if (phase < 1.f)
        tri = phase * 2.f - 1.f;
    else
        tri = 3.f - phase * 2.f;
    return fclamp(tri * Sign(in), -1.f, 1.f);
}

inline float BrwnNz(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float sample_rate_factor = 0.01f + mod * 0.99f;
    state.sample_count++;
    int hold = static_cast<int>(1.f / sample_rate_factor);
    if (state.sample_count >= hold)
    {
        float step = (RandomFloat(state) - 0.5f) * 0.2f;
        state.sample_hold += step + in * 0.1f;
        state.sample_hold = fclamp(state.sample_hold, -1.f, 1.f);
        state.sample_count = 0;
    }
    return state.sample_hold;
}

// ============================================================================
// CAT_TEMPORAL (Green) - Temporal, Filtered & Experimental
// ============================================================================

inline float Attrac(float x, float gain, float mod, float bias, float sample_rate, AlgorithmState& state)
{
    (void)sample_rate;  // Fixed at 48kHz
    float in = (x + bias * 0.5f) * gain;
    if (fabsf(in) < 0.0001f)
    {
        state.filter_state *= 0.999f;
        return 0.f;
    }
    
    // Envelope follower: 1ms (mod=0) to 100ms (mod=1) at 48kHz
    // coeff = 1 / (sample_rate * time) = 1 / (48000 * time)
    float env_samples = 48.f + mod * 4752.f;  // 48 to 4800 samples
    float env_coeff = 1.f / env_samples;
    
    float abs_in = fabsf(in);
    state.filter_state += env_coeff * (abs_in - state.filter_state);
    state.filter_state = fclamp(state.filter_state, 0.f, 10.f);
    
    float chaos = 1.f + gain * 0.5f;
    float attractor = (in >= 0.f) ? 1.f : -1.f;
    float pull = attractor * chaos * (1.f - state.filter_state);
    float out = in + pull * 0.3f;
    return fclamp(SoftLimit(out), -1.f, 1.f);
}

inline float TimeFr(float x, float gain, float mod, float bias, float sample_rate, AlgorithmState& state)
{
    (void)sample_rate;  // Fixed at 48kHz
    float in = (x + bias * 0.5f) * gain;
    
    // Envelope follower: 1ms (mod=0) to 50ms (mod=1) at 48kHz
    float env_samples = 48.f + mod * 2352.f;  // 48 to 2400 samples
    float env_coeff = 1.f / env_samples;
    
    float abs_in = fabsf(in);
    state.filter_state += env_coeff * (abs_in - state.filter_state);
    state.filter_state = fclamp(state.filter_state, 0.f, 10.f);
    
    float threshold = 0.1f + bias * 0.4f;
    if (state.filter_state > threshold)
        state.sample_hold = in;
    return fclamp(state.sample_hold, -1.f, 1.f);
}

inline float Vinyl(float x, float gain, float mod, float bias, float sample_rate, AlgorithmState& state)
{
    float in = x * gain;
    float pre_emphasis_freq = 2122.f;
    float de_emphasis_freq = 500.f;
    float omega_pre = 2.f * 3.14159f * pre_emphasis_freq / sample_rate;
    float omega_de = 2.f * 3.14159f * de_emphasis_freq / sample_rate;
    float coeff_pre = omega_pre / (1.f + omega_pre);
    float coeff_de = omega_de / (1.f + omega_de);
    state.filter_lp += coeff_pre * (in - state.filter_lp);
    float pre_emph = in + (in - state.filter_lp) * 2.f;
    float shaped = SoftLimit(pre_emph * 1.2f);
    shaped += shaped * shaped * 0.1f * Sign(shaped);
    state.filter_state += coeff_de * (shaped - state.filter_state);
    float degradation = mod * 0.3f;
    float out = state.filter_state * (1.f - degradation) + shaped * degradation;
    return fclamp(out, -1.f, 1.f);
}

inline float Clicks(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = (x + bias * 0.5f) * gain;
    float click_density = mod * 0.01f;
    float out = in;
    if (RandomFloat(state) < click_density)
    {
        float click_amp = 0.3f + RandomFloat(state) * 0.5f;
        float click_sign = (RandomFloat(state) < 0.5f) ? 1.f : -1.f;
        out += click_amp * click_sign;
    }
    return fclamp(out, -1.f, 1.f);
}

inline float Jelly(float x, float gain, float mod, float bias, float sample_rate, AlgorithmState& state)
{
    (void)sample_rate;  // Fixed at 48kHz
    float in = (x + bias * 0.5f) * gain;
    
    // Envelope follower: 10ms (mod=0) to 110ms (mod=1) at 48kHz
    float env_samples = 480.f + mod * 4800.f;  // 480 to 5280 samples
    float env_coeff = 1.f / env_samples;
    
    float abs_in = fabsf(in);
    state.filter_state += env_coeff * (abs_in - state.filter_state);
    state.filter_state = fclamp(state.filter_state, 0.f, 10.f);
    
    float env = state.filter_state;
    float shaped = in * (1.f + env * 3.f);
    shaped = SoftLimit(shaped);
    shaped *= (1.f - env * 0.5f);
    return fclamp(shaped, -1.f, 1.f);
}

inline float Accum(float x, float gain, float mod, float bias, AlgorithmState& state)
{
    float in = x * gain;
    float accum_rate = 0.001f + mod * 0.01f;
    state.filter_state += in * accum_rate;
    state.filter_state *= 0.999f;
    state.filter_state = fclamp(state.filter_state, -1.f, 1.f);
    float out = in + state.filter_state * 0.5f;
    return fclamp(SoftLimit(out), -1.f, 1.f);
}

}  // namespace distortion

#endif  // DISTORTION_ALGORITHMS_H
