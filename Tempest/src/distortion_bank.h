/** Distortion Algorithm Bank
 *  A collection of saturation and distortion algorithms organized by category.
 *  
 *  This is the main include file - it aggregates all distortion sub-modules:
 *  - types.h:      Enums, AlgorithmState, utility functions
 *  - tube_amp.h:   Tube amplifier simulation (FAM X mode)
 *  - freq_shift.h: Frequency shifter (FAM O mode)
 *  - algorithms.h: All 36 distortion algorithms
 *  - routing.h:    Process(), ProcessWithFam(), category info
 *  - noise_gate.h: NoiseGate class
 *  - crossover.h:  Crossover filter class
 */
#pragma once
#ifndef DISTORTION_BANK_H
#define DISTORTION_BANK_H

#include "distortion/types.h"
#include "distortion/tube_amp.h"
#include "distortion/freq_shift.h"
#include "distortion/algorithms.h"
#include "distortion/routing.h"
#include "distortion/noise_gate.h"
#include "distortion/crossover.h"

#endif  // DISTORTION_BANK_H
