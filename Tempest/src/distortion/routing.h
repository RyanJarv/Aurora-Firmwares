/** Distortion Routing
 *  Main processing functions, FAM modes, and category information.
 */
#pragma once
#ifndef DISTORTION_ROUTING_H
#define DISTORTION_ROUTING_H

#include "types.h"
#include "algorithms.h"
#include "tube_amp.h"
#include "freq_shift.h"

namespace distortion
{

// Category information structure
struct CategoryInfo
{
    const char* name;
    float r, g, b;
    int algo_count;
};

static const CategoryInfo CATEGORIES[CAT_LAST] = {
    {"Analog",  1.0f, 0.5f, 0.0f, 6},  // Orange - Analog Saturation & Soft Clipping
    {"Fold",    0.7f, 0.0f, 1.0f, 6},  // Purple - Wavefolding & Complex Shaping
    {"Geomet",  1.0f, 1.0f, 0.0f, 6},  // Yellow - Geometric & Variable Shaping
    {"LoFi",    0.0f, 1.0f, 1.0f, 6},  // Cyan - Digital Degradation
    {"Chaos",   1.0f, 0.0f, 0.0f, 6},  // Red - Chaos, Noise & Physics
    {"Temprl",  0.0f, 1.0f, 0.0f, 6},  // Green - Temporal, Filtered & Experimental
};

// Algorithm names per category (6 banks x 6 algorithms = 36 total)
static const char* ALGO_NAMES[CAT_LAST][MAX_ALGOS_PER_CAT] = {
    // Bank 1: Analog Saturation & Soft Clipping
    {"SftClp", "HardClp", "StrgrC", "Valve", "Antipol", "Disto"},
    // Bank 2: Wavefolding & Complex Shaping
    {"WavFld", "Crest", "Sstrgi", "Fezz", "Fuss", "Cross"},
    // Bank 3: Geometric & Variable Shaping
    {"VariWS", "SmartE", "Vshpe", "Square", "Cubic", "Anti+"},
    // Bank 4: Digital Degradation (Lo-Fi)
    {"Bitcrsh", "DwnSpl", "Lowfi", "Lowrfi", "ChastB", "SuspB"},
    // Bank 5: Chaos, Noise & Physics
    {"Fractl", "Zippy", "Bounce", "NoizE", "BrwnNz", "Attrac"},
    // Bank 6: Temporal, Filtered & Experimental
    {"Filter", "TimeFr", "Vinyl", "Clicks", "Jelly", "Accum"},
};

// ============================================================================
// Main Processing Function
// ============================================================================

inline float Process(Category cat, int algo_idx, float x, float gain, float mod, float bias, float sample_rate, AlgorithmState& state)
{
    if (cat < 0 || cat >= CAT_LAST || algo_idx < 0 || algo_idx >= MAX_ALGOS_PER_CAT)
        return 0.f;
    
    switch (cat)
    {
        case CAT_ANALOG:
            switch (algo_idx)
            {
                case 0: return SftClp(x, gain, mod, bias);
                case 1: return HardClp(x, gain, mod, bias);
                case 2: return StrgrC(x, gain, mod, bias);
                case 3: return Valve(x, gain, mod, bias);
                case 4: return Antipol(x, gain, mod, bias);
                case 5: return Disto(x, gain, mod, bias, sample_rate, state);
                default: return 0.f;
            }
            
        case CAT_FOLD:
            switch (algo_idx)
            {
                case 0: return WavFld(x, gain, mod, bias);
                case 1: return Crest(x, gain, mod, bias);
                case 2: return Sstrgi(x, gain, mod, bias);
                case 3: return Fezz(x, gain, mod, bias);
                case 4: return Fuss(x, gain, mod, bias);
                case 5: return Cross(x, gain, mod, bias);
                default: return 0.f;
            }
            
        case CAT_GEOMETRIC:
            switch (algo_idx)
            {
                case 0: return VariWS(x, gain, mod, bias);
                case 1: return SmartE(x, gain, mod, bias);
                case 2: return Vshpe(x, gain, mod, bias);
                case 3: return Square(x, gain, mod, bias);
                case 4: return Cubic(x, gain, mod, bias);
                case 5: return AntiPlus(x, gain, mod, bias);
                default: return 0.f;
            }
            
        case CAT_LOFI:
            switch (algo_idx)
            {
                case 0: return Bitcrsh(x, gain, mod, bias, state);
                case 1: return DwnSpl(x, gain, mod, bias, state);
                case 2: return Lowfi(x, gain, mod, bias, state);
                case 3: return Lowrfi(x, gain, mod, bias, state);
                case 4: return ChastB(x, gain, mod, bias, state);
                case 5: return SuspB(x, gain, mod, bias, state);
                default: return 0.f;
            }
            
        case CAT_CHAOS:
            switch (algo_idx)
            {
                case 0: return Fractl(x, gain, mod, bias, state);
                case 1: return Zippy(x, gain, mod, bias, state);
                case 2: return Bounce(x, gain, mod, bias, state);
                case 3: return NoizE(x, gain, mod, bias);
                case 4: return BrwnNz(x, gain, mod, bias, state);
                case 5: return Attrac(x, gain, mod, bias, sample_rate, state);
                default: return 0.f;
            }
            
        case CAT_TEMPORAL:
            switch (algo_idx)
            {
                case 0: return Filter(x, gain, mod, bias, state);
                case 1: return TimeFr(x, gain, mod, bias, sample_rate, state);
                case 2: return Vinyl(x, gain, mod, bias, sample_rate, state);
                case 3: return Clicks(x, gain, mod, bias, state);
                case 4: return Jelly(x, gain, mod, bias, sample_rate, state);
                case 5: return Accum(x, gain, mod, bias, state);
                default: return 0.f;
            }
            
        default:
            return 0.f;
    }
}

// ============================================================================
// FAM Mode Processing
// ============================================================================

inline float ProcessWithFam(Category cat, int algo_idx, float x, float gain, 
                            float mod, float bias, FamMode fam, 
                            float sample_rate, AlgorithmState& state)
{
    float out;
    
    switch (fam)
    {
        case FAM_STANDARD:
        {
            // S mode: Raw algorithm with DC offset from bias
            float dc_offset = bias * 0.5f;
            float biased_in = x + dc_offset;
            out = Process(cat, algo_idx, biased_in, gain, mod, 0.f, sample_rate, state);
            out -= dc_offset * 0.5f;
            out *= 0.7f;  // Level compensation to match X mode
            break;
        }
        
        case FAM_XXX:
        {
            // X mode: Tube amp simulation
            float distorted = Process(cat, algo_idx, x, gain, mod, 0.f, sample_rate, state);
            out = TubeProcess(distorted, bias, state);
            break;
        }
        
        case FAM_ODD:
        {
            // O mode: Inharmonic distortion via frequency shifting
            float distorted = Process(cat, algo_idx, x, gain, mod, 0.f, sample_rate, state);
            float shift_hz = bias * 100.f;
            float shifted;
            FrequencyShift(distorted, shift_hz, sample_rate, shifted, state);
            float wet_amount = fabsf(bias);
            out = shifted * wet_amount + distorted * (1.f - wet_amount * 0.5f);
            out *= 0.7f;  // Level compensation to match X mode
            break;
        }
        
        default:
            out = Process(cat, algo_idx, x, gain, mod, bias, sample_rate, state);
            break;
    }
    
    if (!std::isfinite(out)) return 0.f;
    return fclamp(out, -1.f, 1.f);
}

// ============================================================================
// Helper Functions
// ============================================================================

inline const char* GetFamName(FamMode fam)
{
    switch (fam)
    {
        case FAM_STANDARD: return "S";
        case FAM_XXX:      return "X";
        case FAM_ODD:      return "O";
        default:           return "?";
    }
}

inline void GetFamColor(FamMode fam, float& r, float& g, float& b)
{
    switch (fam)
    {
        case FAM_STANDARD:
            r = 0.f; g = 1.f; b = 0.f;  // Green
            break;
        case FAM_XXX:
            r = 1.f; g = 0.5f; b = 0.f;  // Orange
            break;
        case FAM_ODD:
            r = 0.7f; g = 0.f; b = 1.f;  // Purple
            break;
        default:
            r = 0.f; g = 0.f; b = 0.f;
            break;
    }
}

inline const char* GetAlgorithmName(Category cat, int algo_idx)
{
    if (cat < CAT_LAST && algo_idx < MAX_ALGOS_PER_CAT)
        return ALGO_NAMES[cat][algo_idx];
    return "Unknown";
}

inline int GetAlgorithmCount(Category cat)
{
    if (cat < CAT_LAST)
        return CATEGORIES[cat].algo_count;
    return 0;
}

inline void GetCategoryColor(Category cat, float& r, float& g, float& b)
{
    if (cat < CAT_LAST)
    {
        r = CATEGORIES[cat].r;
        g = CATEGORIES[cat].g;
        b = CATEGORIES[cat].b;
    }
}

// ============================================================================
// Drive Compensation (legacy - kept for reference, now using Balance/Compressor)
// ============================================================================

inline float GetDriveCompensation(float drive)
{
    float drive_factor = cbrtf(drive);
    return 1.f / drive_factor;
}

inline float GetLowBandBoost(float drive)
{
    float drive_factor = sqrtf(drive);
    return 1.f + fastlogf(drive_factor + 1.f) * 0.4f;
}

}  // namespace distortion

#endif  // DISTORTION_ROUTING_H
