/** Tempest - Distortion Engine for Qu-Bit Aurora
 *
 *  A saturation/distortion effect with 36 algorithms across 6 categories and 3 FAM modes.
 *
 *  Controls (Knob + CV):
 *  - Warp: Drive/input gain (1x to 100x, logarithmic)
 *  - Blur: MOD parameter for algorithm variation
 *  - Atmosphere: Post-distortion tone control (low-pass filter)
 *  - Time: Pre-distortion high-pass filter
 *  - Reflect: BIAS parameter (bipolar: center=0, left=negative, right=positive)
 *  - Mix: Crossover frequency (20Hz-500Hz) - bass below passes dry
 *
 *  All CV inputs are bipolar (-5V to +5V) and add to the corresponding knob value.
 *
 *  Buttons:
 *  - Reverse: Next algorithm in category
 *  - Freeze: Previous algorithm in category
 *  - Shift + Reverse: Next category
 *  - Shift + Freeze: Cycle FAM mode (S/X/O)
 *  - Shift + Mix knob: Adjust output trim (white LEDs)
 *
 *  FAM Modes:
 *  - S (Standard): Raw algorithm, BIAS adds subtle DC offset (±15%)
 *  - X (Xxx): Tube amp simulation, BIAS morphs tube type (starved → triode → power)
 *  - O (Odd): Frequency shifter, BIAS controls shift amount (±100Hz)
 *
 *  LEDs:
 *  - LED 1-6: Current algorithm position (colored by category)
 *  - LED_BOT 1-3: FAM mode indicator (S=green, X=orange, O=purple) + distortion meter
 *  - Category colors: Soft=Orange, Hard=Red, Fold=Purple, Fuzz=Yellow, Digital=Cyan, Experimental=Green
 *
 *  Parameter Recall:
 *  - Settings are automatically saved to flash when changed
 *  - Settings are restored on power-up
 */
#include "aurora.h"
#include "daisysp.h"
#include "distortion_bank.h"
#include "util/PersistentStorage.h"

using namespace daisy;
using namespace aurora;
using namespace daisysp;

Hardware hw;

// ============================================================================
// Persistent Settings
// ============================================================================

struct DistortionSettings
{
    uint8_t category;      // Current category (0-5)
    uint8_t fam_mode;      // FAM mode (0-2)
    uint8_t algorithm;     // Current algorithm (0-5)
    uint8_t cab_sim;       // V30 cab sim enabled (0 or 1)
    float   output_trim;   // Output trim (0.0 - 2.0)
    
    // Required for PersistentStorage to detect changes
    bool operator!=(const DistortionSettings& other) const
    {
        return category != other.category ||
               fam_mode != other.fam_mode ||
               algorithm != other.algorithm ||
               cab_sim != other.cab_sim ||
               output_trim != other.output_trim;
    }
};

// Storage offset - must not overlap with calibration data (4096)
// Using 8192 to be safe
static constexpr uint32_t kSettingsOffset = 8192;

// Persistent storage instance (initialized in main)
PersistentStorage<DistortionSettings>* settings_storage = nullptr;

// Flag to track if settings need saving
bool settings_dirty = false;
uint32_t last_settings_change = 0;
constexpr uint32_t SAVE_DELAY_MS = 2000;  // Wait 2 seconds after last change before saving

// Forward declarations for settings functions
void MarkSettingsDirty();
void CheckAndSaveSettings();
void LoadSettings();

// Filters for tone shaping
Svf pre_filter_l, pre_filter_r;
// Post-filter: 4th-order (two cascaded 2nd-order SVFs) for steeper rolloff
Svf post_filter_l_1, post_filter_l_2;
Svf post_filter_r_1, post_filter_r_2;

// Crossover filters (for dry bass passthrough)
distortion::Crossover xover_l, xover_r;

// Algorithm state (per channel)
distortion::AlgorithmState state_l, state_r;

// Noise gate (per channel)
distortion::NoiseGate gate_l, gate_r;

// Dynamics processing for level normalization
Balance balance_l, balance_r;       // Match output RMS to input RMS
Compressor comp_l, comp_r;          // Gentle peak control

// ============================================================================
// V30 Cabinet Simulation (non-IR)
// ============================================================================
// Emulates a V30-loaded 4x12 cabinet using:
// - Resonant thump: Peaking filter at 80Hz (cabinet air resonance)
// - Phase smear: 0.4ms delay mixed back (speaker interference)
// - High-end rolloff: 4th-order LPF at 5.5kHz (removes digital fizz)

namespace cabsim {

// Biquad filter for resonant peak and low-pass stages
struct Biquad
{
    float b0, b1, b2, a1, a2;
    float z1, z2;
    
    void Reset() { z1 = z2 = 0.f; }
    
    float Process(float in)
    {
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }
    
    // Peaking EQ filter
    void SetPeaking(float freq, float q, float gain_db, float sr)
    {
        float A = pow10f(gain_db / 40.f);
        float w0 = 2.f * 3.14159265f * freq / sr;
        float cosw0 = cosf(w0);
        float sinw0 = sinf(w0);
        float alpha = sinw0 / (2.f * q);
        
        float a0 = 1.f + alpha / A;
        b0 = (1.f + alpha * A) / a0;
        b1 = (-2.f * cosw0) / a0;
        b2 = (1.f - alpha * A) / a0;
        a1 = b1;
        a2 = (1.f - alpha / A) / a0;
    }
};

// Short delay line for phase smear (0.4ms at 48kHz = 19 samples)
constexpr int SMEAR_DELAY_SAMPLES = 19;

struct PhaseSmear
{
    float buffer[SMEAR_DELAY_SAMPLES];
    int write_idx;
    
    void Reset()
    {
        for (int i = 0; i < SMEAR_DELAY_SAMPLES; i++) buffer[i] = 0.f;
        write_idx = 0;
    }
    
    float Process(float in)
    {
        float delayed = buffer[write_idx];
        buffer[write_idx] = in;
        write_idx = (write_idx + 1) % SMEAR_DELAY_SAMPLES;
        return delayed;
    }
};

struct V30CabSim
{
    Biquad resonance;           // 80Hz peaking filter
    PhaseSmear smear;
    
    void Init(float sr)
    {
        resonance.Reset();
        smear.Reset();
        
        // Resonant thump at 80Hz, Q=2.5, +6dB boost
        resonance.SetPeaking(80.f, 2.5f, 6.f, sr);
    }
    
    float Process(float in)
    {
        // Resonant thump
        float x = resonance.Process(in);
        
        // Phase smear: mix delayed signal at -6dB
        float delayed = smear.Process(x);
        x = x + delayed * 0.5f;
        
        return x;
    }
};

}  // namespace cabsim

// Cab sim instances
cabsim::V30CabSim cab_sim_l, cab_sim_r;
bool cab_sim_enabled = false;

// Cab sim toggle state
uint32_t freeze_hold_start = 0;
bool freeze_was_held = false;
bool cab_sim_toggle_armed = true;  // Prevents re-toggle until button released
constexpr uint32_t CAB_SIM_HOLD_MS = 5000;  // 5 seconds to toggle

// LED blink state for cab sim toggle feedback
uint32_t cab_blink_start = 0;
int cab_blink_count = 0;
constexpr uint32_t CAB_BLINK_PERIOD_MS = 150;  // On/off cycle

// GATE_FREEZE: Algorithm hold (sample freeze)
float frozen_sample_l = 0.f;
float frozen_sample_r = 0.f;
bool input_frozen = false;

// GATE_REVERSE: Octave up (DC blocking state)
float octave_dc_l = 0.f;
float octave_dc_r = 0.f;

// DC blocking filter (applied before cab sim)
DcBlock dc_block_l, dc_block_r;

// UI state
bool shift_held = false;
bool adjusting_output_trim = false;
distortion::Category current_cat = distortion::CAT_ANALOG;
distortion::FamMode current_fam = distortion::FAM_STANDARD;
int current_algo = 0;
float output_trim = 1.2f;  // Default 60% (range 0-2)
float sample_rate_global = 48000.f;

// For detecting knob movement
float mix_knob_snapshot = 0.f;
bool mix_knob_captured = false;
bool mix_knob_needs_catchup = false;  // After shift+mix, knob must pass through snapshot to update crossover
float last_crossover_knob_value = 0.f;  // Last valid crossover knob position
constexpr float KNOB_MOVE_THRESHOLD = 0.02f;

// Signal metering for LED display
float distortion_amount = 0.f;
constexpr float ENV_ATTACK = 0.1f;
constexpr float ENV_RELEASE = 0.002f;

// CV smoothing (prevents clicks from S&H or stepped CV sources)
constexpr float CV_SLEW_RATE = 0.005f;  // Slew rate per sample (~8ms full range at 48kHz)
float smoothed_cv[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};  // One per CV input

// Slew limiter helper - smooths CV toward target
inline float SlewCV(float target, float& current)
{
    float delta = target - current;
    if (fabsf(delta) > CV_SLEW_RATE)
        current += (delta > 0.f) ? CV_SLEW_RATE : -CV_SLEW_RATE;
    else
        current = target;
    return current;
}

// Reflect knob dead center feedback
bool bias_centered = false;
uint32_t bias_center_flash_start = 0;
constexpr uint32_t BIAS_CENTER_FLASH_DURATION_MS = 500;
constexpr float BIAS_DEAD_CENTER_THRESHOLD = 0.01f;  // +/- 1%

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    
    shift_held = hw.GetButton(SW_SHIFT).Pressed();
    
    // Cab sim toggle: Hold FREEZE for 5 seconds (without shift)
    bool freeze_pressed = hw.GetButton(SW_FREEZE).Pressed();
    if (!shift_held && freeze_pressed)
    {
        if (!freeze_was_held)
        {
            // Just started holding
            freeze_hold_start = System::GetNow();
            freeze_was_held = true;
        }
        else if (cab_sim_toggle_armed)
        {
            // Check if held for 5 seconds (only if armed)
            uint32_t held_time = System::GetNow() - freeze_hold_start;
            if (held_time >= CAB_SIM_HOLD_MS)
            {
                // Toggle cab sim
                cab_sim_enabled = !cab_sim_enabled;
                MarkSettingsDirty();
                
                // Start LED blink feedback
                cab_blink_start = System::GetNow();
                cab_blink_count = 6;  // 3 blinks = 6 transitions (on/off/on/off/on/off)
                
                // Disarm to prevent re-toggle until button released
                cab_sim_toggle_armed = false;
            }
        }
    }
    else
    {
        freeze_was_held = false;
        freeze_hold_start = 0;
        cab_sim_toggle_armed = true;  // Re-arm when button released
    }
    
    // Track if cab sim was just toggled (to suppress algorithm change on release)
    bool cab_sim_just_toggled = (cab_blink_count > 0);
    
    // Button handling - use FallingEdge to avoid conflict with long press
    if (shift_held)
    {
        // Shift + Freeze: Cycle FAM mode (S -> X -> O -> S)
        if (hw.GetButton(SW_FREEZE).FallingEdge() && !cab_sim_just_toggled)
        {
            current_fam = static_cast<distortion::FamMode>((current_fam + 1) % distortion::FAM_LAST);
            MarkSettingsDirty();
        }
        
        // Shift + Reverse: Next category
        if (hw.GetButton(SW_REVERSE).FallingEdge())
        {
            current_cat = static_cast<distortion::Category>((current_cat + 1) % distortion::CAT_LAST);
            current_algo = 0;  // Reset to first algorithm in new category
            MarkSettingsDirty();
        }
    }
    else
    {
        // Reverse: Next algorithm
        if (hw.GetButton(SW_REVERSE).FallingEdge())
        {
            int algo_count = distortion::GetAlgorithmCount(current_cat);
            current_algo = (current_algo + 1) % algo_count;
            MarkSettingsDirty();
        }
        
        // Freeze: Previous algorithm (only if not a long press for cab sim)
        if (hw.GetButton(SW_FREEZE).FallingEdge() && !cab_sim_just_toggled)
        {
            int algo_count = distortion::GetAlgorithmCount(current_cat);
            current_algo = (current_algo - 1 + algo_count) % algo_count;
            MarkSettingsDirty();
        }
    }
    
    // Read controls - Warp is logarithmic (1x to 100x)
    float warp_knob = hw.GetKnobValue(KNOB_WARP);
    float warp_cv = SlewCV(hw.GetCvValue(CV_WARP), smoothed_cv[CV_WARP]);
    float warp_combined = fclamp(warp_knob + warp_cv, 0.f, 1.f);
    float drive = pow10f(warp_combined * 2.f);  // 1 to 100
    
    // Blur knob with square root curve and extended range
    float blur_knob = hw.GetKnobValue(KNOB_BLUR);
    float blur_cv = SlewCV(hw.GetCvValue(CV_BLUR), smoothed_cv[CV_BLUR]);
    float blur_combined = fclamp(blur_knob + blur_cv, 0.f, 1.f);
    float mod_param = sqrtf(blur_combined) * 1.5f;  // 0 to 1.5 with curve
    
    // Reflect is bipolar: center = 0, left = -1, right = +1
    float reflect_knob = hw.GetKnobValue(KNOB_REFLECT);
    float reflect_cv = SlewCV(hw.GetCvValue(CV_REFLECT), smoothed_cv[CV_REFLECT]);
    
    float bias_raw = (reflect_knob - 0.5f) * 2.f;  // -1 to +1
    
    // Dead center detection (+/- 1%) - knob only
    bool was_centered = bias_centered;
    bias_centered = fabsf(bias_raw) < BIAS_DEAD_CENTER_THRESHOLD;
    
    // Trigger flash when entering dead center
    if (bias_centered && !was_centered)
    {
        bias_center_flash_start = System::GetNow();
    }
    
    // Apply dead center: snap to zero
    if (bias_centered)
    {
        bias_raw = 0.f;
    }
    
    // Add smoothed CV after dead center detection, clamp to valid range
    bias_raw = fclamp(bias_raw + reflect_cv, -1.f, 1.f);
    
    // Scale bias based on FAM mode
    float bias_param;
    if (current_fam == distortion::FAM_STANDARD)
    {
        bias_param = bias_raw * 0.15f;  // S mode: 15% range (subtle DC offset)
    }
    else
    {
        bias_param = bias_raw;  // X and O modes: full range
    }
    
    float mix_knob = hw.GetKnobValue(KNOB_MIX);
    float mix_cv = SlewCV(hw.GetCvValue(CV_MIX), smoothed_cv[CV_MIX]);
    
    // Shift + Mix controls output trim (only when knob is moved, no CV)
    float xover_freq;
    if (shift_held)
    {
        if (!mix_knob_captured)
        {
            mix_knob_snapshot = mix_knob;
            mix_knob_captured = true;
        }
        
        float knob_delta = fabsf(mix_knob - mix_knob_snapshot);
        if (knob_delta > KNOB_MOVE_THRESHOLD)
        {
            output_trim = mix_knob * 2.f;  // 0 to 2x
            adjusting_output_trim = true;
            mix_knob_needs_catchup = true;  // Knob moved, will need catch-up on release
            MarkSettingsDirty();
        }
        
        // Use last crossover setting while adjusting trim
        xover_freq = fmap(last_crossover_knob_value, 20.f, 500.f, Mapping::LOG);
    }
    else
    {
        // Check if knob needs to catch up to previous crossover position
        if (mix_knob_needs_catchup)
        {
            // Knob must pass through the snapshot value (pre-shift position) to re-enable crossover control
            float distance_to_snapshot = fabsf(mix_knob - mix_knob_snapshot);
            if (distance_to_snapshot < KNOB_MOVE_THRESHOLD)
            {
                // Knob has caught up - re-enable crossover control
                mix_knob_needs_catchup = false;
                last_crossover_knob_value = mix_knob;
            }
            // While catching up, keep using the last valid crossover value
            xover_freq = fmap(last_crossover_knob_value, 20.f, 500.f, Mapping::LOG);
        }
        else
        {
            // Normal operation: Mix knob + CV controls crossover frequency (20Hz to 500Hz)
            float mix_combined = fclamp(mix_knob + mix_cv, 0.f, 1.f);
            xover_freq = fmap(mix_combined, 20.f, 500.f, Mapping::LOG);
            last_crossover_knob_value = mix_knob;  // Track for potential shift+mix
        }
        
        mix_knob_captured = false;
        adjusting_output_trim = false;
    }
    
    xover_l.SetFrequency(xover_freq);
    xover_r.SetFrequency(xover_freq);
    
    // Pre-filter (high-pass): 20Hz to 2kHz
    float time_knob = hw.GetKnobValue(KNOB_TIME);
    float time_cv = SlewCV(hw.GetCvValue(CV_TIME), smoothed_cv[CV_TIME]);
    float time_combined = fclamp(time_knob + time_cv, 0.f, 1.f);
    float pre_freq = fmap(time_combined, 20.f, 2000.f, Mapping::LOG);
    pre_filter_l.SetFreq(pre_freq);
    pre_filter_r.SetFreq(pre_freq);
    
    // Post-filter (low-pass): 20Hz to 20kHz
    float atmo_knob = hw.GetKnobValue(KNOB_ATMOSPHERE);
    float atmo_cv = SlewCV(hw.GetCvValue(CV_ATMOSPHERE), smoothed_cv[CV_ATMOSPHERE]);
    float atmo_combined = fclamp(atmo_knob + atmo_cv, 0.f, 1.f);
    float post_freq = fmap(atmo_combined, 20.f, 20000.f, Mapping::LOG);
    post_filter_l_1.SetFreq(post_freq);
    post_filter_l_2.SetFreq(post_freq);
    post_filter_r_1.SetFreq(post_freq);
    post_filter_r_2.SetFreq(post_freq);
    
    // Noise gate threshold scales with drive (more drive = higher threshold)
    float gate_thresh = 0.005f + (warp_combined * 0.05f);
    gate_l.SetThreshold(gate_thresh);
    gate_r.SetThreshold(gate_thresh);
    
    // GATE_FREEZE: Algorithm hold - check gate state once per block
    bool freeze_gate = hw.GetGateState(GATE_FREEZE);
    if (!freeze_gate)
    {
        input_frozen = false;
    }
    
    // GATE_REVERSE: Octave up
    bool reverse_gate = hw.GetGateState(GATE_REVERSE);
    
    for (size_t i = 0; i < size; i++)
    {
        float dry_l = in[0][i];
        float dry_r = in[1][i];
        
        // Crossover: split into low (dry passthrough) and high (to distortion)
        float low_l, high_l, low_r, high_r;
        xover_l.Process(dry_l, low_l, high_l);
        xover_r.Process(dry_r, low_r, high_r);
        
        // Pre-filter on high frequency content
        pre_filter_l.Process(high_l);
        pre_filter_r.Process(high_r);
        float filtered_l = pre_filter_l.High();
        float filtered_r = pre_filter_r.High();
        
        // GATE_FREEZE: Algorithm hold - freeze input sample
        float to_distort_l = filtered_l;
        float to_distort_r = filtered_r;
        if (freeze_gate)
        {
            if (!input_frozen)
            {
                // Capture sample on rising edge
                frozen_sample_l = filtered_l;
                frozen_sample_r = filtered_r;
                input_frozen = true;
            }
            to_distort_l = frozen_sample_l;
            to_distort_r = frozen_sample_r;
        }
        
        // Process through selected algorithm with FAM mode
        float wet_l = distortion::ProcessWithFam(current_cat, current_algo, to_distort_l, drive, mod_param, bias_param, current_fam, sample_rate_global, state_l);
        float wet_r = distortion::ProcessWithFam(current_cat, current_algo, to_distort_r, drive, mod_param, bias_param, current_fam, sample_rate_global, state_r);
        
        // GATE_REVERSE: Octave up via full-wave rectification
        if (reverse_gate)
        {
            wet_l = fabsf(wet_l);
            wet_r = fabsf(wet_r);
            
            // DC blocking to remove offset from rectification
            octave_dc_l = 0.995f * octave_dc_l + 0.005f * wet_l;
            octave_dc_r = 0.995f * octave_dc_r + 0.005f * wet_r;
            wet_l -= octave_dc_l;
            wet_r -= octave_dc_r;
        }
        
        // Balance: match distorted output RMS to input RMS
        // Use filtered input as reference (pre-distortion signal)
        wet_l = balance_l.Process(wet_l, to_distort_l);
        wet_r = balance_r.Process(wet_r, to_distort_r);
        
        // Compressor: gentle peak control
        wet_l = comp_l.Process(wet_l);
        wet_r = comp_r.Process(wet_r);
        
        // DC blocking - centers signal around 0V before cab sim
        wet_l = dc_block_l.Process(wet_l);
        wet_r = dc_block_r.Process(wet_r);
        
        // V30 cab sim (before post-filter)
        if (cab_sim_enabled)
        {
            wet_l = cab_sim_l.Process(wet_l);
            wet_r = cab_sim_r.Process(wet_r);
        }
        
        // Post-filter (4th-order: two cascaded 2nd-order SVFs)
        post_filter_l_1.Process(wet_l);
        post_filter_l_2.Process(post_filter_l_1.Low());
        wet_l = post_filter_l_2.Low();
        
        post_filter_r_1.Process(wet_r);
        post_filter_r_2.Process(post_filter_r_1.Low());
        wet_r = post_filter_r_2.Low();
        
        // Combine: low band (dry) + processed highs
        float combined_l = low_l + wet_l;
        float combined_r = low_r + wet_r;
        
        // Soft limiter: safety clipping before output trim
        combined_l = SoftClip(combined_l);
        combined_r = SoftClip(combined_r);
        
        // Output trim (Shift+Mix) - final volume control after all processing
        combined_l *= output_trim;
        combined_r *= output_trim;
        
        // Noise gate at end of chain
        out[0][i] = gate_l.Process(combined_l);
        out[1][i] = gate_r.Process(combined_r);
        
        // Measure distortion amount for LED display
        float input_level = fabsf(filtered_l) + fabsf(filtered_r);
        float output_level_sig = fabsf(wet_l) + fabsf(wet_r);
        
        float current_dist = 0.f;
        if (input_level > 0.01f)
        {
            float driven_level = input_level * drive;
            current_dist = 1.f - (output_level_sig / daisysp::fmax(driven_level, 0.01f));
            current_dist = daisysp::fmax(0.f, daisysp::fmin(current_dist, 1.f));
        }
        
        if (current_dist > distortion_amount)
            distortion_amount += ENV_ATTACK * (current_dist - distortion_amount);
        else
            distortion_amount += ENV_RELEASE * (current_dist - distortion_amount);
    }
}

void UpdateLeds()
{
    hw.ClearLeds();
    
    // Cab sim toggle blink feedback (3 blinks on LED_FREEZE and LED_REVERSE)
    if (cab_blink_count > 0)
    {
        uint32_t elapsed = System::GetNow() - cab_blink_start;
        int blink_phase = elapsed / CAB_BLINK_PERIOD_MS;
        
        if (blink_phase >= cab_blink_count)
        {
            // Blink sequence complete
            cab_blink_count = 0;
        }
        else
        {
            // Blink on during even phases (0, 2, 4)
            bool led_on = (blink_phase % 2) == 0;
            if (led_on)
            {
                // White blink
                hw.SetLed(LED_FREEZE, 1.f, 1.f, 1.f);
                hw.SetLed(LED_REVERSE, 1.f, 1.f, 1.f);
            }
            hw.WriteLeds();
            return;  // Skip normal LED update during blink
        }
    }
    
    // Get category color
    float r = 0.f, g = 0.f, b = 0.f;
    distortion::GetCategoryColor(current_cat, r, g, b);
    
    // When adjusting output trim, show progressive white LEDs
    if (adjusting_output_trim)
    {
        Leds main_leds[] = {LED_1, LED_2, LED_3, LED_4, LED_5, LED_6};
        constexpr int num_leds = 6;
        
        // Normalize output_trim (0-2) to 0-1 range
        float normalized = output_trim / 2.f;
        
        // Calculate which LED is the "current" one and its intensity
        // Each LED represents 1/6 of the range
        float position = normalized * num_leds;  // 0 to 6
        
        for (int i = 0; i < num_leds; i++)
        {
            float led_start = static_cast<float>(i);
            float led_end = static_cast<float>(i + 1);
            
            float intensity;
            if (position >= led_end)
            {
                // Fully lit
                intensity = 1.f;
            }
            else if (position > led_start)
            {
                // Partially lit (current LED)
                intensity = position - led_start;
            }
            else
            {
                // Off
                intensity = 0.f;
            }
            
            hw.SetLed(main_leds[i], intensity, intensity, intensity);
        }

        hw.WriteLeds();
        return;
    }
    
    // LED_FREEZE shows FAM mode: Green=S, Orange=X, Purple=O
    float freeze_r = 0.f, freeze_g = 0.f, freeze_b = 0.f;
    distortion::GetFamColor(current_fam, freeze_r, freeze_g, freeze_b);
    
    // Limit Freeze and Reverse LEDs to 80% intensity
    constexpr float LED_MAX_INTENSITY = 0.8f;
    
    hw.SetLed(LED_FREEZE, freeze_r * LED_MAX_INTENSITY, freeze_g * LED_MAX_INTENSITY, freeze_b * LED_MAX_INTENSITY);
    
    // Reverse LED shows category color
    hw.SetLed(LED_REVERSE, r * LED_MAX_INTENSITY, g * LED_MAX_INTENSITY, b * LED_MAX_INTENSITY);
    
    // LEDs 1-6 show current algorithm position with category color
    Leds main_leds[] = {LED_1, LED_2, LED_3, LED_4, LED_5, LED_6};
    int algo_count = distortion::GetAlgorithmCount(current_cat);
    
    // Check if we're in the bias center flash period
    bool flash_white = false;
    if (bias_center_flash_start > 0)
    {
        uint32_t elapsed = System::GetNow() - bias_center_flash_start;
        if (elapsed < BIAS_CENTER_FLASH_DURATION_MS)
        {
            flash_white = true;
        }
        else
        {
            bias_center_flash_start = 0;  // Flash period ended
        }
    }
    
    // Build array of inactive LED indices for distortion meter
    int inactive_leds[5];
    int inactive_count = 0;
    for (int i = 0; i < 6; i++)
    {
        if (i != current_algo && i < algo_count)
        {
            inactive_leds[inactive_count++] = i;
        }
    }
    
    // Map distortion_amount (0-1) to meter position (0 to inactive_count)
    float meter_pos = distortion_amount * static_cast<float>(inactive_count);
    
    for (int i = 0; i < 6; i++)
    {
        if (i < algo_count)
        {
            if (i == current_algo)
            {
                if (flash_white)
                {
                    // Flash white when bias is centered
                    hw.SetLed(main_leds[i], 1.f, 1.f, 1.f);
                }
                else
                {
                    // Current algorithm: full brightness category color
                    hw.SetLed(main_leds[i], r, g, b);
                }
            }
            else
            {
                // Find this LED's position in the inactive array
                int meter_idx = -1;
                for (int j = 0; j < inactive_count; j++)
                {
                    if (inactive_leds[j] == i)
                    {
                        meter_idx = j;
                        break;
                    }
                }
                
                if (meter_idx >= 0)
                {
                    // Calculate intensity for this meter LED
                    float led_start = static_cast<float>(meter_idx);
                    float led_end = static_cast<float>(meter_idx + 1);
                    
                    float intensity;
                    if (meter_pos >= led_end)
                        intensity = 1.f;
                    else if (meter_pos > led_start)
                        intensity = meter_pos - led_start;
                    else
                        intensity = 0.f;
                    
                    // VU gradient color based on meter position (0-4 mapped to gradient)
                    // Green -> Orange -> Red -> White
                    // Green and orange are dimmed to balance perceived brightness
                    float gradient_pos = static_cast<float>(meter_idx) / static_cast<float>(inactive_count > 1 ? inactive_count - 1 : 1);
                    
                    float mr, mg, mb;
                    if (gradient_pos < 0.33f)
                    {
                        // Green to Orange (green dimmed to 50%)
                        float t = gradient_pos / 0.33f;
                        mr = t * 0.7f;           // 0 -> 0.7
                        mg = 0.5f - t * 0.2f;    // 0.5 -> 0.3
                        mb = 0.f;
                    }
                    else if (gradient_pos < 0.66f)
                    {
                        // Orange to Red (orange dimmed)
                        float t = (gradient_pos - 0.33f) / 0.33f;
                        mr = 0.7f + t * 0.3f;    // 0.7 -> 1.0
                        mg = 0.3f - t * 0.3f;    // 0.3 -> 0
                        mb = 0.f;
                    }
                    else
                    {
                        // Red to White
                        float t = (gradient_pos - 0.66f) / 0.34f;
                        mr = 1.f;
                        mg = t;  // 0 -> 1
                        mb = t;  // 0 -> 1
                    }
                    
                    hw.SetLed(main_leds[i], mr * intensity, mg * intensity, mb * intensity);
                }
                else
                {
                    // LED not in meter (shouldn't happen), turn off
                    hw.SetLed(main_leds[i], 0.f, 0.f, 0.f);
                }
            }
        }
    }
    
    // Bottom LEDs show FAM mode color with distortion-modulated intensity
    float fam_r = 0.f, fam_g = 0.f, fam_b = 0.f;
    distortion::GetFamColor(current_fam, fam_r, fam_g, fam_b);
    
    // Distortion meter modulates intensity (base 20% + up to 80% from distortion)
    Leds bot_leds[] = {LED_BOT_1, LED_BOT_2, LED_BOT_3};
    float dist_scaled = distortion_amount * 3.f;
    
    for (int i = 0; i < 3; i++)
    {
        float led_level = daisysp::fmin(daisysp::fmax(dist_scaled - i, 0.f), 1.f);
        float intensity = 0.2f + led_level * 0.8f;
        hw.SetLed(bot_leds[i], fam_r * intensity, fam_g * intensity, fam_b * intensity);
    }
    
    hw.WriteLeds();
}

// Mark settings as changed (will be saved after delay)
void MarkSettingsDirty()
{
    settings_dirty = true;
    last_settings_change = System::GetNow();
}

// Save settings to flash if dirty and enough time has passed
void CheckAndSaveSettings()
{
    if (settings_dirty && settings_storage != nullptr)
    {
        uint32_t elapsed = System::GetNow() - last_settings_change;
        if (elapsed >= SAVE_DELAY_MS)
        {
            auto& settings = settings_storage->GetSettings();
            settings.category = static_cast<uint8_t>(current_cat);
            settings.fam_mode = static_cast<uint8_t>(current_fam);
            settings.algorithm = static_cast<uint8_t>(current_algo);
            settings.cab_sim = cab_sim_enabled ? 1 : 0;
            settings.output_trim = output_trim;
            settings_storage->Save();
            settings_dirty = false;
        }
    }
}

// Load settings from flash
void LoadSettings()
{
    if (settings_storage != nullptr)
    {
        auto& settings = settings_storage->GetSettings();
        
        // Validate and apply settings
        if (settings.category < distortion::CAT_LAST)
            current_cat = static_cast<distortion::Category>(settings.category);
        
        if (settings.fam_mode < distortion::FAM_LAST)
            current_fam = static_cast<distortion::FamMode>(settings.fam_mode);
        
        int algo_count = distortion::GetAlgorithmCount(current_cat);
        if (settings.algorithm < algo_count)
            current_algo = settings.algorithm;
        else
            current_algo = 0;
        
        if (settings.output_trim >= 0.f && settings.output_trim <= 2.f)
            output_trim = settings.output_trim;
        else
            output_trim = 1.2f;
        
        cab_sim_enabled = (settings.cab_sim != 0);
    }
}

int main(void)
{
    hw.Init();
    
    // Seed random number generator for BrwnNz and Clicks algorithms
    srand(System::GetNow());
    
    float sr = hw.AudioSampleRate();
    sample_rate_global = sr;
    
    // Initialize persistent storage
    static PersistentStorage<DistortionSettings> storage(hw.seed.qspi);
    settings_storage = &storage;
    
    DistortionSettings defaults;
    defaults.category = 0;
    defaults.fam_mode = 0;
    defaults.algorithm = 0;
    defaults.cab_sim = 0;
    defaults.output_trim = 1.2f;  // Default 60% (range 0-2)
    
    storage.Init(defaults, kSettingsOffset);
    LoadSettings();
    
    // Initialize pre-filters (high-pass)
    pre_filter_l.Init(sr);
    pre_filter_r.Init(sr);
    pre_filter_l.SetRes(0.5f);
    pre_filter_r.SetRes(0.5f);
    
    // Initialize post-filters (4th-order low-pass: two cascaded SVFs)
    post_filter_l_1.Init(sr);
    post_filter_l_2.Init(sr);
    post_filter_r_1.Init(sr);
    post_filter_r_2.Init(sr);
    // Use Q=0.707 (Butterworth) for flat response, no resonant bump
    post_filter_l_1.SetRes(0.3f);
    post_filter_l_2.SetRes(0.3f);
    post_filter_r_1.SetRes(0.3f);
    post_filter_r_2.SetRes(0.3f);
    
    // Initialize crossover filters
    xover_l.Init(sr);
    xover_r.Init(sr);
    
    // Initialize noise gates
    gate_l.Init(sr);
    gate_r.Init(sr);
    
    // Initialize cab sim
    cab_sim_l.Init(sr);
    cab_sim_r.Init(sr);
    
    // Initialize DC blocking filters
    dc_block_l.Init(sr);
    dc_block_r.Init(sr);
    
    // Initialize dynamics processing
    balance_l.Init(sr);
    balance_r.Init(sr);
    
    comp_l.Init(sr);
    comp_r.Init(sr);
    comp_l.SetThreshold(-3.f);    // Only compress hot signals
    comp_r.SetThreshold(-3.f);
    comp_l.SetRatio(8.f);         // Strong ratio but high threshold
    comp_r.SetRatio(8.f);
    comp_l.SetAttack(0.005f);     // 5ms - catch transients
    comp_r.SetAttack(0.005f);
    comp_l.SetRelease(0.05f);     // 50ms - quick recovery
    comp_r.SetRelease(0.05f);
    comp_l.AutoMakeup(false);     // No makeup gain
    comp_r.AutoMakeup(false);
    
    hw.StartAudio(AudioCallback);
    
    while (1)
    {
        UpdateLeds();
        CheckAndSaveSettings();
        System::Delay(10);
    }
}
