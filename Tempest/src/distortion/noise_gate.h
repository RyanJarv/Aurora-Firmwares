/** Noise Gate
 *  Reduces noise floor on high gain settings.
 */
#pragma once
#ifndef DISTORTION_NOISE_GATE_H
#define DISTORTION_NOISE_GATE_H

#include "types.h"

namespace distortion
{

class NoiseGate
{
public:
    void Init(float sample_rate)
    {
        sample_rate_ = sample_rate;
        envelope_ = 0.f;
        gain_ = 1.f;
        
        // Fast attack, slower release
        attack_coeff_ = expf(-1.f / (sample_rate_ * 0.001f));   // 1ms
        release_coeff_ = expf(-1.f / (sample_rate_ * 0.05f));   // 50ms
    }
    
    void SetThreshold(float thresh)
    {
        threshold_ = fclamp(thresh, 0.f, 1.f);
    }
    
    float Process(float in)
    {
        float abs_in = fabsf(in);
        
        // Envelope follower
        if (abs_in > envelope_)
            envelope_ = attack_coeff_ * envelope_ + (1.f - attack_coeff_) * abs_in;
        else
            envelope_ = release_coeff_ * envelope_ + (1.f - release_coeff_) * abs_in;
        
        // Gate with soft knee
        float target_gain;
        if (envelope_ < threshold_)
        {
            // Below threshold: attenuate
            float ratio = envelope_ / daisysp::fmax(threshold_, 0.0001f);
            target_gain = ratio * ratio;  // Quadratic curve for soft knee
        }
        else
        {
            target_gain = 1.f;
        }
        
        // Smooth gain changes
        gain_ += 0.01f * (target_gain - gain_);
        
        return in * gain_;
    }
    
private:
    float sample_rate_ = 48000.f;
    float envelope_ = 0.f;
    float gain_ = 1.f;
    float threshold_ = 0.01f;
    float attack_coeff_ = 0.f;
    float release_coeff_ = 0.f;
};

}  // namespace distortion

#endif  // DISTORTION_NOISE_GATE_H
