/** Crossover Filter
 *  Splits signal into low and high frequency bands.
 */
#pragma once
#ifndef DISTORTION_CROSSOVER_H
#define DISTORTION_CROSSOVER_H

#include "types.h"

namespace distortion
{

class Crossover
{
public:
    void Init(float sample_rate)
    {
        sample_rate_ = sample_rate;
        lp_state_ = 0.f;
        cutoff_freq_ = 200.f;
        UpdateCoefficient();
    }
    
    void SetFrequency(float freq)
    {
        cutoff_freq_ = fclamp(freq, 20.f, 2000.f);
        UpdateCoefficient();
    }
    
    float GetFrequency() const
    {
        return cutoff_freq_;
    }
    
    void Process(float in, float& low_out, float& high_out)
    {
        // Simple one-pole lowpass
        lp_state_ += coeff_ * (in - lp_state_);
        low_out = lp_state_;
        high_out = in - lp_state_;
    }
    
private:
    void UpdateCoefficient()
    {
        // One-pole coefficient
        float omega = 2.f * 3.14159f * cutoff_freq_ / sample_rate_;
        coeff_ = omega / (1.f + omega);
    }
    
    float sample_rate_ = 48000.f;
    float lp_state_ = 0.f;
    float cutoff_freq_ = 200.f;
    float coeff_ = 0.f;
};

}  // namespace distortion

#endif  // DISTORTION_CROSSOVER_H
