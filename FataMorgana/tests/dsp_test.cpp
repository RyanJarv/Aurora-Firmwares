/**
 * DSP Unit Tests
 * 
 * Tests for DSP functions used in FataMorgana wavetable synthesizer.
 * Runs on host system (not embedded) to validate DSP logic before deployment.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>

// ============================================================================
// DSP Functions (extracted from oscillator.h for testability)
// ============================================================================

namespace dsp_test {

/** Convert MIDI note number to frequency.
 *  @param midi_note MIDI note (0-127, 69 = A4 = 440Hz)
 *  @return Frequency in Hz
 */
inline float MidiToFreq(float midi_note)
{
    return 440.f * powf(2.f, (midi_note - 69.f) / 12.f);
}

/** Convert V/Oct voltage to frequency.
 *  @param voct V/Oct value (0V = C0 = 16.35Hz)
 *  @return Frequency in Hz
 */
inline float VoctToFreq(float voct)
{
    // C0 = 16.35Hz at 0V, each volt = one octave
    return 16.35f * powf(2.f, voct);
}

/** Clamp value to range */
inline float fclamp(float in, float min, float max)
{
    return in < min ? min : (in > max ? max : in);
}

// ============================================================================
// Trilinear Interpolation (extracted from wave_cube.h)
// ============================================================================

static constexpr int SAMPLES_PER_WAVE = 256;
static constexpr int CUBE_SIZE = 8;

// Simple test wave cube for unit testing
struct TestWaveCube
{
    float data[CUBE_SIZE][CUBE_SIZE][CUBE_SIZE][SAMPLES_PER_WAVE];
    bool loaded;
    
    void Clear()
    {
        for (int z = 0; z < CUBE_SIZE; z++)
            for (int y = 0; y < CUBE_SIZE; y++)
                for (int x = 0; x < CUBE_SIZE; x++)
                    for (int s = 0; s < SAMPLES_PER_WAVE; s++)
                        data[z][y][x][s] = 0.f;
        loaded = false;
    }
    
    // Fill with test pattern: value = x + y*10 + z*100 at sample 0
    void FillTestPattern()
    {
        for (int z = 0; z < CUBE_SIZE; z++)
            for (int y = 0; y < CUBE_SIZE; y++)
                for (int x = 0; x < CUBE_SIZE; x++)
                    for (int s = 0; s < SAMPLES_PER_WAVE; s++)
                        data[z][y][x][s] = static_cast<float>(x + y * 10 + z * 100);
        loaded = true;
    }
    
    // Fill with sine waves of different frequencies
    void FillSinePattern()
    {
        for (int z = 0; z < CUBE_SIZE; z++)
            for (int y = 0; y < CUBE_SIZE; y++)
                for (int x = 0; x < CUBE_SIZE; x++)
                    for (int s = 0; s < SAMPLES_PER_WAVE; s++)
                    {
                        float phase = static_cast<float>(s) / SAMPLES_PER_WAVE;
                        int harmonics = 1 + x;
                        data[z][y][x][s] = sinf(2.f * 3.14159265f * phase * harmonics);
                    }
        loaded = true;
    }
};

/** Trilinear interpolation to read from the wave cube. */
inline float ReadCube(const TestWaveCube& cube, float x, float y, float z, float phase,
                      bool morph_x = true, bool morph_y = true, bool morph_z = true)
{
    // Clamp coordinates to valid range
    x = fmaxf(0.f, fminf(x, 7.f));
    y = fmaxf(0.f, fminf(y, 7.f));
    z = fmaxf(0.f, fminf(z, 7.f));
    
    // Get integer indices and fractional parts
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int z0 = static_cast<int>(z);
    
    float fx = morph_x ? (x - x0) : 0.f;
    float fy = morph_y ? (y - y0) : 0.f;
    float fz = morph_z ? (z - z0) : 0.f;
    
    // Clamp upper indices
    int x1 = (x0 < 7) ? x0 + 1 : x0;
    int y1 = (y0 < 7) ? y0 + 1 : y0;
    int z1 = (z0 < 7) ? z0 + 1 : z0;
    
    // Calculate sample index with linear interpolation
    float sample_pos = phase * (SAMPLES_PER_WAVE - 1);
    int s0 = static_cast<int>(sample_pos);
    int s1 = (s0 < SAMPLES_PER_WAVE - 1) ? s0 + 1 : 0;  // Wrap for seamless looping
    float fs = sample_pos - s0;
    
    // Fetch 8 corner samples (with sample interpolation)
    auto sample_interp = [&](int xi, int yi, int zi) -> float {
        float v0 = cube.data[zi][yi][xi][s0];
        float v1 = cube.data[zi][yi][xi][s1];
        return v0 + fs * (v1 - v0);
    };
    
    // Trilinear interpolation
    float c000 = sample_interp(x0, y0, z0);
    float c100 = sample_interp(x1, y0, z0);
    float c010 = sample_interp(x0, y1, z0);
    float c110 = sample_interp(x1, y1, z0);
    float c001 = sample_interp(x0, y0, z1);
    float c101 = sample_interp(x1, y0, z1);
    float c011 = sample_interp(x0, y1, z1);
    float c111 = sample_interp(x1, y1, z1);
    
    // Interpolate along X
    float c00 = c000 + fx * (c100 - c000);
    float c10 = c010 + fx * (c110 - c010);
    float c01 = c001 + fx * (c101 - c001);
    float c11 = c011 + fx * (c111 - c011);
    
    // Interpolate along Y
    float c0 = c00 + fy * (c10 - c00);
    float c1 = c01 + fy * (c11 - c01);
    
    // Interpolate along Z
    return c0 + fz * (c1 - c0);
}

// ============================================================================
// Phase Accumulator (extracted from oscillator.h)
// ============================================================================

class PhaseAccumulator
{
public:
    void Init(float sample_rate)
    {
        sample_rate_ = sample_rate;
        phase_ = 0.f;
        freq_ = 440.f;
    }
    
    void SetFreq(float freq) { freq_ = freq; }
    float GetPhase() const { return phase_; }
    void Reset() { phase_ = 0.f; }
    
    float Process(float fm_input = 0.f, float fm_amount = 0.f)
    {
        float fm_mod = fm_input * fm_amount * freq_;
        float effective_freq = freq_ + fm_mod;
        float phase_inc = effective_freq / sample_rate_;
        
        phase_ += phase_inc;
        while (phase_ >= 1.f) phase_ -= 1.f;
        while (phase_ < 0.f) phase_ += 1.f;
        
        return phase_;
    }
    
private:
    float sample_rate_;
    float phase_;
    float freq_;
};

}  // namespace dsp_test

// ============================================================================
// Tests: Frequency Conversion
// ============================================================================

class FrequencyConversionTest : public ::testing::Test {};

TEST_F(FrequencyConversionTest, MidiToFreq_A4)
{
    // A4 (MIDI 69) = 440 Hz
    float freq = dsp_test::MidiToFreq(69.f);
    EXPECT_NEAR(freq, 440.f, 0.001f);
}

TEST_F(FrequencyConversionTest, MidiToFreq_A3)
{
    // A3 (MIDI 57) = 220 Hz (one octave below A4)
    float freq = dsp_test::MidiToFreq(57.f);
    EXPECT_NEAR(freq, 220.f, 0.001f);
}

TEST_F(FrequencyConversionTest, MidiToFreq_A5)
{
    // A5 (MIDI 81) = 880 Hz (one octave above A4)
    float freq = dsp_test::MidiToFreq(81.f);
    EXPECT_NEAR(freq, 880.f, 0.001f);
}

TEST_F(FrequencyConversionTest, MidiToFreq_MiddleC)
{
    // C4 (MIDI 60) = 261.63 Hz
    float freq = dsp_test::MidiToFreq(60.f);
    EXPECT_NEAR(freq, 261.63f, 0.01f);
}

TEST_F(FrequencyConversionTest, MidiToFreq_OctaveRelationship)
{
    // Each octave doubles the frequency
    float freq_a3 = dsp_test::MidiToFreq(57.f);
    float freq_a4 = dsp_test::MidiToFreq(69.f);
    float freq_a5 = dsp_test::MidiToFreq(81.f);
    
    EXPECT_NEAR(freq_a4 / freq_a3, 2.f, 0.001f);
    EXPECT_NEAR(freq_a5 / freq_a4, 2.f, 0.001f);
}

TEST_F(FrequencyConversionTest, VoctToFreq_0V)
{
    // 0V = C0 = 16.35 Hz
    float freq = dsp_test::VoctToFreq(0.f);
    EXPECT_NEAR(freq, 16.35f, 0.01f);
}

TEST_F(FrequencyConversionTest, VoctToFreq_1V)
{
    // 1V = C1 = 32.7 Hz (one octave above C0)
    float freq = dsp_test::VoctToFreq(1.f);
    EXPECT_NEAR(freq, 32.7f, 0.1f);
}

TEST_F(FrequencyConversionTest, VoctToFreq_OctaveRelationship)
{
    // Each volt doubles the frequency
    float freq_0v = dsp_test::VoctToFreq(0.f);
    float freq_1v = dsp_test::VoctToFreq(1.f);
    float freq_2v = dsp_test::VoctToFreq(2.f);
    
    EXPECT_NEAR(freq_1v / freq_0v, 2.f, 0.001f);
    EXPECT_NEAR(freq_2v / freq_1v, 2.f, 0.001f);
}

// ============================================================================
// Tests: Trilinear Interpolation
// ============================================================================

class TrilinearInterpolationTest : public ::testing::Test {
protected:
    dsp_test::TestWaveCube cube;
    
    void SetUp() override {
        cube.FillTestPattern();
    }
};

TEST_F(TrilinearInterpolationTest, CornerValues)
{
    // At integer coordinates, should return exact corner values
    // Value = x + y*10 + z*100
    EXPECT_NEAR(dsp_test::ReadCube(cube, 0.f, 0.f, 0.f, 0.f), 0.f, 0.001f);
    EXPECT_NEAR(dsp_test::ReadCube(cube, 1.f, 0.f, 0.f, 0.f), 1.f, 0.001f);
    EXPECT_NEAR(dsp_test::ReadCube(cube, 0.f, 1.f, 0.f, 0.f), 10.f, 0.001f);
    EXPECT_NEAR(dsp_test::ReadCube(cube, 0.f, 0.f, 1.f, 0.f), 100.f, 0.001f);
    EXPECT_NEAR(dsp_test::ReadCube(cube, 7.f, 7.f, 7.f, 0.f), 777.f, 0.001f);
}

TEST_F(TrilinearInterpolationTest, MidpointX)
{
    // At x=0.5, should be average of x=0 and x=1
    // (0 + 1) / 2 = 0.5
    float result = dsp_test::ReadCube(cube, 0.5f, 0.f, 0.f, 0.f);
    EXPECT_NEAR(result, 0.5f, 0.001f);
}

TEST_F(TrilinearInterpolationTest, MidpointY)
{
    // At y=0.5, should be average of y=0 and y=1
    // (0 + 10) / 2 = 5
    float result = dsp_test::ReadCube(cube, 0.f, 0.5f, 0.f, 0.f);
    EXPECT_NEAR(result, 5.f, 0.001f);
}

TEST_F(TrilinearInterpolationTest, MidpointZ)
{
    // At z=0.5, should be average of z=0 and z=1
    // (0 + 100) / 2 = 50
    float result = dsp_test::ReadCube(cube, 0.f, 0.f, 0.5f, 0.f);
    EXPECT_NEAR(result, 50.f, 0.001f);
}

TEST_F(TrilinearInterpolationTest, MidpointXYZ)
{
    // At (0.5, 0.5, 0.5), should be average of all 8 corners
    // Corners: 0, 1, 10, 11, 100, 101, 110, 111
    // Average = (0+1+10+11+100+101+110+111) / 8 = 55.5
    float result = dsp_test::ReadCube(cube, 0.5f, 0.5f, 0.5f, 0.f);
    EXPECT_NEAR(result, 55.5f, 0.001f);
}

TEST_F(TrilinearInterpolationTest, MorphDisabledX)
{
    // With morph_x disabled, should snap to nearest X
    float result = dsp_test::ReadCube(cube, 0.9f, 0.f, 0.f, 0.f, false, true, true);
    EXPECT_NEAR(result, 0.f, 0.001f);  // Snaps to x=0
}

TEST_F(TrilinearInterpolationTest, MorphDisabledY)
{
    // With morph_y disabled, should snap to nearest Y
    float result = dsp_test::ReadCube(cube, 0.f, 0.9f, 0.f, 0.f, true, false, true);
    EXPECT_NEAR(result, 0.f, 0.001f);  // Snaps to y=0
}

TEST_F(TrilinearInterpolationTest, MorphDisabledZ)
{
    // With morph_z disabled, should snap to nearest Z
    float result = dsp_test::ReadCube(cube, 0.f, 0.f, 0.9f, 0.f, true, true, false);
    EXPECT_NEAR(result, 0.f, 0.001f);  // Snaps to z=0
}

TEST_F(TrilinearInterpolationTest, ClampingNegative)
{
    // Negative coordinates should clamp to 0
    float result = dsp_test::ReadCube(cube, -1.f, -1.f, -1.f, 0.f);
    EXPECT_NEAR(result, 0.f, 0.001f);
}

TEST_F(TrilinearInterpolationTest, ClampingOverflow)
{
    // Coordinates > 7 should clamp to 7
    float result = dsp_test::ReadCube(cube, 10.f, 10.f, 10.f, 0.f);
    EXPECT_NEAR(result, 777.f, 0.001f);
}

// ============================================================================
// Tests: Phase Accumulator
// ============================================================================

class PhaseAccumulatorTest : public ::testing::Test {
protected:
    dsp_test::PhaseAccumulator phase;
    
    void SetUp() override {
        phase.Init(48000.f);  // 48kHz sample rate
    }
};

TEST_F(PhaseAccumulatorTest, InitialPhase)
{
    EXPECT_NEAR(phase.GetPhase(), 0.f, 0.001f);
}

TEST_F(PhaseAccumulatorTest, PhaseAdvances)
{
    phase.SetFreq(48000.f);  // 1 cycle per sample
    phase.Process();
    EXPECT_NEAR(phase.GetPhase(), 0.f, 0.001f);  // Wrapped back to 0
}

TEST_F(PhaseAccumulatorTest, PhaseWrapsAt1)
{
    phase.SetFreq(24000.f);  // 0.5 cycles per sample
    phase.Process();
    EXPECT_NEAR(phase.GetPhase(), 0.5f, 0.001f);
    phase.Process();
    EXPECT_NEAR(phase.GetPhase(), 0.f, 0.001f);  // Wrapped
}

TEST_F(PhaseAccumulatorTest, FrequencyAccuracy)
{
    // Use a frequency that divides evenly into sample rate
    // 4800 Hz = exactly 10 samples per cycle at 48kHz
    phase.SetFreq(4800.f);
    
    // After 100 samples, should have completed exactly 10 cycles
    // Phase should be back at 0
    for (int i = 0; i < 100; i++) {
        phase.Process();
    }
    EXPECT_NEAR(phase.GetPhase(), 0.f, 0.001f);
}

TEST_F(PhaseAccumulatorTest, Reset)
{
    phase.SetFreq(1000.f);
    for (int i = 0; i < 100; i++) {
        phase.Process();
    }
    EXPECT_GT(phase.GetPhase(), 0.f);
    
    phase.Reset();
    EXPECT_NEAR(phase.GetPhase(), 0.f, 0.001f);
}

TEST_F(PhaseAccumulatorTest, NegativeFrequency)
{
    // TZFM: negative frequency should run phase backward
    phase.SetFreq(-48000.f);  // -1 cycle per sample
    
    // Start at 0, go negative, should wrap to near 1
    phase.Process();
    EXPECT_NEAR(phase.GetPhase(), 0.f, 0.001f);  // Wrapped from negative
}

TEST_F(PhaseAccumulatorTest, FMModulation)
{
    phase.SetFreq(1000.f);
    
    // With FM input of 1.0 and amount of 1.0, effective freq = 2000 Hz
    float fm_input = 1.f;
    float fm_amount = 1.f;
    
    // Phase increment should be doubled
    float phase1 = phase.Process(0.f, 0.f);  // No FM
    phase.Reset();
    float phase2 = phase.Process(fm_input, fm_amount);  // With FM
    
    EXPECT_NEAR(phase2, phase1 * 2.f, 0.001f);
}

// ============================================================================
// Tests: Clamp Function
// ============================================================================

class ClampTest : public ::testing::Test {};

TEST_F(ClampTest, WithinRange)
{
    EXPECT_NEAR(dsp_test::fclamp(0.5f, 0.f, 1.f), 0.5f, 0.001f);
}

TEST_F(ClampTest, BelowMin)
{
    EXPECT_NEAR(dsp_test::fclamp(-1.f, 0.f, 1.f), 0.f, 0.001f);
}

TEST_F(ClampTest, AboveMax)
{
    EXPECT_NEAR(dsp_test::fclamp(2.f, 0.f, 1.f), 1.f, 0.001f);
}

TEST_F(ClampTest, AtMin)
{
    EXPECT_NEAR(dsp_test::fclamp(0.f, 0.f, 1.f), 0.f, 0.001f);
}

TEST_F(ClampTest, AtMax)
{
    EXPECT_NEAR(dsp_test::fclamp(1.f, 0.f, 1.f), 1.f, 0.001f);
}

// ============================================================================
// Tests: Oscillator Integration
// ============================================================================

class OscillatorIntegrationTest : public ::testing::Test {
protected:
    dsp_test::TestWaveCube cube;
    dsp_test::PhaseAccumulator phase;
    
    void SetUp() override {
        cube.FillSinePattern();
        phase.Init(48000.f);
    }
    
    // Simulate oscillator output for N samples
    std::vector<float> GenerateSamples(int count, float freq, float x, float y, float z)
    {
        std::vector<float> samples;
        samples.reserve(count);
        
        phase.Reset();
        phase.SetFreq(freq);
        
        for (int i = 0; i < count; i++)
        {
            float p = phase.Process();
            float sample = dsp_test::ReadCube(cube, x, y, z, p);
            samples.push_back(sample);
        }
        
        return samples;
    }
    
    // Calculate RMS of samples
    float CalculateRMS(const std::vector<float>& samples)
    {
        float sum_sq = 0.f;
        for (float s : samples) {
            sum_sq += s * s;
        }
        return sqrtf(sum_sq / samples.size());
    }
    
    // Calculate zero crossings (rough frequency estimate)
    int CountZeroCrossings(const std::vector<float>& samples)
    {
        int crossings = 0;
        for (size_t i = 1; i < samples.size(); i++) {
            if ((samples[i-1] < 0.f && samples[i] >= 0.f) ||
                (samples[i-1] >= 0.f && samples[i] < 0.f)) {
                crossings++;
            }
        }
        return crossings;
    }
};

TEST_F(OscillatorIntegrationTest, GeneratesOutput)
{
    auto samples = GenerateSamples(1000, 440.f, 0.f, 0.f, 0.f);
    
    // Should have non-zero output
    float rms = CalculateRMS(samples);
    EXPECT_GT(rms, 0.1f);
}

TEST_F(OscillatorIntegrationTest, FrequencyAffectsZeroCrossings)
{
    // Higher frequency = more zero crossings
    auto samples_low = GenerateSamples(1000, 100.f, 0.f, 0.f, 0.f);
    auto samples_high = GenerateSamples(1000, 400.f, 0.f, 0.f, 0.f);
    
    int crossings_low = CountZeroCrossings(samples_low);
    int crossings_high = CountZeroCrossings(samples_high);
    
    // 4x frequency should give ~4x zero crossings
    EXPECT_GT(crossings_high, crossings_low * 3);
}

TEST_F(OscillatorIntegrationTest, MorphPositionAffectsTimbre)
{
    // Different X positions should produce different waveforms
    auto samples_x0 = GenerateSamples(1000, 440.f, 0.f, 0.f, 0.f);
    auto samples_x7 = GenerateSamples(1000, 440.f, 7.f, 0.f, 0.f);
    
    // Calculate difference
    float diff_sum = 0.f;
    for (size_t i = 0; i < samples_x0.size(); i++) {
        diff_sum += fabsf(samples_x0[i] - samples_x7[i]);
    }
    float avg_diff = diff_sum / samples_x0.size();
    
    // Should be noticeably different
    EXPECT_GT(avg_diff, 0.1f);
}

TEST_F(OscillatorIntegrationTest, OutputBounded)
{
    // Output should stay within reasonable bounds
    auto samples = GenerateSamples(10000, 440.f, 3.5f, 3.5f, 3.5f);
    
    for (float s : samples) {
        EXPECT_GE(s, -2.f);
        EXPECT_LE(s, 2.f);
    }
}

TEST_F(OscillatorIntegrationTest, PhaseWrapsCorrectly)
{
    // Run for many cycles, phase should always be 0-1
    phase.SetFreq(1000.f);
    
    for (int i = 0; i < 100000; i++) {
        float p = phase.Process();
        EXPECT_GE(p, 0.f);
        EXPECT_LT(p, 1.f);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
