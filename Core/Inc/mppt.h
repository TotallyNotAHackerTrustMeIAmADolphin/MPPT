/**
  ******************************************************************************
  * @file           : mppt.h
  * @brief          : Header for MPPT algorithms (P&O and Sweep).
  ******************************************************************************
  */

#ifndef __MPPT_H
#define __MPPT_H

#include "system_types.h"

/**
 * @brief Performs a single step of the Perturb and Observe algorithm.
 * @param m Pointer to current measurements.
 * @param limits Pointer to system operational limits.
 * @return The new target duty cycle in ticks.
 */
int32_t MPPT_PerturbAndObserve(const Measurements_t *m, const DeviceLimits_t *limits);

/**
 * @brief Manages the global power sweep.
 * @param m Pointer to current measurements.
 * @param isFinished Pointer to a bool set to true when sweep is done.
 * @return The current sweep duty cycle in ticks.
 */
int32_t MPPT_RunSweep(const Measurements_t *m, bool *isFinished);

/**
 * @brief Resets the sweep internal state.
 */
void MPPT_ResetSweep(void);

/**
 * @brief Initializes the P&O tracking baseline with the current power measurement.
 * @param m Pointer to current measurements.
 */
void MPPT_StartTracking(const Measurements_t *m);

#endif /* __MPPT_H */
