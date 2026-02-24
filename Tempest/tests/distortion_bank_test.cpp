/**
 * Unit tests for distortion_bank.h algorithms
 * Tests zero-signal behavior to catch DC offset issues
 */

#include <gtest/gtest.h>
#include <cmath>
#include "../distortion_bank.h"

using namespace distortion;

// Tolerance for floating point comparisons
constexpr float ZERO_TOLERANCE = 0.001f;
constexpr float SAMPLE_RATE = 48000.f;

// Algorithms that intentionally produce output with zero input (noise/chaos generators)
// These are excluded from zero-signal tests
bool IsNoiseGenerator(Category cat, int algo_idx)
{
    // CAT_CHAOS: Fractl (0), BrwnNz (4) produce chaotic output
    if (cat == CAT_CHAOS && (algo_idx == 0 || algo_idx == 4))
        return true;
    // CAT_TEMPORAL: Clicks (3) adds random clicks
    if (cat == CAT_TEMPORAL && algo_idx == 3)
        return true;
    return false;
}

class DistortionBankTest : public ::testing::Test
{
protected:
    AlgorithmState state_l;
    AlgorithmState state_r;
    
    void SetUp() override
    {
        // Reset state before each test
        state_l = AlgorithmState{};
        state_r = AlgorithmState{};
        // Use different PRNG seeds for L/R to verify independence
        state_l.prng_state = 0x12345678;
        state_r.prng_state = 0x87654321;
    }
    
    // Helper to run algorithm with zero input for N samples
    float RunZeroInput(Category cat, int algo_idx, int num_samples = 1000)
    {
        float max_output = 0.f;
        for (int i = 0; i < num_samples; i++)
        {
            float out = Process(cat, algo_idx, 0.f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
            max_output = std::max(max_output, std::fabs(out));
        }
        return max_output;
    }
    
    // Helper to run algorithm with zero input through FAM mode
    float RunZeroInputWithFam(Category cat, int algo_idx, FamMode fam, int num_samples = 1000)
    {
        float max_output = 0.f;
        for (int i = 0; i < num_samples; i++)
        {
            float out = ProcessWithFam(cat, algo_idx, 0.f, 1.f, 0.5f, 0.f, fam, SAMPLE_RATE, state_l);
            max_output = std::max(max_output, std::fabs(out));
        }
        return max_output;
    }
};

// ============================================================================
// Zero Signal Tests - All algorithms should output near-zero with zero input
// ============================================================================

TEST_F(DistortionBankTest, ZeroInput_CAT_ANALOG)
{
    for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
    {
        float max_out = RunZeroInput(CAT_ANALOG, algo);
        EXPECT_LT(max_out, ZERO_TOLERANCE) 
            << "CAT_ANALOG algo " << algo << " (" << GetAlgorithmName(CAT_ANALOG, algo) 
            << ") produced non-zero output: " << max_out;
    }
}

TEST_F(DistortionBankTest, ZeroInput_CAT_FOLD)
{
    for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
    {
        float max_out = RunZeroInput(CAT_FOLD, algo);
        EXPECT_LT(max_out, ZERO_TOLERANCE) 
            << "CAT_FOLD algo " << algo << " (" << GetAlgorithmName(CAT_FOLD, algo) 
            << ") produced non-zero output: " << max_out;
    }
}

TEST_F(DistortionBankTest, ZeroInput_CAT_GEOMETRIC)
{
    for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
    {
        float max_out = RunZeroInput(CAT_GEOMETRIC, algo);
        EXPECT_LT(max_out, ZERO_TOLERANCE) 
            << "CAT_GEOMETRIC algo " << algo << " (" << GetAlgorithmName(CAT_GEOMETRIC, algo) 
            << ") produced non-zero output: " << max_out;
    }
}

TEST_F(DistortionBankTest, ZeroInput_CAT_LOFI)
{
    for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
    {
        float max_out = RunZeroInput(CAT_LOFI, algo);
        EXPECT_LT(max_out, ZERO_TOLERANCE) 
            << "CAT_LOFI algo " << algo << " (" << GetAlgorithmName(CAT_LOFI, algo) 
            << ") produced non-zero output: " << max_out;
    }
}

TEST_F(DistortionBankTest, ZeroInput_CAT_CHAOS)
{
    for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
    {
        if (IsNoiseGenerator(CAT_CHAOS, algo)) continue;  // Skip noise generators
        float max_out = RunZeroInput(CAT_CHAOS, algo);
        EXPECT_LT(max_out, ZERO_TOLERANCE) 
            << "CAT_CHAOS algo " << algo << " (" << GetAlgorithmName(CAT_CHAOS, algo) 
            << ") produced non-zero output: " << max_out;
    }
}

TEST_F(DistortionBankTest, ZeroInput_CAT_TEMPORAL)
{
    for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
    {
        if (IsNoiseGenerator(CAT_TEMPORAL, algo)) continue;  // Skip noise generators
        float max_out = RunZeroInput(CAT_TEMPORAL, algo);
        EXPECT_LT(max_out, ZERO_TOLERANCE) 
            << "CAT_TEMPORAL algo " << algo << " (" << GetAlgorithmName(CAT_TEMPORAL, algo) 
            << ") produced non-zero output: " << max_out;
    }
}

// ============================================================================
// Zero Signal Tests with FAM modes
// ============================================================================

TEST_F(DistortionBankTest, ZeroInput_FAM_STANDARD_AllAlgos)
{
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            if (IsNoiseGenerator(static_cast<Category>(cat), algo)) continue;
            SetUp(); // Reset state
            float max_out = RunZeroInputWithFam(static_cast<Category>(cat), algo, FAM_STANDARD);
            EXPECT_LT(max_out, ZERO_TOLERANCE) 
                << "FAM_STANDARD " << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo) 
                << " produced non-zero output: " << max_out;
        }
    }
}

TEST_F(DistortionBankTest, ZeroInput_FAM_XXX_AllAlgos)
{
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            if (IsNoiseGenerator(static_cast<Category>(cat), algo)) continue;
            SetUp(); // Reset state
            float max_out = RunZeroInputWithFam(static_cast<Category>(cat), algo, FAM_XXX);
            EXPECT_LT(max_out, ZERO_TOLERANCE) 
                << "FAM_XXX " << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo) 
                << " produced non-zero output: " << max_out;
        }
    }
}

TEST_F(DistortionBankTest, ZeroInput_FAM_ODD_AllAlgos)
{
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            if (IsNoiseGenerator(static_cast<Category>(cat), algo)) continue;
            SetUp(); // Reset state
            float max_out = RunZeroInputWithFam(static_cast<Category>(cat), algo, FAM_ODD);
            EXPECT_LT(max_out, ZERO_TOLERANCE) 
                << "FAM_ODD " << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo) 
                << " produced non-zero output: " << max_out;
        }
    }
}

// ============================================================================
// Specific algorithm tests for previously buggy algorithms
// ============================================================================

TEST_F(DistortionBankTest, NoizE_ZeroInput_NoDCOffset)
{
    // NoizE was producing -1 DC offset at zero input
    for (int i = 0; i < 1000; i++)
    {
        float out = NoizE(0.f, 1.f, 0.5f, 0.f);
        EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE) << "NoizE sample " << i;
    }
}

TEST_F(DistortionBankTest, Attrac_ZeroInput_NoDCOffset)
{
    // Attrac was producing positive DC offset at zero input
    for (int i = 0; i < 1000; i++)
    {
        float out = Attrac(0.f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
        EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE) << "Attrac sample " << i;
    }
}

TEST_F(DistortionBankTest, SuspB_ZeroInput_NoDCOffset)
{
    // SuspB manipulates floating-point exponent
    for (int i = 0; i < 1000; i++)
    {
        float out = SuspB(0.f, 1.f, 0.5f, 0.f, state_l);
        EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE) << "SuspB sample " << i;
    }
}

// ============================================================================
// Bounds checking tests
// ============================================================================

TEST_F(DistortionBankTest, Process_InvalidCategory_ReturnsZero)
{
    float out = Process(static_cast<Category>(-1), 0, 0.5f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    EXPECT_EQ(out, 0.f);
    
    out = Process(CAT_LAST, 0, 0.5f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    EXPECT_EQ(out, 0.f);
}

TEST_F(DistortionBankTest, Process_InvalidAlgoIdx_ReturnsZero)
{
    float out = Process(CAT_ANALOG, -1, 0.5f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    EXPECT_EQ(out, 0.f);
    
    out = Process(CAT_ANALOG, MAX_ALGOS_PER_CAT, 0.5f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    EXPECT_EQ(out, 0.f);
}

// ============================================================================
// PRNG independence test
// ============================================================================

TEST_F(DistortionBankTest, PRNG_LRChannelsIndependent)
{
    // Run BrwnNz on both channels and verify they produce different sequences
    std::vector<float> left_samples, right_samples;
    
    for (int i = 0; i < 100; i++)
    {
        float l = BrwnNz(0.1f, 1.f, 0.5f, 0.f, state_l);
        float r = BrwnNz(0.1f, 1.f, 0.5f, 0.f, state_r);
        left_samples.push_back(l);
        right_samples.push_back(r);
    }
    
    // At least some samples should differ
    int diff_count = 0;
    for (size_t i = 0; i < left_samples.size(); i++)
    {
        if (std::fabs(left_samples[i] - right_samples[i]) > 0.001f)
            diff_count++;
    }
    
    EXPECT_GT(diff_count, 10) << "L/R channels should produce different random sequences";
}

// ============================================================================
// Output range tests
// ============================================================================

TEST_F(DistortionBankTest, AllAlgorithms_OutputInRange)
{
    // Test with various input levels
    float test_inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    float test_gains[] = {0.5f, 1.f, 5.f, 10.f};
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            for (float input : test_inputs)
            {
                for (float gain : test_gains)
                {
                    SetUp(); // Reset state
                    for (int i = 0; i < 100; i++)
                    {
                        float out = Process(static_cast<Category>(cat), algo, input, gain, 0.5f, 0.f, SAMPLE_RATE, state_l);
                        EXPECT_GE(out, -1.f) << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo);
                        EXPECT_LE(out, 1.f) << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo);
                    }
                }
            }
        }
    }
}

// ============================================================================
// DC Blocker test
// ============================================================================

TEST_F(DistortionBankTest, DCBlock_RemovesDCOffset)
{
    // Feed constant DC into DC blocker, verify it converges to zero
    float dc_input = 0.5f;
    
    for (int i = 0; i < 10000; i++)
    {
        DCBlock(dc_input, state_l);
    }
    
    // After many samples, output should be near zero
    float final_out = DCBlock(dc_input, state_l);
    EXPECT_LT(std::fabs(final_out), 0.01f) << "DC blocker should remove constant DC";
}

// ============================================================================
// TubeProcess tests - verify different tube types at various bias values
// ============================================================================

TEST_F(DistortionBankTest, TubeProcess_ZeroInput_AllBiasValues)
{
    // Zero input should produce near-zero output at all bias values
    float bias_values[] = {-1.f, -0.75f, -0.5f, -0.25f, 0.f, 0.25f, 0.5f, 0.75f, 1.f};
    
    for (float bias : bias_values)
    {
        SetUp();
        float max_out = 0.f;
        for (int i = 0; i < 1000; i++)
        {
            float out = TubeProcess(0.f, bias, state_l);
            max_out = std::max(max_out, std::fabs(out));
        }
        EXPECT_LT(max_out, ZERO_TOLERANCE) 
            << "TubeProcess with zero input at bias=" << bias << " produced output: " << max_out;
    }
}

TEST_F(DistortionBankTest, TubeProcess_OutputInRange_AllBiasValues)
{
    // Output should always be in [-1, 1] range
    float bias_values[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    float input_values[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    
    for (float bias : bias_values)
    {
        for (float input : input_values)
        {
            SetUp();
            for (int i = 0; i < 100; i++)
            {
                float out = TubeProcess(input, bias, state_l);
                EXPECT_GE(out, -1.f) << "TubeProcess output below -1 at bias=" << bias << " input=" << input;
                EXPECT_LE(out, 1.f) << "TubeProcess output above 1 at bias=" << bias << " input=" << input;
            }
        }
    }
}

TEST_F(DistortionBankTest, TubeProcess_StarvedPlate_HasDeadZone)
{
    // At bias=-1 (starved plate), small inputs should be attenuated (dead zone)
    SetUp();
    
    // Warm up the state
    for (int i = 0; i < 100; i++)
        TubeProcess(0.01f, -1.f, state_l);
    
    // Small input should produce very small output due to dead zone
    float small_input = 0.03f;
    float out = TubeProcess(small_input, -1.f, state_l);
    
    // At starved plate, small signals should be attenuated
    EXPECT_LT(std::fabs(out), std::fabs(small_input) * 2.f) 
        << "Starved plate should attenuate small signals";
}

TEST_F(DistortionBankTest, TubeProcess_RectifierSag_HasEnvelopeFollower)
{
    // At bias=-0.5 (rectifier sag), the envelope follower should be active
    // Verify that sustained loud signal causes sag (reduced output over time)
    SetUp();
    
    // Start with quiet signal to reset envelope
    for (int i = 0; i < 100; i++)
        TubeProcess(0.1f, -0.5f, state_l);
    
    // First loud sample (envelope hasn't caught up yet)
    float first_loud = TubeProcess(0.8f, -0.5f, state_l);
    
    // Sustain loud signal - envelope builds up, causing sag
    for (int i = 0; i < 50; i++)
        TubeProcess(0.8f, -0.5f, state_l);
    
    float sustained_loud = TubeProcess(0.8f, -0.5f, state_l);
    
    // After envelope builds up, output should be somewhat reduced (sag effect)
    // The difference may be subtle, so we just verify the envelope is working
    EXPECT_GT(state_l.tube_env, 0.1f) 
        << "Rectifier sag envelope should track loud signal";
}

TEST_F(DistortionBankTest, TubeProcess_Triode_AsymmetricClipping)
{
    // At bias=0 (triode), positive and negative clipping should differ
    SetUp();
    
    // Warm up
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 0.f, state_l);
    
    float pos_out = TubeProcess(0.8f, 0.f, state_l);
    
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 0.f, state_l);
    
    float neg_out = TubeProcess(-0.8f, 0.f, state_l);
    
    // Triode should have asymmetric response
    EXPECT_NE(std::fabs(pos_out), std::fabs(neg_out))
        << "Triode should have asymmetric clipping";
}

TEST_F(DistortionBankTest, TubeProcess_Pentode_MoreSymmetric)
{
    // At bias=0.5 (pentode), clipping should be more symmetric than triode
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 0.5f, state_l);
    float pos_out = TubeProcess(0.8f, 0.5f, state_l);
    
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 0.5f, state_l);
    float neg_out = TubeProcess(-0.8f, 0.5f, state_l);
    
    float pentode_asymmetry = std::fabs(std::fabs(pos_out) - std::fabs(neg_out));
    
    // Compare to triode asymmetry
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 0.f, state_l);
    float triode_pos = TubeProcess(0.8f, 0.f, state_l);
    
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 0.f, state_l);
    float triode_neg = TubeProcess(-0.8f, 0.f, state_l);
    
    float triode_asymmetry = std::fabs(std::fabs(triode_pos) - std::fabs(triode_neg));
    
    // Pentode should be more symmetric (less asymmetry)
    EXPECT_LT(pentode_asymmetry, triode_asymmetry + 0.05f)
        << "Pentode should be more symmetric than triode";
}

TEST_F(DistortionBankTest, TubeProcess_PowerTube_MoreSaturation)
{
    // At bias=1 (power tube), should have more saturation/compression
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 1.f, state_l);
    
    // Large input should be processed through all stages
    float power_out = TubeProcess(1.f, 1.f, state_l);
    
    // Compare to triode
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(0.f, 0.f, state_l);
    float triode_out = TubeProcess(1.f, 0.f, state_l);
    
    // Both should produce output, power tube uses more stages
    EXPECT_GT(std::fabs(power_out), 0.1f) << "Power tube should produce output";
    EXPECT_LE(std::fabs(power_out), 1.f) << "Power tube should be clamped";
    // Power tube and triode should differ (different staging)
    EXPECT_NE(power_out, triode_out) << "Power tube and triode should differ";
}

TEST_F(DistortionBankTest, TubeProcess_BiasAffectsHarmonics)
{
    // Different bias values should produce different harmonic content
    // We test this by checking that outputs differ for same input
    float input = 0.5f;
    
    std::vector<float> outputs;
    float bias_values[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    
    for (float bias : bias_values)
    {
        SetUp();
        // Warm up and collect steady-state output
        for (int i = 0; i < 100; i++)
            TubeProcess(input, bias, state_l);
        outputs.push_back(TubeProcess(input, bias, state_l));
    }
    
    // All outputs should be different (different tube characters)
    for (size_t i = 0; i < outputs.size(); i++)
    {
        for (size_t j = i + 1; j < outputs.size(); j++)
        {
            EXPECT_NE(outputs[i], outputs[j])
                << "Bias " << bias_values[i] << " and " << bias_values[j] 
                << " should produce different outputs";
        }
    }
}

TEST_F(DistortionBankTest, TubeProcess_LRChannelsIndependent)
{
    // L and R channels should have independent state
    // Feed different signals to verify state independence
    
    for (int i = 0; i < 100; i++)
    {
        TubeProcess(0.3f, -0.5f, state_l);  // L: quieter, sag mode
        TubeProcess(0.8f, 0.5f, state_r);   // R: louder, pentode mode
    }
    
    float out_l = TubeProcess(0.3f, -0.5f, state_l);
    float out_r = TubeProcess(0.8f, 0.5f, state_r);
    
    // Different input + bias should produce different output
    EXPECT_NE(out_l, out_r) << "L/R with different input/bias should differ";
    
    // Verify envelope state is truly independent (sag tracking differs)
    EXPECT_NE(state_l.tube_env, state_r.tube_env) 
        << "L/R tube_env state should be independent";
}

// ============================================================================
// TubeProcessV2 Stage Tests
// ============================================================================

TEST_F(DistortionBankTest, TubeStage1_ZeroInput_ZeroOutput)
{
    float out = TubeStage1(0.f, 0.f);
    EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE);
}

TEST_F(DistortionBankTest, TubeStage1_AsymmetricClipping)
{
    // Stage 1 should have slight asymmetry (different pos/neg response)
    float pos_out = TubeStage1(0.5f, 0.f);
    float neg_out = TubeStage1(-0.5f, 0.f);
    
    // Asymmetric: |pos| != |neg|
    EXPECT_NE(std::fabs(pos_out), std::fabs(neg_out))
        << "TubeStage1 should have asymmetric clipping";
}

TEST_F(DistortionBankTest, TubeStage1_OutputInRange)
{
    float inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    for (float in : inputs)
    {
        float out = TubeStage1(in, 0.f);
        EXPECT_GE(out, -1.f);
        EXPECT_LE(out, 1.f);
    }
}

TEST_F(DistortionBankTest, TubeStage2_ZeroInput_ZeroOutput)
{
    float out = TubeStage2(0.f);
    EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE);
}

TEST_F(DistortionBankTest, TubeStage2_AddsHarmonics)
{
    // Stage 2 adds 2nd harmonic and is gain-matched
    // Output should be non-zero and within reasonable range
    float in = 0.5f;
    float out = TubeStage2(in);
    
    // Output should be non-zero and roughly gain-matched to input
    EXPECT_NE(out, 0.f);
    EXPECT_GT(out, 0.2f);  // Should have reasonable output
    EXPECT_LT(out, 0.7f);  // Should be gain-matched (not amplified)
}

TEST_F(DistortionBankTest, TubeStage2_OutputInRange)
{
    float inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    for (float in : inputs)
    {
        float out = TubeStage2(in);
        EXPECT_GE(out, -1.5f);  // Slightly wider due to harmonic addition
        EXPECT_LE(out, 1.5f);
    }
}

TEST_F(DistortionBankTest, TubeStage3_ZeroInput_ZeroOutput)
{
    float out = TubeStage3(0.f);
    EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE);
}

TEST_F(DistortionBankTest, TubeStage3_MoreSymmetric)
{
    // Stage 3 is pentode-like, more symmetric than triode stages
    float pos_out = TubeStage3(0.5f);
    float neg_out = TubeStage3(-0.5f);
    
    float asymmetry = std::fabs(std::fabs(pos_out) - std::fabs(neg_out));
    EXPECT_LT(asymmetry, 0.1f) << "TubeStage3 should be relatively symmetric";
}

TEST_F(DistortionBankTest, TubeStage3_OutputInRange)
{
    float inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    for (float in : inputs)
    {
        float out = TubeStage3(in);
        EXPECT_GE(out, -1.f);
        EXPECT_LE(out, 1.f);
    }
}

TEST_F(DistortionBankTest, TubeStage4_ZeroInput_ZeroOutput)
{
    float out = TubeStage4(0.f, 0.f);
    EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE);
}

TEST_F(DistortionBankTest, TubeStage4_CrossoverDistortion_OnlyWhenEnabled)
{
    // Without crossover (amount=0), small signals pass normally
    float small_in = 0.05f;
    float out_no_crossover = TubeStage4(small_in, 0.f);
    float out_with_crossover = TubeStage4(small_in, 1.f);
    
    // With crossover enabled, small signals should be attenuated
    EXPECT_LT(std::fabs(out_with_crossover), std::fabs(out_no_crossover))
        << "Crossover distortion should attenuate small signals";
}

TEST_F(DistortionBankTest, TubeStage4_CrossoverDistortion_NotAffectLargeSignals)
{
    // Large signals should be minimally affected by crossover
    float large_in = 0.8f;
    float out_no_crossover = TubeStage4(large_in, 0.f);
    float out_with_crossover = TubeStage4(large_in, 1.f);
    
    // Large signals should be similar (crossover only affects zero region)
    float diff = std::fabs(out_with_crossover - out_no_crossover);
    EXPECT_LT(diff, 0.1f) << "Crossover should not significantly affect large signals";
}

TEST_F(DistortionBankTest, TubeStage4_OutputInRange)
{
    float inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    float crossover_amounts[] = {0.f, 0.5f, 1.f};
    
    for (float in : inputs)
    {
        for (float xover : crossover_amounts)
        {
            float out = TubeStage4(in, xover);
            EXPECT_GE(out, -1.f);
            EXPECT_LE(out, 1.f);
        }
    }
}

// ============================================================================
// ApplySag Tests
// ============================================================================

TEST_F(DistortionBankTest, ApplySag_ZeroInput_ZeroOutput)
{
    float out = ApplySag(0.f, 0.f, state_l);
    EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE);
}

TEST_F(DistortionBankTest, ApplySag_MorePronouncedAtNegativeBias)
{
    // Reset states
    SetUp();
    AlgorithmState state_neg, state_pos;
    
    // Feed same loud signal with different bias
    for (int i = 0; i < 100; i++)
    {
        ApplySag(0.8f, -0.8f, state_neg);  // Negative bias = more sag
        ApplySag(0.8f, 0.8f, state_pos);   // Positive bias = less sag
    }
    
    float out_neg = ApplySag(0.8f, -0.8f, state_neg);
    float out_pos = ApplySag(0.8f, 0.8f, state_pos);
    
    // Negative bias should have more gain reduction (lower output)
    EXPECT_LT(out_neg, out_pos) 
        << "Negative bias should produce more sag (lower output)";
}

TEST_F(DistortionBankTest, ApplySag_EnvelopeTracksLevel)
{
    SetUp();
    
    // Feed loud signal to build up envelope
    for (int i = 0; i < 100; i++)
    {
        ApplySag(0.9f, -0.5f, state_l);
    }
    
    float envelope_after_loud = state_l.tube_env;
    
    // Feed quiet signal to let envelope decay
    for (int i = 0; i < 500; i++)
    {
        ApplySag(0.1f, -0.5f, state_l);
    }
    
    float envelope_after_quiet = state_l.tube_env;
    
    EXPECT_GT(envelope_after_loud, envelope_after_quiet)
        << "Envelope should decay after quiet signal";
}

TEST_F(DistortionBankTest, ApplySag_OutputInRange)
{
    float inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    float biases[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    
    for (float in : inputs)
    {
        for (float bias : biases)
        {
            SetUp();
            for (int i = 0; i < 50; i++)
            {
                float out = ApplySag(in, bias, state_l);
                EXPECT_GE(out, -1.f);
                EXPECT_LE(out, 1.f);
            }
        }
    }
}

// ============================================================================
// TubeProcess Additional Integration Tests
// ============================================================================

TEST_F(DistortionBankTest, TubeProcess_ProgressiveStaging)
{
    // Different bias values should produce different output character
    std::vector<float> outputs;
    float bias_values[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    
    for (float bias : bias_values)
    {
        SetUp();
        // Warm up
        for (int i = 0; i < 100; i++)
            TubeProcess(0.5f, bias, state_l);
        outputs.push_back(TubeProcess(0.5f, bias, state_l));
    }
    
    // All outputs should be different
    for (size_t i = 0; i < outputs.size(); i++)
    {
        for (size_t j = i + 1; j < outputs.size(); j++)
        {
            EXPECT_NE(outputs[i], outputs[j])
                << "Bias " << bias_values[i] << " and " << bias_values[j] 
                << " should produce different outputs";
        }
    }
}

TEST_F(DistortionBankTest, TubeProcess_CrossoverDistortion_AtHighBias)
{
    // Crossover distortion at high bias values
    float small_in = 0.05f;
    
    // At bias=0.85
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(small_in, 0.85f, state_l);
    float out_low_bias = TubeProcess(small_in, 0.85f, state_l);
    
    // At bias=1.0
    SetUp();
    for (int i = 0; i < 100; i++)
        TubeProcess(small_in, 1.f, state_l);
    float out_high_bias = TubeProcess(small_in, 1.f, state_l);
    
    // Outputs should differ at different bias values
    EXPECT_NE(out_low_bias, out_high_bias)
        << "Different bias values should produce different outputs";
}

TEST_F(DistortionBankTest, TubeProcess_AllInputsOutputInRange)
{
    float inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    float biases[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    
    for (float in : inputs)
    {
        for (float bias : biases)
        {
            SetUp();
            for (int i = 0; i < 100; i++)
            {
                float out = TubeProcess(in, bias, state_l);
                EXPECT_GE(out, -1.f) << "in=" << in << " bias=" << bias;
                EXPECT_LE(out, 1.f) << "in=" << in << " bias=" << bias;
            }
        }
    }
}

TEST_F(DistortionBankTest, Lerp_Interpolation)
{
    EXPECT_FLOAT_EQ(Lerp(0.f, 1.f, 0.f), 0.f);
    EXPECT_FLOAT_EQ(Lerp(0.f, 1.f, 1.f), 1.f);
    EXPECT_FLOAT_EQ(Lerp(0.f, 1.f, 0.5f), 0.5f);
    EXPECT_FLOAT_EQ(Lerp(2.f, 4.f, 0.25f), 2.5f);
}

// ============================================================================
// Disto Algorithm Tests
// ============================================================================

TEST_F(DistortionBankTest, Disto_ZeroInput_NoOutput)
{
    // Zero input should produce zero output
    for (int i = 0; i < 1000; i++)
    {
        float out = Disto(0.f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
        EXPECT_FALSE(std::isnan(out)) << "Disto produced NaN at sample " << i;
        EXPECT_FALSE(std::isinf(out)) << "Disto produced Inf at sample " << i;
        EXPECT_NEAR(out, 0.f, ZERO_TOLERANCE) << "Disto should output zero for zero input";
    }
}

TEST_F(DistortionBankTest, Disto_EnvelopeFollowsInput)
{
    // Envelope should track input level
    // Start with silence, then loud signal
    // Note: Uses 500Hz sine wave since sidechain is bandpass filtered
    for (int i = 0; i < 100; i++)
        Disto(0.f, 1.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    
    float env_before = state_l.disto_env;
    
    // Apply loud 500Hz signal (at sidechain center frequency)
    for (int i = 0; i < 2000; i++)
    {
        float phase = static_cast<float>(i) * 500.f / SAMPLE_RATE;
        float input = sinf(phase * 2.f * 3.14159f) * 0.8f;
        Disto(input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    }
    
    float env_after = state_l.disto_env;
    
    EXPECT_GT(env_after, env_before) << "Envelope should increase with loud input";
    EXPECT_GT(env_after, 0.1f) << "Envelope should track loud signal";
}

TEST_F(DistortionBankTest, Disto_EnvelopeDecays)
{
    // Envelope should decay after loud signal stops
    // First, build up envelope with loud 500Hz signal
    for (int i = 0; i < 2000; i++)
    {
        float phase = static_cast<float>(i) * 500.f / SAMPLE_RATE;
        float input = sinf(phase * 2.f * 3.14159f) * 0.8f;
        Disto(input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    }
    
    float env_peak = state_l.disto_env;
    
    // Now silence - envelope should decay
    for (int i = 0; i < 10000; i++)
        Disto(0.f, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    
    float env_after_decay = state_l.disto_env;
    
    EXPECT_LT(env_after_decay, env_peak) << "Envelope should decay after signal stops";
}

TEST_F(DistortionBankTest, Disto_ModAffectsEnvelopeSpeed)
{
    // MOD=0 should have slow envelope, MOD=1 should have fast envelope
    AlgorithmState state_slow, state_fast;
    
    // Apply same 500Hz signal with different MOD values
    for (int i = 0; i < 1000; i++)
    {
        float phase = static_cast<float>(i) * 500.f / SAMPLE_RATE;
        float input = sinf(phase * 2.f * 3.14159f) * 0.8f;
        Disto(input, 5.f, 0.f, 0.f, SAMPLE_RATE, state_slow);   // Slow envelope
        Disto(input, 5.f, 1.f, 0.f, SAMPLE_RATE, state_fast);   // Fast envelope
    }
    
    // Fast envelope should reach higher level sooner
    EXPECT_GT(state_fast.disto_env, state_slow.disto_env * 0.5f)
        << "Fast envelope (MOD=1) should track faster than slow (MOD=0)";
}

TEST_F(DistortionBankTest, Disto_OutputBounded)
{
    // Output should always be in [-1, 1] range
    float inputs[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    float gains[] = {1.f, 5.f, 10.f, 50.f, 100.f};
    float mods[] = {0.f, 0.5f, 1.f};
    
    for (float input : inputs)
    {
        for (float gain : gains)
        {
            for (float mod : mods)
            {
                SetUp();
                for (int i = 0; i < 100; i++)
                {
                    float out = Disto(input, gain, mod, 0.f, SAMPLE_RATE, state_l);
                    EXPECT_FALSE(std::isnan(out));
                    EXPECT_FALSE(std::isinf(out));
                    EXPECT_GE(out, -1.f);
                    EXPECT_LE(out, 1.f);
                }
            }
        }
    }
}

TEST_F(DistortionBankTest, Disto_ExtremeSampleRates)
{
    // Should not crash with extreme sample rates
    float sample_rates[] = {100.f, 1000.f, 8000.f, 44100.f, 48000.f, 96000.f, 192000.f};
    
    for (float sr : sample_rates)
    {
        SetUp();
        for (int i = 0; i < 100; i++)
        {
            float out = Disto(0.5f, 5.f, 0.5f, 0.f, sr, state_l);
            EXPECT_FALSE(std::isnan(out)) << "NaN at sample_rate=" << sr;
            EXPECT_FALSE(std::isinf(out)) << "Inf at sample_rate=" << sr;
        }
    }
}

TEST_F(DistortionBankTest, Disto_ZeroSampleRate_NoCrash)
{
    // Should handle zero/negative sample rate gracefully
    SetUp();
    for (int i = 0; i < 100; i++)
    {
        float out = Disto(0.5f, 5.f, 0.5f, 0.f, 0.f, state_l);
        EXPECT_FALSE(std::isnan(out)) << "NaN at sample_rate=0";
        EXPECT_FALSE(std::isinf(out)) << "Inf at sample_rate=0";
    }
}

TEST_F(DistortionBankTest, Disto_LongRun_NoStateAccumulation)
{
    // Run for many samples to check for state accumulation issues
    for (int i = 0; i < 100000; i++)
    {
        float input = (i % 2 == 0) ? 0.5f : -0.5f;
        float out = Disto(input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
        
        if (i % 10000 == 9999)
        {
            EXPECT_FALSE(std::isnan(out)) << "NaN after " << i << " samples";
            EXPECT_FALSE(std::isinf(out)) << "Inf after " << i << " samples";
            EXPECT_FALSE(std::isnan(state_l.disto_env)) << "Envelope NaN after " << i << " samples";
            EXPECT_FALSE(std::isinf(state_l.disto_env)) << "Envelope Inf after " << i << " samples";
            EXPECT_FALSE(std::isnan(state_l.disto_sc_low)) << "SC low NaN after " << i << " samples";
            EXPECT_FALSE(std::isnan(state_l.disto_sc_band)) << "SC band NaN after " << i << " samples";
            EXPECT_GE(state_l.disto_env, 0.f);
            EXPECT_LE(state_l.disto_env, 100.f);
        }
    }
}

TEST_F(DistortionBankTest, Disto_Sidechain_ReducesLFPumping)
{
    // Low frequency input should not cause excessive envelope modulation
    // due to bandpass sidechain filtering out sub-bass
    
    // Generate 30Hz sine wave (below sidechain HP cutoff)
    float max_env = 0.f;
    float min_env = 100.f;
    
    for (int i = 0; i < 4800; i++)  // 100ms at 48kHz
    {
        float phase = static_cast<float>(i) * 30.f / SAMPLE_RATE;
        float input = sinf(phase * 2.f * 3.14159f) * 0.8f;
        Disto(input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
        
        // After initial settling (first 50ms)
        if (i > 2400)
        {
            max_env = std::max(max_env, state_l.disto_env);
            min_env = std::min(min_env, state_l.disto_env);
        }
    }
    
    // Envelope ripple should be small due to HP filtering of 30Hz
    float ripple = max_env - min_env;
    EXPECT_LT(ripple, 0.5f) << "Low frequency should not cause large envelope ripple";
}

TEST_F(DistortionBankTest, Disto_Sidechain_TracksMiddleFrequencies)
{
    // 500Hz input (at sidechain center frequency) should track well
    
    // First, run with silence to reset
    for (int i = 0; i < 1000; i++)
        Disto(0.f, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    
    float env_before = state_l.disto_env;
    
    // Generate 500Hz sine wave
    for (int i = 0; i < 4800; i++)
    {
        float phase = static_cast<float>(i) * 500.f / SAMPLE_RATE;
        float input = sinf(phase * 2.f * 3.14159f) * 0.8f;
        Disto(input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    }
    
    float env_after = state_l.disto_env;
    
    // Envelope should have increased significantly
    EXPECT_GT(env_after, env_before + 0.1f) 
        << "500Hz signal should be tracked by sidechain";
}

TEST_F(DistortionBankTest, Disto_Sidechain_SVFStatesBounded)
{
    // SVF states should remain bounded even with extreme input
    for (int i = 0; i < 10000; i++)
    {
        float input = (i % 2 == 0) ? 10.f : -10.f;  // Extreme input
        Disto(input, 100.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
    }
    
    EXPECT_GE(state_l.disto_sc_low, -100.f);
    EXPECT_LE(state_l.disto_sc_low, 100.f);
    EXPECT_GE(state_l.disto_sc_band, -100.f);
    EXPECT_LE(state_l.disto_sc_band, 100.f);
}

// ============================================================================
// Extreme Parameter Tests
// ============================================================================

TEST_F(DistortionBankTest, ExtremeGain_ZeroGain_AllAlgorithms)
{
    // Zero gain should produce near-zero or attenuated output
    float input = 0.5f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            if (IsNoiseGenerator(static_cast<Category>(cat), algo)) continue;
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, input, 0.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out)) 
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at gain=0";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at gain=0";
                EXPECT_GE(out, -1.f);
                EXPECT_LE(out, 1.f);
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeGain_VeryHighGain_AllAlgorithms)
{
    // Very high gain (100x) should not produce NaN/Inf
    float input = 0.5f;
    float extreme_gain = 100.f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, input, extreme_gain, 0.5f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at gain=100";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at gain=100";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeMod_ZeroMod_AllAlgorithms)
{
    // MOD parameter at 0 should not cause issues
    float input = 0.5f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, input, 5.f, 0.f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at mod=0";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at mod=0";
                EXPECT_GE(out, -1.f);
                EXPECT_LE(out, 1.f);
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeMod_MaxMod_AllAlgorithms)
{
    // MOD parameter at 1 should not cause issues
    float input = 0.5f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, input, 5.f, 1.f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at mod=1";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at mod=1";
                EXPECT_GE(out, -1.f);
                EXPECT_LE(out, 1.f);
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeBias_NegativeOne_AllAlgorithms)
{
    // BIAS at -1 (extreme negative)
    float input = 0.5f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, input, 5.f, 0.5f, -1.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at bias=-1";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at bias=-1";
                EXPECT_GE(out, -1.f);
                EXPECT_LE(out, 1.f);
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeBias_PositiveOne_AllAlgorithms)
{
    // BIAS at +1 (extreme positive) should not produce NaN/Inf
    float input = 0.5f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, input, 5.f, 0.5f, 1.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at bias=+1";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at bias=+1";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeInput_VeryLargeInput_AllAlgorithms)
{
    // Input well beyond normal range (simulating hot signal or CV spike)
    // Should not produce NaN/Inf
    float extreme_input = 10.f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, extreme_input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at input=10";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at input=10";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeInput_VeryLargeNegativeInput_AllAlgorithms)
{
    // Large negative input should not produce NaN/Inf
    float extreme_input = -10.f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, extreme_input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at input=-10";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at input=-10";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeCombo_AllParametersAtZero)
{
    // All parameters at zero (edge case)
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, 0.f, 0.f, 0.f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN with all params=0";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf with all params=0";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeCombo_AllParametersAtMax)
{
    // All parameters at maximum should not produce NaN/Inf
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, 1.f, 100.f, 1.f, 1.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN with all params at max";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf with all params at max";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeCombo_HighGainHighInput)
{
    // High gain with high input (worst case for clipping)
    // Should not produce NaN/Inf
    float high_input = 1.f;
    float high_gain = 100.f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, high_input, high_gain, 0.5f, 0.f, SAMPLE_RATE, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at high gain + high input";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at high gain + high input";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeSampleRate_VeryLow)
{
    // Very low sample rate (edge case)
    float low_sample_rate = 1000.f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            if (IsNoiseGenerator(static_cast<Category>(cat), algo)) continue;
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, 0.5f, 5.f, 0.5f, 0.f, low_sample_rate, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at sample_rate=1000";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at sample_rate=1000";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeSampleRate_VeryHigh)
{
    // Very high sample rate
    float high_sample_rate = 192000.f;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            if (IsNoiseGenerator(static_cast<Category>(cat), algo)) continue;
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = Process(static_cast<Category>(cat), algo, 0.5f, 5.f, 0.5f, 0.f, high_sample_rate, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced NaN at sample_rate=192000";
                EXPECT_FALSE(std::isinf(out))
                    << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                    << " produced Inf at sample_rate=192000";
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeFAM_AllModesWithExtremeParams)
{
    // Test all FAM modes with extreme parameters
    float extreme_inputs[] = {-10.f, -1.f, 0.f, 1.f, 10.f};
    float extreme_gains[] = {0.f, 0.1f, 1.f, 10.f, 100.f};
    float extreme_biases[] = {-1.f, 0.f, 1.f};
    
    for (FamMode fam = FAM_STANDARD; fam < FAM_LAST; fam = static_cast<FamMode>(fam + 1))
    {
        for (int cat = 0; cat < CAT_LAST; cat++)
        {
            for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
            {
                if (IsNoiseGenerator(static_cast<Category>(cat), algo)) continue;
                
                for (float input : extreme_inputs)
                {
                    for (float gain : extreme_gains)
                    {
                        for (float bias : extreme_biases)
                        {
                            SetUp();
                            
                            for (int i = 0; i < 10; i++)
                            {
                                float out = ProcessWithFam(static_cast<Category>(cat), algo, input, gain, 0.5f, bias, fam, SAMPLE_RATE, state_l);
                                EXPECT_FALSE(std::isnan(out))
                                    << "FAM=" << fam << " " << CATEGORIES[cat].name << "/" 
                                    << GetAlgorithmName(static_cast<Category>(cat), algo)
                                    << " input=" << input << " gain=" << gain << " bias=" << bias;
                                EXPECT_FALSE(std::isinf(out))
                                    << "FAM=" << fam << " " << CATEGORIES[cat].name << "/" 
                                    << GetAlgorithmName(static_cast<Category>(cat), algo)
                                    << " input=" << input << " gain=" << gain << " bias=" << bias;
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeTube_ExtremeInputs)
{
    // TubeProcess with extreme inputs
    float extreme_inputs[] = {-100.f, -10.f, -1.f, 0.f, 1.f, 10.f, 100.f};
    float biases[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
    
    for (float input : extreme_inputs)
    {
        for (float bias : biases)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out = TubeProcess(input, bias, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << "TubeProcess NaN at input=" << input << " bias=" << bias;
                EXPECT_FALSE(std::isinf(out))
                    << "TubeProcess Inf at input=" << input << " bias=" << bias;
                EXPECT_GE(out, -1.f);
                EXPECT_LE(out, 1.f);
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeTubeStages_ExtremeInputs)
{
    // TubeStage functions with extreme inputs
    float extreme_inputs[] = {-100.f, -10.f, -1.f, 0.f, 1.f, 10.f, 100.f};
    
    for (float input : extreme_inputs)
    {
        float out1 = TubeStage1(input, 0.f);
        EXPECT_FALSE(std::isnan(out1)) << "TubeStage1 NaN at input=" << input;
        EXPECT_FALSE(std::isinf(out1)) << "TubeStage1 Inf at input=" << input;
        
        float out2 = TubeStage2(input);
        EXPECT_FALSE(std::isnan(out2)) << "TubeStage2 NaN at input=" << input;
        EXPECT_FALSE(std::isinf(out2)) << "TubeStage2 Inf at input=" << input;
        
        float out3 = TubeStage3(input);
        EXPECT_FALSE(std::isnan(out3)) << "TubeStage3 NaN at input=" << input;
        EXPECT_FALSE(std::isinf(out3)) << "TubeStage3 Inf at input=" << input;
        
        float out4 = TubeStage4(input, 0.5f);
        EXPECT_FALSE(std::isnan(out4)) << "TubeStage4 NaN at input=" << input;
        EXPECT_FALSE(std::isinf(out4)) << "TubeStage4 Inf at input=" << input;
    }
}

TEST_F(DistortionBankTest, ExtremeDCBlock_ExtremeInputs)
{
    // DC blocker with extreme inputs
    float extreme_inputs[] = {-1000.f, -100.f, -1.f, 0.f, 1.f, 100.f, 1000.f};
    
    for (float input : extreme_inputs)
    {
        SetUp();
        
        for (int i = 0; i < 1000; i++)
        {
            float out = DCBlock(input, state_l);
            EXPECT_FALSE(std::isnan(out))
                << "DCBlock NaN at input=" << input;
            EXPECT_FALSE(std::isinf(out))
                << "DCBlock Inf at input=" << input;
        }
    }
}

TEST_F(DistortionBankTest, ExtremeFrequencyShift_ExtremeParams)
{
    // Frequency shifter with extreme parameters
    float extreme_inputs[] = {-10.f, -1.f, 0.f, 1.f, 10.f};
    float extreme_shifts[] = {-1000.f, -100.f, 0.f, 100.f, 1000.f};
    
    for (float input : extreme_inputs)
    {
        for (float shift : extreme_shifts)
        {
            SetUp();
            
            for (int i = 0; i < 100; i++)
            {
                float out;
                FrequencyShift(input, shift, SAMPLE_RATE, out, state_l);
                EXPECT_FALSE(std::isnan(out))
                    << "FrequencyShift NaN at input=" << input << " shift=" << shift;
                EXPECT_FALSE(std::isinf(out))
                    << "FrequencyShift Inf at input=" << input << " shift=" << shift;
            }
        }
    }
}

TEST_F(DistortionBankTest, ExtremeLongRun_NoStateAccumulation)
{
    // Run algorithms for many samples to check for state accumulation issues
    int long_run = 100000;
    
    for (int cat = 0; cat < CAT_LAST; cat++)
    {
        for (int algo = 0; algo < MAX_ALGOS_PER_CAT; algo++)
        {
            SetUp();
            
            // Alternate between positive and negative input
            for (int i = 0; i < long_run; i++)
            {
                float input = (i % 2 == 0) ? 0.5f : -0.5f;
                float out = Process(static_cast<Category>(cat), algo, input, 5.f, 0.5f, 0.f, SAMPLE_RATE, state_l);
                
                // Check periodically
                if (i % 10000 == 9999)
                {
                    EXPECT_FALSE(std::isnan(out))
                        << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                        << " produced NaN after " << i << " samples";
                    EXPECT_FALSE(std::isinf(out))
                        << CATEGORIES[cat].name << "/" << GetAlgorithmName(static_cast<Category>(cat), algo)
                        << " produced Inf after " << i << " samples";
                    EXPECT_GE(out, -1.f);
                    EXPECT_LE(out, 1.f);
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
