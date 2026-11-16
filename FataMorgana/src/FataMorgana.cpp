/** FataMorgana - 3D Wavetable Synthesizer for Qu-Bit Aurora
 *
 *  A dual-oscillator wavetable synthesizer inspired by the Industrial Music Electronics
 *  Piston Honda MKIII. Features 3D morphing across an 8x8x8 wave cube with thru-zero FM.
 *  Oscillators A and B can have independent wavetable positions and frequency offset.
 *
 *  Controls (Knob + CV):
 *  - Warp: Base frequency (V/Oct, calibrated 1V/octave tracking)
 *  - Blur: FM depth / External input gain
 *  - Time: X-axis morph position (0-7) for selected oscillator
 *  - Reflect: Y-axis morph position (0-7) for selected oscillator
 *  - Atmosphere: Z-axis morph position (0-7) for selected oscillator
 *  - Mix: Unison detune spread
 *
 *  Shift+Knob:
 *  - Shift + Warp: Osc B semitone offset (±24 semitones)
 *  - Shift + Time: Sub A level
 *  - Shift + Reflect: Sub B level
 *
 *  Buttons:
 *  - Freeze: Toggle morph interpolation (smooth vs glitchy)
 *  - Reverse (tap): Cycle oscillator selection (A → B → Both)
 *  - Reverse (hold 5s): Sync Osc B wavetable position to Osc A
 *  - Shift + Freeze: Toggle external input mode (waveshaper)
 *  - Shift + Reverse: Cycle tone mode (Orthodox/Degraded)
 *
 *  LEDs:
 *  - Reverse LED: Oscillator selection (Orange=A, Cyan=B, Purple=Both)
 *  - LED 1-2: X position, LED 3-4: Y position, LED 5-6: Z position
 *
 *  Audio I/O:
 *  - Left Out: Oscillator A
 *  - Right Out: Oscillator B
 *  - Left In: FM input for Osc A (normalled to Osc B output)
 *  - Right In: FM input for Osc B (normalled to Osc A output)
 *
 *  USB Drive:
 *  - Load 8 WAV files (1.wav - 8.wav) to populate the wave cube
 *  - Each file: 16-bit mono, 256 samples/wave, 64 waves (8x8 grid)
 */
#include "daisy_seed.h"
#include "fatfs.h"

namespace aurora
{
// The SDK defines this object in aurora.h. Place its sector cache in D2 SRAM
// before that definition so PrepareMedia and f_mount share a DMA-safe object.
extern daisy::FatFSInterface DMA_BUFFER_MEM_SECTION fatfs_interface;
}

#include "aurora.h"
#include "daisysp.h"
#include "Effects/bitcrush.h"
#include "Effects/fold.h"
#include "Effects/overdrive.h"
#include "diskio.h"
#include "wavetable/wave_cube.h"
#include "wavetable/oscillator.h"
#include "util/PersistentStorage.h"

using namespace daisy;
using namespace aurora;
using namespace daisysp;

Hardware hw;

// ============================================================================
// Wave Cube Instance (in SDRAM) and DMA Buffer
// ============================================================================
wavetable::WaveCube wavetable::wave_cube DSY_SDRAM_BSS;
alignas(32) uint8_t DMA_BUFFER_MEM_SECTION wavetable::g_sector_buf[wavetable::SECTOR_SIZE];

// ============================================================================
// Oscillators
// ============================================================================
wavetable::Oscillator osc_a;
wavetable::Oscillator osc_b;

// Sub-oscillators (wavetable at -1 octave) for low-end weight
wavetable::Oscillator osc_a_sub;
wavetable::Oscillator osc_b_sub;

// ============================================================================
// Oscillator Selection
// ============================================================================
enum class OscillatorSelect : uint8_t
{
    OSC_A = 0,
    OSC_B = 1,
    OSC_BOTH = 2
};

// ============================================================================
// Persistent Settings
// ============================================================================
struct PistonSettings
{
    uint8_t morph_mode;      // 0 = smooth, 1 = glitchy (no interpolation)
    uint8_t unison_enabled;  // 0 = off, 1 = on
    uint8_t external_mode;   // 0 = oscillator, 1 = waveshaper
    uint8_t tone_mode;       // 0 = orthodox (clean), 1 = degraded
    uint8_t sub_a_level;     // Sub A level (0-255 maps to 0.0-1.0)
    uint8_t sub_b_level;     // Sub B level (0-255 maps to 0.0-1.0)
    
    // Oscillator selection and independent positions
    uint8_t osc_select;           // 0=A, 1=B, 2=Both
    int8_t  osc_b_semitone_offset; // -24 to +24 semitones
    uint8_t x_pos_a;              // 0-255 maps to 0.0-7.0
    uint8_t y_pos_a;
    uint8_t z_pos_a;
    uint8_t x_pos_b;
    uint8_t y_pos_b;
    uint8_t z_pos_b;
    
    // Effect system
    uint8_t effect_mode;          // 0-4 (EffectMode enum)
    uint8_t output_trim;          // 0-255 maps to 0.0-1.0
    
    bool operator!=(const PistonSettings& other) const
    {
        return morph_mode != other.morph_mode ||
               unison_enabled != other.unison_enabled ||
               external_mode != other.external_mode ||
               tone_mode != other.tone_mode ||
               sub_a_level != other.sub_a_level ||
               sub_b_level != other.sub_b_level ||
               osc_select != other.osc_select ||
               osc_b_semitone_offset != other.osc_b_semitone_offset ||
               x_pos_a != other.x_pos_a ||
               y_pos_a != other.y_pos_a ||
               z_pos_a != other.z_pos_a ||
               x_pos_b != other.x_pos_b ||
               y_pos_b != other.y_pos_b ||
               z_pos_b != other.z_pos_b ||
               effect_mode != other.effect_mode ||
               output_trim != other.output_trim;
    }
};

// ============================================================================
// Constants
// ============================================================================
namespace constants {
    constexpr uint32_t SETTINGS_OFFSET = 8192;
    constexpr uint32_t SAVE_DELAY_MS = 2000;
    constexpr float KNOB_THRESHOLD = 0.02f;
    constexpr float CV_SCALE_XYZ = 3.5f;
    constexpr float CV_SCALE_BLUR = 0.5f;
    constexpr float CV_SCALE_MIX = 0.025f;
    constexpr float MAX_DETUNE = 0.1f;
    constexpr float SUB_LEVEL_THRESHOLD = 0.001f;
    constexpr float INT16_NORMALIZE = 32768.f;
}

PersistentStorage<PistonSettings>* settings_storage = nullptr;
bool settings_dirty = false;
uint32_t last_settings_change = 0;

// Current state
bool morph_enabled = true;
bool unison_enabled = false;
bool external_mode = false;
int tone_mode = 0;  // 0 = orthodox, 1 = degraded
float sub_a_level = 0.f;
float sub_b_level = 0.f;

// Oscillator selection and independent wavetable positions
OscillatorSelect current_osc_select = OscillatorSelect::OSC_BOTH;
int osc_b_semitone_offset = 0;  // -24 to +24 semitones
float osc_b_freq_ratio = 1.f;   // Cached frequency ratio for Osc B offset

// Separate X/Y/Z positions for each oscillator (knob-defined values)
float x_pos_a = 0.f, y_pos_a = 0.f, z_pos_a = 0.f;
float x_pos_b = 0.f, y_pos_b = 0.f, z_pos_b = 0.f;

// Knob pickup tracking for oscillator switching
float last_x_knob = 0.f, last_y_knob = 0.f, last_z_knob = 0.f;
bool x_knob_picked_up = true, y_knob_picked_up = true, z_knob_picked_up = true;

// Stored voct value and pickup tracking for warp knob
float stored_voct = 0.f;
float stored_warp_knob = 0.f;  // Warp knob position when shift was pressed
bool warp_knob_picked_up = true;

// Reverse button long press tracking
uint32_t reverse_press_start = 0;
bool reverse_long_press_triggered = false;
constexpr uint32_t LONG_PRESS_MS = 5000;

// Sync flash feedback
bool sync_flash_active = false;
uint32_t sync_flash_start = 0;
constexpr uint32_t SYNC_FLASH_MS = 150;

// Octave crossing flash feedback
bool octave_flash_active = false;
uint32_t octave_flash_start = 0;
constexpr uint32_t OCTAVE_FLASH_MS = 80;
int last_octave = 0;  // Track previous octave for crossing detection

// ============================================================================
// Effect System
// ============================================================================
enum class EffectMode : uint8_t {
    FM = 0,
    RING_MOD = 1,
    BITCRUSH = 2,
    WAVEFOLD = 3,
    OVERDRIVE = 4,
    NUM_MODES = 5
};

EffectMode current_effect = EffectMode::FM;
float output_trim = 1.0f;  // 0.0 to 1.0, controlled by Shift+Mix

// DaisySP effect instances (one per channel)
Bitcrush bitcrush_a, bitcrush_b;
Fold fold_a, fold_b;
Overdrive overdrive_a, overdrive_b;

// Shift mode knob tracking for effects and trim
float last_blur_knob = 0.f;
float last_mix_knob = 0.f;

// Random number generator for gate randomization
uint32_t random_state = 12345;

inline float RandomFloat()
{
    random_state = random_state * 1664525 + 1013904223;  // LCG
    return static_cast<float>(random_state >> 8) / 16777216.f;  // 0.0 to 1.0
}

// ============================================================================
// USB State Machine
// ============================================================================
enum class UsbState
{
    WAITING,
    CONNECTED,
    LOADING,
    READY,
    ERROR
};

volatile UsbState usb_state = UsbState::WAITING;
wavetable::LoadStatus load_status = wavetable::LoadStatus::NOT_STARTED;

void UsbConnectCallback(void* userdata)
{
    usb_state = UsbState::CONNECTED;
}

void UsbDisconnectCallback(void* userdata)
{
    usb_state = UsbState::WAITING;
}

void UsbClassActiveCallback(void* userdata)
{
    usb_state = UsbState::LOADING;
}

void UsbErrorCallback(void* userdata)
{
    usb_state = UsbState::ERROR;
}

// ============================================================================
// Tone Degradation (Lo-Fi Mode)
// ============================================================================
namespace degradation
{

// State per channel
struct DegradationState
{
    float sr_phase = 0.f;
    float sr_hold = 0.f;
    uint32_t dither_state = 0x12345678;  // PRNG state
    float noise_shaped_error = 0.f;      // For noise shaping
};

DegradationState state_a, state_b;

// Fast PRNG for dithering
inline float RandomFloat(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) / 4294967296.f - 0.5f;
}

// Bit crushing with optional dithering
inline float Bitcrush(float in, int bits, bool dither, uint32_t& prng)
{
    float levels = static_cast<float>(1 << bits);
    if (dither)
    {
        // Triangular dither (TPDF)
        float d1 = RandomFloat(prng);
        float d2 = RandomFloat(prng);
        in += (d1 + d2) / levels;
    }
    return floorf(in * levels + 0.5f) / levels;
}

// Sample rate reduction
inline float SampleRateReduce(float in, float& phase, float& hold, float factor)
{
    phase += factor;
    if (phase >= 1.f)
    {
        phase -= 1.f;
        hold = in;
    }
    return hold;
}

// Noise shaping for better low-level detail
inline float NoiseShape(float in, float quantized, float& error)
{
    float shaped = in + error * 0.5f;
    float new_quantized = quantized;
    error = shaped - new_quantized;
    return new_quantized;
}

// Apply degradation based on mode
// Mode 0: Orthodox (clean 16-bit)
// Mode 1: Degraded (12-bit with dither, slight SR reduction)
inline float Process(float in, DegradationState& state, int mode)
{
    if (mode == 0) return in;  // Orthodox - clean
    
    // Degraded mode: 12-bit quantization with dithering + sample rate reduction
    float out = Bitcrush(in, 12, true, state.dither_state);
    out = NoiseShape(in, out, state.noise_shaped_error);
    out = SampleRateReduce(out, state.sr_phase, state.sr_hold, 0.85f);
    return out;
}

}  // namespace degradation

// DC Blocking Filters (using DaisySP)
DcBlock dc_block_a, dc_block_b;

// ============================================================================
// Input Detection (for FM normalling)
// ============================================================================
struct InputDetector
{
    float env = 0.f;
    static constexpr float THRESHOLD = 0.01f;
    static constexpr float ATTACK = 0.001f;
    static constexpr float RELEASE = 0.0001f;
    
    bool HasSignal(float in)
    {
        float abs_in = fabsf(in);
        if (abs_in > env)
            env += ATTACK * (abs_in - env);
        else
            env += RELEASE * (abs_in - env);
        return env > THRESHOLD;
    }
};

InputDetector input_detect_l, input_detect_r;

// ============================================================================
// Audio Callback
// ============================================================================
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // Note: hw.ProcessAllControls() is called in main loop, not here
    // This ensures button edge detection works correctly in ProcessControls()
    
    float sr = hw.AudioSampleRate();
    bool shift = hw.GetButton(SW_SHIFT).Pressed();
    float warp_knob = hw.GetKnobValue(KNOB_WARP);
    float warp_cv = hw.GetWarpVoct() / 12.f;  // CV in octaves (always process CV)
    
    // Frequency from V/Oct (Warp knob + CV)
    // Knob uses pickup logic to prevent pitch jumps when releasing shift
    // CV is always processed regardless of shift state
    if (!shift)
    {
        // Check if warp knob has been picked up (reached stored position)
        if (!warp_knob_picked_up)
        {
            if (fabsf(warp_knob - stored_warp_knob) < constants::KNOB_THRESHOLD)
            {
                warp_knob_picked_up = true;
            }
        }
        
        // Only update knob-based voct if knob is picked up
        if (warp_knob_picked_up)
        {
            stored_voct = warp_knob * 5.f;
        }
    }
    // CV is always added to stored voct (even during shift)
    float voct_with_cv = stored_voct + warp_cv;
    float freq_a = wavetable::VoctToFreq(voct_with_cv);
    
    // Detect octave crossing for LED flash feedback
    int current_octave = static_cast<int>(floorf(voct_with_cv));
    if (current_octave != last_octave)
    {
        octave_flash_active = true;
        octave_flash_start = System::GetNow();
        last_octave = current_octave;
    }
    
    // Oscillator B frequency with semitone offset (using cached ratio)
    float freq_b = freq_a * osc_b_freq_ratio;
    
    // Read knob values for wavetable axes
    float x_knob = hw.GetKnobValue(KNOB_TIME);
    float y_knob = hw.GetKnobValue(KNOB_REFLECT);
    float z_knob = hw.GetKnobValue(KNOB_ATMOSPHERE);
    
    // Knob pickup logic - only update position when knob reaches stored value
    // This prevents jumps when switching oscillator selection
    
    if (!shift)  // Don't update wavetable positions in shift mode
    {
        // Convert knob to position scale (0-7)
        float x_val = x_knob * 7.f;
        float y_val = y_knob * 7.f;
        float z_val = z_knob * 7.f;
        
        // Get target positions based on current oscillator selection
        float target_x = (current_osc_select == OscillatorSelect::OSC_B) ? x_pos_b : x_pos_a;
        float target_y = (current_osc_select == OscillatorSelect::OSC_B) ? y_pos_b : y_pos_a;
        float target_z = (current_osc_select == OscillatorSelect::OSC_B) ? z_pos_b : z_pos_a;
        
        // Pickup when knob reaches the stored position (within threshold)
        if (!x_knob_picked_up && fabsf(x_val - target_x) < constants::KNOB_THRESHOLD * 7.f)
        {
            x_knob_picked_up = true;
        }
        if (!y_knob_picked_up && fabsf(y_val - target_y) < constants::KNOB_THRESHOLD * 7.f)
        {
            y_knob_picked_up = true;
        }
        if (!z_knob_picked_up && fabsf(z_val - target_z) < constants::KNOB_THRESHOLD * 7.f)
        {
            z_knob_picked_up = true;
        }
        
        // Update positions based on oscillator selection and pickup state
        switch (current_osc_select)
        {
            case OscillatorSelect::OSC_A:
                if (x_knob_picked_up) x_pos_a = x_val;
                if (y_knob_picked_up) y_pos_a = y_val;
                if (z_knob_picked_up) z_pos_a = z_val;
                break;
            case OscillatorSelect::OSC_B:
                if (x_knob_picked_up) x_pos_b = x_val;
                if (y_knob_picked_up) y_pos_b = y_val;
                if (z_knob_picked_up) z_pos_b = z_val;
                break;
            case OscillatorSelect::OSC_BOTH:
            default:
                if (x_knob_picked_up) { x_pos_a = x_val; x_pos_b = x_val; }
                if (y_knob_picked_up) { y_pos_a = y_val; y_pos_b = y_val; }
                if (z_knob_picked_up) { z_pos_a = z_val; z_pos_b = z_val; }
                break;
        }
    }
    
    // CV offsets both oscillators from their knob-defined positions
    float cv_x = hw.GetCvValue(CV_TIME) * constants::CV_SCALE_XYZ;
    float cv_y = hw.GetCvValue(CV_REFLECT) * constants::CV_SCALE_XYZ;
    float cv_z = hw.GetCvValue(CV_ATMOSPHERE) * constants::CV_SCALE_XYZ;
    
    // Final positions with CV offset, clamped to valid range
    float final_x_a = fclamp(x_pos_a + cv_x, 0.f, 7.f);
    float final_y_a = fclamp(y_pos_a + cv_y, 0.f, 7.f);
    float final_z_a = fclamp(z_pos_a + cv_z, 0.f, 7.f);
    float final_x_b = fclamp(x_pos_b + cv_x, 0.f, 7.f);
    float final_y_b = fclamp(y_pos_b + cv_y, 0.f, 7.f);
    float final_z_b = fclamp(z_pos_b + cv_z, 0.f, 7.f);
    
    // Effect depth (Blur knob + CV)
    float effect_depth = hw.GetKnobValue(KNOB_BLUR);
    effect_depth += hw.GetCvValue(CV_BLUR) * constants::CV_SCALE_BLUR;
    effect_depth = fclamp(effect_depth, 0.f, 1.f);
    
    // Unison detune (Mix knob only - CV_MIX is now VCA)
    float detune = hw.GetKnobValue(KNOB_MIX) * 0.05f;  // 0-5% detune
    detune = fclamp(detune, 0.f, constants::MAX_DETUNE);
    
    // VCA from CV_MIX (0V = unity, positive = boost, negative = attenuate)
    float vca_cv = hw.GetCvValue(CV_MIX);
    float vca_gain = 1.0f + vca_cv;  // -1 to +1 maps to 0 to 2
    vca_gain = fmaxf(vca_gain, 0.f);  // Clamp negative to 0
    
    // Update oscillator A parameters
    osc_a.SetX(final_x_a);
    osc_a.SetY(final_y_a);
    osc_a.SetZ(final_z_a);
    osc_a.SetFreq(freq_a);
    osc_a.SetUnison(unison_enabled);
    osc_a.SetUnisonDetune(detune);
    osc_a.SetMorphX(morph_enabled);
    osc_a.SetMorphY(morph_enabled);
    osc_a.SetMorphZ(morph_enabled);
    
    // Update oscillator B parameters (independent positions, offset frequency)
    osc_b.SetX(final_x_b);
    osc_b.SetY(final_y_b);
    osc_b.SetZ(final_z_b);
    osc_b.SetFreq(freq_b);
    osc_b.SetUnison(unison_enabled);
    osc_b.SetUnisonDetune(detune);
    osc_b.SetMorphX(morph_enabled);
    osc_b.SetMorphY(morph_enabled);
    osc_b.SetMorphZ(morph_enabled);
    
    // External mode gain settings
    osc_a.SetExternalGain(effect_depth * 2.f);
    osc_b.SetExternalGain(effect_depth * 2.f);
    
    // Set FM amount based on effect mode (only FM mode uses it)
    float fm_amount = (current_effect == EffectMode::FM) ? effect_depth : 0.f;
    osc_a.SetFmAmount(fm_amount);
    osc_b.SetFmAmount(fm_amount);
    
    // Sub-oscillators: follow their respective main oscillator positions
    if (sub_a_level > constants::SUB_LEVEL_THRESHOLD)
    {
        osc_a_sub.SetX(final_x_a);
        osc_a_sub.SetY(final_y_a);
        osc_a_sub.SetZ(final_z_a);
        osc_a_sub.SetFreq(freq_a * 0.5f);  // One octave down
        osc_a_sub.SetMorphX(morph_enabled);
        osc_a_sub.SetMorphY(morph_enabled);
        osc_a_sub.SetMorphZ(morph_enabled);
        osc_a_sub.SetFmAmount(0.f);
        osc_a_sub.SetUnison(false);
    }
    
    if (sub_b_level > constants::SUB_LEVEL_THRESHOLD)
    {
        osc_b_sub.SetX(final_x_b);
        osc_b_sub.SetY(final_y_b);
        osc_b_sub.SetZ(final_z_b);
        osc_b_sub.SetFreq(freq_b * 0.5f);  // One octave down (from offset freq)
        osc_b_sub.SetMorphX(morph_enabled);
        osc_b_sub.SetMorphY(morph_enabled);
        osc_b_sub.SetMorphZ(morph_enabled);
        osc_b_sub.SetFmAmount(0.f);
        osc_b_sub.SetUnison(false);
    }
    
    for (size_t i = 0; i < size; i++)
    {
        float in_l = in[0][i];
        float in_r = in[1][i];
        
        float out_a, out_b;
        
        if (external_mode)
        {
            // Waveshaper mode: external audio drives wavetable phase
            out_a = osc_a.ProcessExternal(in_l);
            out_b = osc_b.ProcessExternal(in_r);
        }
        else
        {
            // Process based on current effect mode
            switch (current_effect)
            {
                case EffectMode::FM:
                {
                    // FM mode with cross-modulation (original behavior)
                    bool has_input_l = input_detect_l.HasSignal(in_l);
                    bool has_input_r = input_detect_r.HasSignal(in_r);
                    
                    float fm_for_b = has_input_r ? in_r : 0.f;
                    out_b = osc_b.Process(fm_for_b);
                    
                    float fm_for_a = has_input_l ? in_l : out_b;
                    out_a = osc_a.Process(fm_for_a);
                    break;
                }
                
                case EffectMode::RING_MOD:
                {
                    // Ring modulation: multiply A and B outputs
                    out_a = osc_a.Process(0.f);
                    out_b = osc_b.Process(0.f);
                    
                    float ring = out_a * out_b * 2.f;  // Boost ring output
                    out_a = out_a * (1.f - effect_depth) + ring * effect_depth;
                    out_b = out_b * (1.f - effect_depth) + ring * effect_depth;
                    break;
                }
                
                case EffectMode::BITCRUSH:
                {
                    // Process oscillators
                    out_a = osc_a.Process(0.f);
                    out_b = osc_b.Process(0.f);
                    
                    // Bitcrush: depth controls bit depth (16 to 2) and sample rate
                    int bits = 16 - static_cast<int>(effect_depth * 14.f);
                    float crush_rate = sr / (1.f + effect_depth * 47.f);
                    
                    bitcrush_a.SetBitDepth(bits);
                    bitcrush_b.SetBitDepth(bits);
                    bitcrush_a.SetCrushRate(crush_rate);
                    bitcrush_b.SetCrushRate(crush_rate);
                    
                    out_a = bitcrush_a.Process(out_a);
                    out_b = bitcrush_b.Process(out_b);
                    break;
                }
                
                case EffectMode::WAVEFOLD:
                {
                    // Process oscillators
                    out_a = osc_a.Process(0.f);
                    out_b = osc_b.Process(0.f);
                    
                    // Wavefold: depth controls fold intensity
                    float fold_amt = effect_depth * 0.2f;
                    fold_a.SetIncrement(fold_amt);
                    fold_b.SetIncrement(fold_amt);
                    
                    float folded_a = fold_a.Process(out_a);
                    float folded_b = fold_b.Process(out_b);
                    
                    // Wet/dry mix
                    out_a = out_a * (1.f - effect_depth) + folded_a * effect_depth;
                    out_b = out_b * (1.f - effect_depth) + folded_b * effect_depth;
                    break;
                }
                
                case EffectMode::OVERDRIVE:
                {
                    // Process oscillators
                    out_a = osc_a.Process(0.f);
                    out_b = osc_b.Process(0.f);
                    
                    // Overdrive: depth controls drive amount
                    overdrive_a.SetDrive(effect_depth);
                    overdrive_b.SetDrive(effect_depth);
                    
                    out_a = overdrive_a.Process(out_a);
                    out_b = overdrive_b.Process(out_b);
                    break;
                }
                
                default:
                    out_a = osc_a.Process(0.f);
                    out_b = osc_b.Process(0.f);
                    break;
            }
        }
        
        // Add sub-oscillators (wavetable at -1 octave) for low-end weight
        if (sub_a_level > constants::SUB_LEVEL_THRESHOLD)
        {
            out_a += osc_a_sub.Process() * sub_a_level;
        }
        if (sub_b_level > constants::SUB_LEVEL_THRESHOLD)
        {
            out_b += osc_b_sub.Process() * sub_b_level;
        }
        
        // Apply tone degradation
        out_a = degradation::Process(out_a, degradation::state_a, tone_mode);
        out_b = degradation::Process(out_b, degradation::state_b, tone_mode);
        
        // Apply VCA (CV_MIX)
        out_a *= vca_gain;
        out_b *= vca_gain;
        
        // Soft clip to prevent harsh clipping from VCA boost
        out_a = SoftClip(out_a);
        out_b = SoftClip(out_b);
        
        // Apply output trim (Shift+Mix)
        out_a *= output_trim;
        out_b *= output_trim;
        
        // DC blocking
        out_a = dc_block_a.Process(out_a);
        out_b = dc_block_b.Process(out_b);
        
        // Output
        out[0][i] = out_a;
        out[1][i] = out_b;
    }
}

// ============================================================================
// Shift+Knob State Tracking
// ============================================================================
// Track if we're in shift mode to capture knob values
bool shift_mode_active = false;
float last_time_knob = 0.f;
float last_reflect_knob = 0.f;
float last_warp_knob = 0.f;

// ============================================================================
// Button and Shift+Knob Handling
// ============================================================================
void ProcessControls()
{
    bool shift = hw.GetButton(SW_SHIFT).Pressed();
    uint32_t now = System::GetNow();
    
    // Shift+Knob handling
    if (shift)
    {
        if (!shift_mode_active)
        {
            // Just entered shift mode - capture current knob positions
            last_time_knob = hw.GetKnobValue(KNOB_TIME);
            last_reflect_knob = hw.GetKnobValue(KNOB_REFLECT);
            last_warp_knob = hw.GetKnobValue(KNOB_WARP);
            last_blur_knob = hw.GetKnobValue(KNOB_BLUR);
            last_mix_knob = hw.GetKnobValue(KNOB_MIX);
            // Store warp knob position for pickup on shift release
            stored_warp_knob = last_warp_knob;
            warp_knob_picked_up = false;
            shift_mode_active = true;
        }
        
        // Shift + Time = Sub A level
        float time_knob = hw.GetKnobValue(KNOB_TIME);
        if (fabsf(time_knob - last_time_knob) > constants::KNOB_THRESHOLD)
        {
            sub_a_level = time_knob;
            settings_dirty = true;
            last_settings_change = now;
            last_time_knob = time_knob;
        }
        
        // Shift + Reflect = Sub B level
        float reflect_knob = hw.GetKnobValue(KNOB_REFLECT);
        if (fabsf(reflect_knob - last_reflect_knob) > constants::KNOB_THRESHOLD)
        {
            sub_b_level = reflect_knob;
            settings_dirty = true;
            last_settings_change = now;
            last_reflect_knob = reflect_knob;
        }
        
        // Shift + Warp = Osc B semitone offset (±24 semitones)
        float warp_knob = hw.GetKnobValue(KNOB_WARP);
        if (fabsf(warp_knob - last_warp_knob) > constants::KNOB_THRESHOLD)
        {
            // Map 0.0-1.0 to -24 to +24 semitones
            osc_b_semitone_offset = static_cast<int>((warp_knob - 0.5f) * 48.f);
            if (osc_b_semitone_offset < -24) osc_b_semitone_offset = -24;
            if (osc_b_semitone_offset > 24) osc_b_semitone_offset = 24;
            // Cache frequency ratio to avoid powf() in audio callback
            // 2^(x/12) = e^(x * ln(2)/12) where ln(2)/12 ≈ 0.05776
            osc_b_freq_ratio = expf(0.05776226504666211f * static_cast<float>(osc_b_semitone_offset));
            settings_dirty = true;
            last_settings_change = now;
            last_warp_knob = warp_knob;
        }
        
        // Shift + Blur = Effect mode selection
        float blur_knob = hw.GetKnobValue(KNOB_BLUR);
        if (fabsf(blur_knob - last_blur_knob) > constants::KNOB_THRESHOLD)
        {
            // Map 0.0-1.0 to 5 discrete modes
            int mode = static_cast<int>(blur_knob * 4.99f);
            current_effect = static_cast<EffectMode>(mode);
            settings_dirty = true;
            last_settings_change = now;
            last_blur_knob = blur_knob;
        }
        
        // Shift + Mix = Output volume trim
        float mix_knob = hw.GetKnobValue(KNOB_MIX);
        if (fabsf(mix_knob - last_mix_knob) > constants::KNOB_THRESHOLD)
        {
            output_trim = mix_knob;  // 0.0 to 1.0
            settings_dirty = true;
            last_settings_change = now;
            last_mix_knob = mix_knob;
        }
    }
    else
    {
        shift_mode_active = false;
    }
    
    // Freeze button
    if (hw.GetButton(SW_FREEZE).RisingEdge())
    {
        if (shift)
        {
            // Toggle external mode
            external_mode = !external_mode;
            settings_dirty = true;
            last_settings_change = now;
        }
        else
        {
            // Toggle morph interpolation
            morph_enabled = !morph_enabled;
            settings_dirty = true;
            last_settings_change = now;
        }
    }
    
    // Reverse button - oscillator selection (tap) or sync B to A (long press)
    bool reverse_pressed = hw.GetButton(SW_REVERSE).Pressed();
    
    if (hw.GetButton(SW_REVERSE).RisingEdge())
    {
        // Always reset long press tracking on any press
        reverse_press_start = now;
        reverse_long_press_triggered = false;
        
        if (shift)
        {
            // Shift+Reverse: Cycle tone mode
            tone_mode = (tone_mode + 1) % 2;
            settings_dirty = true;
            last_settings_change = now;
            // Mark as triggered so tap action doesn't fire on release
            reverse_long_press_triggered = true;
        }
    }
    
    // Check for long press (sync B to A)
    if (reverse_pressed && !shift && !reverse_long_press_triggered)
    {
        if (now - reverse_press_start >= LONG_PRESS_MS)
        {
            // Long press: Sync oscillator B positions to A
            x_pos_b = x_pos_a;
            y_pos_b = y_pos_a;
            z_pos_b = z_pos_a;
            reverse_long_press_triggered = true;
            settings_dirty = true;
            last_settings_change = now;
            
            // Trigger sync flash feedback
            sync_flash_active = true;
            sync_flash_start = now;
        }
    }
    
    // Reverse button release - tap action (cycle oscillator selection)
    if (hw.GetButton(SW_REVERSE).FallingEdge() && !shift)
    {
        if (!reverse_long_press_triggered)
        {
            // Short tap: Cycle oscillator selection A -> B -> Both -> A
            switch (current_osc_select)
            {
                case OscillatorSelect::OSC_A:
                    current_osc_select = OscillatorSelect::OSC_B;
                    break;
                case OscillatorSelect::OSC_B:
                    current_osc_select = OscillatorSelect::OSC_BOTH;
                    break;
                case OscillatorSelect::OSC_BOTH:
                default:
                    current_osc_select = OscillatorSelect::OSC_A;
                    break;
            }
            settings_dirty = true;
            last_settings_change = now;
            
            // Reset knob pickup when switching oscillators
            x_knob_picked_up = false;
            y_knob_picked_up = false;
            z_knob_picked_up = false;
            last_x_knob = hw.GetKnobValue(KNOB_TIME);
            last_y_knob = hw.GetKnobValue(KNOB_REFLECT);
            last_z_knob = hw.GetKnobValue(KNOB_ATMOSPHERE);
        }
    }
    
    // Update sync flash state
    if (sync_flash_active && (now - sync_flash_start >= SYNC_FLASH_MS))
    {
        sync_flash_active = false;
    }
    
    // GATE_REVERSE: Randomize XYZ positions for both oscillators
    if (hw.GetGateTrig(GATE_REVERSE))
    {
        // Randomize all XYZ positions for both oscillators
        x_pos_a = RandomFloat() * 7.f;
        y_pos_a = RandomFloat() * 7.f;
        z_pos_a = RandomFloat() * 7.f;
        
        x_pos_b = RandomFloat() * 7.f;
        y_pos_b = RandomFloat() * 7.f;
        z_pos_b = RandomFloat() * 7.f;
        
        // Reset knob pickup so knobs don't immediately override
        x_knob_picked_up = false;
        y_knob_picked_up = false;
        z_knob_picked_up = false;
        
        // Flash LEDs to indicate randomization
        sync_flash_active = true;
        sync_flash_start = now;
    }
}

// ============================================================================
// LED Display
// ============================================================================
void UpdateLeds()
{
    bool shift = hw.GetButton(SW_SHIFT).Pressed();
    
    // Update octave flash state
    uint32_t now = System::GetNow();
    if (octave_flash_active && (now - octave_flash_start >= OCTAVE_FLASH_MS))
    {
        octave_flash_active = false;
    }
    
    // Sync flash overrides normal LED display
    if (sync_flash_active)
    {
        // Flash all position LEDs white during sync
        hw.SetLed(LED_1, 0.8f, 0.8f, 0.8f);
        hw.SetLed(LED_2, 0.8f, 0.8f, 0.8f);
        hw.SetLed(LED_3, 0.8f, 0.8f, 0.8f);
        hw.SetLed(LED_4, 0.8f, 0.8f, 0.8f);
        hw.SetLed(LED_5, 0.8f, 0.8f, 0.8f);
        hw.SetLed(LED_6, 0.8f, 0.8f, 0.8f);
    }
    else if (octave_flash_active)
    {
        // Flash all position LEDs white when crossing octave boundary
        hw.SetLed(LED_1, 0.9f, 0.9f, 0.9f);
        hw.SetLed(LED_2, 0.9f, 0.9f, 0.9f);
        hw.SetLed(LED_3, 0.9f, 0.9f, 0.9f);
        hw.SetLed(LED_4, 0.9f, 0.9f, 0.9f);
        hw.SetLed(LED_5, 0.9f, 0.9f, 0.9f);
        hw.SetLed(LED_6, 0.9f, 0.9f, 0.9f);
    }
    else if (shift)
    {
        // Shift mode: Show sub-oscillator levels, effect mode, and output trim
        // LED 1-2: Sub A level (cyan)
        float sub_a_brightness = sub_a_level;
        hw.SetLed(LED_1, 0.f, sub_a_brightness * 0.5f, sub_a_brightness * 0.7f);
        hw.SetLed(LED_2, 0.f, sub_a_brightness * 0.7f, sub_a_brightness);
        
        // LED 3-4: Effect mode indicator (5 distinct colors)
        switch (current_effect)
        {
            case EffectMode::FM:
                hw.SetLed(LED_3, 0.f, 0.6f, 0.f);   // Green
                hw.SetLed(LED_4, 0.f, 0.6f, 0.f);
                break;
            case EffectMode::RING_MOD:
                hw.SetLed(LED_3, 0.6f, 0.6f, 0.f);  // Yellow
                hw.SetLed(LED_4, 0.6f, 0.6f, 0.f);
                break;
            case EffectMode::BITCRUSH:
                hw.SetLed(LED_3, 0.6f, 0.f, 0.f);   // Red
                hw.SetLed(LED_4, 0.6f, 0.f, 0.f);
                break;
            case EffectMode::WAVEFOLD:
                hw.SetLed(LED_3, 0.6f, 0.f, 0.6f);  // Magenta
                hw.SetLed(LED_4, 0.6f, 0.f, 0.6f);
                break;
            case EffectMode::OVERDRIVE:
                hw.SetLed(LED_3, 0.6f, 0.3f, 0.f);  // Orange
                hw.SetLed(LED_4, 0.6f, 0.3f, 0.f);
                break;
            default:
                hw.SetLed(LED_3, 0.1f, 0.1f, 0.1f);
                hw.SetLed(LED_4, 0.1f, 0.1f, 0.1f);
                break;
        }
        
        // LED 5-6: Sub B level (magenta)
        float sub_b_brightness = sub_b_level;
        hw.SetLed(LED_5, sub_b_brightness * 0.7f, 0.f, sub_b_brightness * 0.5f);
        hw.SetLed(LED_6, sub_b_brightness, 0.f, sub_b_brightness * 0.7f);
        
        // Bottom LEDs in shift mode
        hw.SetLed(LED_BOT_1, 0.f, sub_a_brightness * 0.5f, sub_a_brightness * 0.5f);
        hw.SetLed(LED_BOT_2, sub_b_brightness * 0.5f, 0.f, sub_b_brightness * 0.5f);
        // LED_BOT_3: Output trim indicator (dim to bright white)
        hw.SetLed(LED_BOT_3, output_trim * 0.5f, output_trim * 0.5f, output_trim * 0.5f);
    }
    else
    {
        // Normal mode: Show X/Y/Z wavetable position with CV offset
        // Calculate final positions (knob + CV), clamped to 0-7
        float cv_x = hw.GetCvValue(CV_TIME) * constants::CV_SCALE_XYZ;
        float cv_y = hw.GetCvValue(CV_REFLECT) * constants::CV_SCALE_XYZ;
        float cv_z = hw.GetCvValue(CV_ATMOSPHERE) * constants::CV_SCALE_XYZ;
        
        float final_x_a = fclamp(x_pos_a + cv_x, 0.f, 7.f);
        float final_y_a = fclamp(y_pos_a + cv_y, 0.f, 7.f);
        float final_z_a = fclamp(z_pos_a + cv_z, 0.f, 7.f);
        float final_x_b = fclamp(x_pos_b + cv_x, 0.f, 7.f);
        float final_y_b = fclamp(y_pos_b + cv_y, 0.f, 7.f);
        float final_z_b = fclamp(z_pos_b + cv_z, 0.f, 7.f);
        
        // Normalize to 0-1 for LED brightness
        float disp_x_a = final_x_a / 7.f;
        float disp_y_a = final_y_a / 7.f;
        float disp_z_a = final_z_a / 7.f;
        float disp_x_b = final_x_b / 7.f;
        float disp_y_b = final_y_b / 7.f;
        float disp_z_b = final_z_b / 7.f;
        
        // Optional: Quantize to discrete steps in glitchy mode (8 steps for 8x8x8 cube)
        if (!morph_enabled)
        {
            disp_x_a = floorf(disp_x_a * 7.f + 0.5f) / 7.f;
            disp_y_a = floorf(disp_y_a * 7.f + 0.5f) / 7.f;
            disp_z_a = floorf(disp_z_a * 7.f + 0.5f) / 7.f;
            disp_x_b = floorf(disp_x_b * 7.f + 0.5f) / 7.f;
            disp_y_b = floorf(disp_y_b * 7.f + 0.5f) / 7.f;
            disp_z_b = floorf(disp_z_b * 7.f + 0.5f) / 7.f;
        }
        
        // Brightness multipliers based on oscillator selection
        // Selected oscillator = full brightness, other = dimmed
        float bright_a, bright_b;
        switch (current_osc_select)
        {
            case OscillatorSelect::OSC_A:
                bright_a = 1.0f;
                bright_b = 0.15f;  // Dimmed
                break;
            case OscillatorSelect::OSC_B:
                bright_a = 0.15f;  // Dimmed
                bright_b = 1.0f;
                break;
            case OscillatorSelect::OSC_BOTH:
            default:
                bright_a = 1.0f;
                bright_b = 1.0f;  // No dimming
                break;
        }
        
        // LED layout:
        // LED_1 (odd) = Osc A X position, LED_2 (even) = Osc B X position
        // LED_3 (odd) = Osc A Y position, LED_4 (even) = Osc B Y position
        // LED_5 (odd) = Osc A Z position, LED_6 (even) = Osc B Z position
        
        // X position: Red
        hw.SetLed(LED_1, disp_x_a * 0.7f * bright_a, disp_x_a * 0.1f * bright_a, 0.f);
        hw.SetLed(LED_2, disp_x_b * 0.7f * bright_b, disp_x_b * 0.1f * bright_b, 0.f);
        
        // Y position: Green
        hw.SetLed(LED_3, disp_y_a * 0.1f * bright_a, disp_y_a * 0.7f * bright_a, 0.f);
        hw.SetLed(LED_4, disp_y_b * 0.1f * bright_b, disp_y_b * 0.7f * bright_b, 0.f);
        
        // Z position: Blue
        hw.SetLed(LED_5, 0.f, disp_z_a * 0.1f * bright_a, disp_z_a * 0.7f * bright_a);
        hw.SetLed(LED_6, 0.f, disp_z_b * 0.1f * bright_b, disp_z_b * 0.7f * bright_b);
        
        // Bottom LEDs: Mode indicators
        // BOT_1: Morph mode (green = smooth, red = glitchy)
        if (sub_a_level > 0.01f)
            hw.SetLed(LED_BOT_1, 0.f, sub_a_level * 0.3f, sub_a_level * 0.5f);
        else if (morph_enabled)
            hw.SetLed(LED_BOT_1, 0.f, 0.5f, 0.f);
        else
            hw.SetLed(LED_BOT_1, 0.5f, 0.f, 0.f);
        
        // BOT_2: Unison indicator
        if (sub_b_level > 0.01f)
            hw.SetLed(LED_BOT_2, sub_b_level * 0.5f, 0.f, sub_b_level * 0.3f);
        else if (unison_enabled)
            hw.SetLed(LED_BOT_2, 0.f, 0.f, 0.5f);
        else
            hw.SetLed(LED_BOT_2, 0.1f, 0.1f, 0.1f);
        
        // BOT_3: External/Tone mode
        if (external_mode)
            hw.SetLed(LED_BOT_3, 0.5f, 0.f, 0.5f);
        else if (tone_mode == 1)
            hw.SetLed(LED_BOT_3, 0.5f, 0.25f, 0.f);
        else
            hw.SetLed(LED_BOT_3, 0.1f, 0.1f, 0.1f);
    }
    
    // Freeze LED: Loading status
    if (usb_state == UsbState::LOADING)
        hw.SetLed(LED_FREEZE, 0.5f, 0.5f, 0.f);  // Yellow = loading
    else if (usb_state == UsbState::ERROR)
    {
        switch (load_status)
        {
            case wavetable::LoadStatus::ERROR_FILE_NOT_FOUND:
                hw.SetLed(LED_FREEZE, 0.5f, 0.f, 0.f);
                break;
            case wavetable::LoadStatus::ERROR_NOT_RIFF:
            case wavetable::LoadStatus::ERROR_NOT_WAVE:
                hw.SetLed(LED_FREEZE, 0.5f, 0.2f, 0.f);
                break;
            case wavetable::LoadStatus::ERROR_NOT_PCM:
                hw.SetLed(LED_FREEZE, 0.5f, 0.f, 0.5f);
                break;
            case wavetable::LoadStatus::ERROR_NOT_MONO:
                hw.SetLed(LED_FREEZE, 0.f, 0.f, 0.5f);
                break;
            case wavetable::LoadStatus::ERROR_NOT_16BIT:
                hw.SetLed(LED_FREEZE, 0.5f, 0.5f, 0.5f);
                break;
            case wavetable::LoadStatus::ERROR_DATA_TOO_SMALL:
                hw.SetLed(LED_FREEZE, 0.f, 0.5f, 0.5f);
                break;
            case wavetable::LoadStatus::ERROR_NO_DRIVE:
                hw.SetLed(LED_FREEZE, 0.5f, 0.5f, 0.f);
                break;
            default:
                hw.SetLed(LED_FREEZE, 0.5f, 0.f, 0.f);
                break;
        }
    }
    else if (wavetable::wave_cube.loaded)
    {
        // Show morph state: green = smooth (enabled), red = glitchy (disabled)
        if (morph_enabled)
            hw.SetLed(LED_FREEZE, 0.f, 0.5f, 0.f);   // Green = smooth morph
        else
            hw.SetLed(LED_FREEZE, 0.5f, 0.f, 0.f);   // Red = glitchy (no interp)
    }
    else
        hw.SetLed(LED_FREEZE, 0.1f, 0.1f, 0.1f);
    
    // Reverse LED: Oscillator selection indicator
    // OSC_A = Orange/Red, OSC_B = Blue/Cyan, OSC_BOTH = White/Purple
    switch (current_osc_select)
    {
        case OscillatorSelect::OSC_A:
            hw.SetLed(LED_REVERSE, 0.6f, 0.2f, 0.f);   // Orange
            break;
        case OscillatorSelect::OSC_B:
            hw.SetLed(LED_REVERSE, 0.f, 0.3f, 0.6f);   // Cyan/Blue
            break;
        case OscillatorSelect::OSC_BOTH:
        default:
            hw.SetLed(LED_REVERSE, 0.4f, 0.2f, 0.5f);  // Purple/White
            break;
    }
    
    hw.WriteLeds();
}

// ============================================================================
// Settings Management
// ============================================================================
void CheckAndSaveSettings()
{
    if (!settings_storage) return;
    
    if (settings_dirty && (System::GetNow() - last_settings_change > constants::SAVE_DELAY_MS))
    {
        auto& settings = settings_storage->GetSettings();
        settings.morph_mode = morph_enabled ? 0 : 1;
        settings.unison_enabled = unison_enabled ? 1 : 0;
        settings.external_mode = external_mode ? 1 : 0;
        settings.tone_mode = tone_mode;
        settings.sub_a_level = static_cast<uint8_t>(sub_a_level * 255.f);
        settings.sub_b_level = static_cast<uint8_t>(sub_b_level * 255.f);
        
        // Oscillator selection and positions
        settings.osc_select = static_cast<uint8_t>(current_osc_select);
        settings.osc_b_semitone_offset = static_cast<int8_t>(osc_b_semitone_offset);
        settings.x_pos_a = static_cast<uint8_t>(x_pos_a * 255.f / 7.f);
        settings.y_pos_a = static_cast<uint8_t>(y_pos_a * 255.f / 7.f);
        settings.z_pos_a = static_cast<uint8_t>(z_pos_a * 255.f / 7.f);
        settings.x_pos_b = static_cast<uint8_t>(x_pos_b * 255.f / 7.f);
        settings.y_pos_b = static_cast<uint8_t>(y_pos_b * 255.f / 7.f);
        settings.z_pos_b = static_cast<uint8_t>(z_pos_b * 255.f / 7.f);
        
        // Effect system
        settings.effect_mode = static_cast<uint8_t>(current_effect);
        settings.output_trim = static_cast<uint8_t>(output_trim * 255.f);
        
        settings_storage->Save();
        settings_dirty = false;
    }
}

void LoadSettings()
{
    if (!settings_storage) return;
    
    auto& settings = settings_storage->GetSettings();
    morph_enabled = (settings.morph_mode == 0);
    unison_enabled = (settings.unison_enabled == 1);
    external_mode = (settings.external_mode == 1);
    tone_mode = settings.tone_mode;
    sub_a_level = static_cast<float>(settings.sub_a_level) / 255.f;
    sub_b_level = static_cast<float>(settings.sub_b_level) / 255.f;
    
    // Oscillator selection and positions
    uint8_t osc_sel = settings.osc_select;
    if (osc_sel <= 2)
        current_osc_select = static_cast<OscillatorSelect>(osc_sel);
    else
        current_osc_select = OscillatorSelect::OSC_BOTH;
    
    osc_b_semitone_offset = settings.osc_b_semitone_offset;
    if (osc_b_semitone_offset < -24) osc_b_semitone_offset = -24;
    if (osc_b_semitone_offset > 24) osc_b_semitone_offset = 24;
    osc_b_freq_ratio = expf(0.05776226504666211f * static_cast<float>(osc_b_semitone_offset));
    
    x_pos_a = static_cast<float>(settings.x_pos_a) * 7.f / 255.f;
    y_pos_a = static_cast<float>(settings.y_pos_a) * 7.f / 255.f;
    z_pos_a = static_cast<float>(settings.z_pos_a) * 7.f / 255.f;
    x_pos_b = static_cast<float>(settings.x_pos_b) * 7.f / 255.f;
    y_pos_b = static_cast<float>(settings.y_pos_b) * 7.f / 255.f;
    z_pos_b = static_cast<float>(settings.z_pos_b) * 7.f / 255.f;
    
    // Effect system
    uint8_t eff_mode = settings.effect_mode;
    if (eff_mode < static_cast<uint8_t>(EffectMode::NUM_MODES))
        current_effect = static_cast<EffectMode>(eff_mode);
    else
        current_effect = EffectMode::FM;
    
    output_trim = static_cast<float>(settings.output_trim) / 255.f;
}

// ============================================================================
// Main
// ============================================================================
int main(void)
{
    hw.Init();
    
    // Initialize oscillators
    float sr = hw.AudioSampleRate();
    osc_a.Init(sr);
    osc_b.Init(sr);
    
    // Initialize sub-oscillators (wavetable at -1 octave)
    osc_a_sub.Init(sr);
    osc_b_sub.Init(sr);
    
    // Initialize DC blocking filters
    dc_block_a.Init(sr);
    dc_block_b.Init(sr);
    
    // Initialize DaisySP effects
    bitcrush_a.Init(sr);
    bitcrush_b.Init(sr);
    fold_a.Init();
    fold_b.Init();
    overdrive_a.Init();
    overdrive_b.Init();
    
    // Initialize persistent storage
    static PersistentStorage<PistonSettings> storage(hw.seed.qspi);
    settings_storage = &storage;
    PistonSettings default_settings = {
        0,    // morph_mode (smooth)
        0,    // unison_enabled (off)
        0,    // external_mode (oscillator)
        0,    // tone_mode (orthodox)
        0,    // sub_a_level
        0,    // sub_b_level
        2,    // osc_select (OSC_BOTH)
        0,    // osc_b_semitone_offset
        0, 0, 0,  // x/y/z_pos_a
        0, 0, 0,  // x/y/z_pos_b
        0,    // effect_mode (FM)
        255   // output_trim (1.0 = full volume)
    };
    settings_storage->Init(default_settings, constants::SETTINGS_OFFSET);
    LoadSettings();
    
    // Prepare USB for wavetable loading
    hw.PrepareMedia(UsbConnectCallback, UsbDisconnectCallback,
                    UsbClassActiveCallback, UsbErrorCallback, nullptr);
    
    // Generate default wave cube (will be replaced if USB files found)
    wavetable::GenerateDefaultCube();
    
    // Start audio
    hw.StartAudio(AudioCallback);
    
    // Main loop
    while (1)
    {
        // Process USB host
        aurora::usb.Process();
        
        // Load wavetables when USB becomes active
        // Use GetReady() check similar to USBFileReader example for reliability
        if (usb_state == UsbState::LOADING || 
            (aurora::usb.GetReady() && !wavetable::wave_cube.loaded && usb_state != UsbState::ERROR))
        {
            usb_state = UsbState::LOADING;
            
            // Initial delay to let USB stabilize (matching USBFileReader example)
            System::Delay(1000);
            
            // Keep processing USB during wait
            for (int i = 0; i < 20; i++)
            {
                aurora::usb.Process();
                System::Delay(50);
            }
            
            // Initialize disk
            disk_initialize(0);
            System::Delay(100);
            
            // Mount filesystem
            const char* usb_path = aurora::fatfs_interface.GetUSBPath();
            FRESULT mount_res = f_mount(&aurora::fatfs_interface.GetUSBFileSystem(), usb_path, 1);
            
            if (mount_res != FR_OK)
            {
                // Try alternate path
                const char* alt_path = (usb_path[0] == '0') ? "1:/" : "0:/";
                mount_res = f_mount(&aurora::fatfs_interface.GetUSBFileSystem(), alt_path, 1);
                if (mount_res == FR_OK)
                {
                    usb_path = alt_path;
                }
            }
            
            System::Delay(100);
            
            // Try loading
            load_status = wavetable::LoadWaveCube(usb_path);
            
            if (load_status == wavetable::LoadStatus::SUCCESS)
            {
                usb_state = UsbState::READY;
            }
            else
            {
                usb_state = UsbState::ERROR;
            }
        }
        
        // Process hardware controls (buttons, knobs, CVs)
        // Must be called before ProcessControls() for edge detection to work
        hw.ProcessAllControls();
        
        // Process buttons and shift+knob controls
        ProcessControls();
        
        // Update LEDs
        UpdateLeds();
        
        // Save settings if needed
        CheckAndSaveSettings();
        
        System::Delay(10);
    }
}
