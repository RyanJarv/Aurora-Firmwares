/** Wavetable Oscillator with Thru-Zero FM
 *  Custom oscillator for 3D wavetable morphing with TZFM support.
 */
#pragma once
#ifndef WAVETABLE_OSCILLATOR_H
#define WAVETABLE_OSCILLATOR_H

#include "wave_cube.h"
#include "Utility/dsp.h"

namespace wavetable
{

using daisysp::fclamp;
using daisysp::mtof;

/** Wavetable oscillator with 3D morphing and thru-zero FM.
 *  
 *  Features:
 *  - Phase accumulator supporting negative frequencies (TZFM)
 *  - Trilinear interpolation across X, Y, Z axes
 *  - Per-axis morph enable/disable for glitchy transitions
 *  - Unison mode with detuned secondary oscillator
 */
class Oscillator
{
  public:
    Oscillator() {}
    ~Oscillator() {}
    
    /** Initialize the oscillator.
     *  @param sample_rate Audio sample rate in Hz
     */
    void Init(float sample_rate)
    {
        sample_rate_ = sample_rate;
        phase_ = 0.f;
        freq_ = 440.f;
        x_ = 0.f;
        y_ = 0.f;
        z_ = 0.f;
        morph_x_ = true;
        morph_y_ = true;
        morph_z_ = true;
        fm_amount_ = 0.f;
        
        // Unison
        unison_enabled_ = false;
        unison_phase_ = 0.f;
        unison_detune_ = 0.01f;  // 1% detune
        
        // External input mode
        external_gain_ = 1.f;
    }
    
    /** Set base frequency in Hz.
     *  @param freq Frequency in Hz (can be negative for TZFM)
     */
    void SetFreq(float freq) { freq_ = freq; }
    
    /** Get current frequency. */
    float GetFreq() const { return freq_; }
    
    /** Set X-axis morph position (0.0 to 7.0). */
    void SetX(float x) { x_ = fclamp(x, 0.f, 7.f); }
    
    /** Set Y-axis morph position (0.0 to 7.0). */
    void SetY(float y) { y_ = fclamp(y, 0.f, 7.f); }
    
    /** Set Z-axis morph position (0.0 to 7.0). */
    void SetZ(float z) { z_ = fclamp(z, 0.f, 7.f); }
    
    /** Enable/disable X-axis interpolation. */
    void SetMorphX(bool enable) { morph_x_ = enable; }
    
    /** Enable/disable Y-axis interpolation. */
    void SetMorphY(bool enable) { morph_y_ = enable; }
    
    /** Enable/disable Z-axis interpolation. */
    void SetMorphZ(bool enable) { morph_z_ = enable; }
    
    /** Set FM modulation amount.
     *  @param amount FM depth (0.0 = none, 1.0 = full)
     */
    void SetFmAmount(float amount) { fm_amount_ = amount; }
    
    /** Enable/disable unison mode. */
    void SetUnison(bool enable) { unison_enabled_ = enable; }
    
    /** Set unison detune amount.
     *  @param detune Detune as fraction of frequency (0.0 to 0.1)
     */
    void SetUnisonDetune(float detune) { unison_detune_ = fclamp(detune, 0.f, 0.1f); }
    
    /** Set external input gain for waveshaper mode. */
    void SetExternalGain(float gain) { external_gain_ = gain; }
    
    /** Reset phase to zero. */
    void Reset() 
    { 
        phase_ = 0.f; 
        unison_phase_ = 0.f;
    }
    
    /** Process one sample with optional FM input.
     *  @param fm_input FM modulation signal (-1.0 to 1.0)
     *  @return Output sample
     */
    float Process(float fm_input = 0.f)
    {
        if (!wave_cube.loaded) return 0.f;
        
        // Calculate effective frequency with FM
        // TZFM: frequency can go negative, causing phase to run backward
        float fm_mod = fm_input * fm_amount_ * freq_;
        float effective_freq = freq_ + fm_mod;
        
        // Phase increment (can be negative for TZFM)
        float phase_inc = effective_freq / sample_rate_;
        
        // Update phase with wrapping (handles negative values)
        phase_ += phase_inc;
        while (phase_ >= 1.f) phase_ -= 1.f;
        while (phase_ < 0.f) phase_ += 1.f;
        
        // Read from wave cube
        float output = ReadCube(x_, y_, z_, phase_, morph_x_, morph_y_, morph_z_);
        
        // Unison: add detuned second oscillator
        if (unison_enabled_)
        {
            float unison_freq = effective_freq * (1.f + unison_detune_);
            float unison_inc = unison_freq / sample_rate_;
            
            unison_phase_ += unison_inc;
            while (unison_phase_ >= 1.f) unison_phase_ -= 1.f;
            while (unison_phase_ < 0.f) unison_phase_ += 1.f;
            
            float unison_out = ReadCube(x_, y_, z_, unison_phase_, morph_x_, morph_y_, morph_z_);
            output = (output + unison_out) * 0.5f;
        }
        
        return output;
    }
    
    /** Process one sample in external input (waveshaper) mode.
     *  External audio directly drives the wavetable phase.
     *  @param external_input External audio signal
     *  @return Waveshaped output
     */
    float ProcessExternal(float external_input)
    {
        if (!wave_cube.loaded) return 0.f;
        
        // Scale and offset external input to 0-1 phase range
        // Input is assumed to be -1 to +1, map to 0-1
        float phase = (external_input * external_gain_ + 1.f) * 0.5f;
        phase = fclamp(phase, 0.f, 0.9999f);
        
        return ReadCube(x_, y_, z_, phase, morph_x_, morph_y_, morph_z_);
    }
    
    /** Get current phase (0.0 to 1.0). */
    float GetPhase() const { return phase_; }
    
  private:
    float sample_rate_;
    float phase_;
    float freq_;
    float x_, y_, z_;
    bool morph_x_, morph_y_, morph_z_;
    float fm_amount_;
    
    // Unison
    bool unison_enabled_;
    float unison_phase_;
    float unison_detune_;
    
    // External input mode
    float external_gain_;
};

/** Convert MIDI note number to frequency.
 *  Uses DaisySP's optimized mtof() function.
 *  @param midi_note MIDI note (0-127, 69 = A4 = 440Hz)
 *  @return Frequency in Hz
 */
inline float MidiToFreq(float midi_note)
{
    return mtof(midi_note);
}

/** Convert V/Oct voltage to frequency.
 *  @param voct V/Oct value (0V = C0 = 16.35Hz)
 *  @return Frequency in Hz
 */
inline float VoctToFreq(float voct)
{
    // C0 = 16.35Hz at 0V, each volt = one octave
    // Use optimized 2^x calculation: 2^x = e^(x * ln(2))
    return 16.35f * expf(0.6931471805599453f * voct);
}

}  // namespace wavetable

#endif  // WAVETABLE_OSCILLATOR_H
