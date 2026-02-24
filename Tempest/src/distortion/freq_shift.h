/** Frequency Shifter
 *  Simplified Hilbert transform for inharmonic distortion (FAM Odd mode).
 */
#pragma once
#ifndef DISTORTION_FREQ_SHIFT_H
#define DISTORTION_FREQ_SHIFT_H

#include "types.h"

namespace distortion
{

// Frequency shifter for FAM Odd (inharmonic distortion)
// Uses a simplified Hilbert transform approximation
inline void FrequencyShift(float in, float shift_hz, float sample_rate, 
                           float& out_shifted, AlgorithmState& state)
{
    // Allpass-based Hilbert transform approximation
    // Coefficients for ~90 degree phase shift
    const float a1 = 0.94657f;
    const float a2 = 0.06338f;
    
    // First allpass (0 degrees at output)
    float temp1 = a1 * (in - state.hilbert_y1) + state.hilbert_x1;
    state.hilbert_x1 = in;
    state.hilbert_y1 = temp1;
    
    // Second allpass (90 degrees at output)  
    float temp2 = a2 * (in - state.hilbert_y2) + state.hilbert_x2;
    state.hilbert_x2 = in;
    state.hilbert_y2 = temp2;
    
    float in_phase = temp1;
    float quadrature = temp2;
    
    // Oscillator for frequency shifting
    float omega = 2.f * 3.14159f * shift_hz / sample_rate;
    state.osc_phase += omega;
    if (state.osc_phase > 6.28318f) state.osc_phase -= 6.28318f;
    if (state.osc_phase < -6.28318f) state.osc_phase += 6.28318f;
    
    float cos_osc = cosf(state.osc_phase);
    float sin_osc = sinf(state.osc_phase);
    
    // Single sideband modulation (frequency shift)
    out_shifted = in_phase * cos_osc - quadrature * sin_osc;
}

}  // namespace distortion

#endif  // DISTORTION_FREQ_SHIFT_H
