#include <unity.h>
#include <stddef.h>

#ifndef TIMER_PERIOD
#define TIMER_PERIOD 240
#endif

#include "../../Core/Src/power.c" // Include source directly for native test (NATIVE_TEST strips HAL calls)
#include "system_config.h"

// power.c is self-contained (no other module dependencies), so no mocks are
// needed here - these tests exercise the real hardware-map logic, not a copy.

static uint32_t sumDither(const uint16_t *table) {
    uint32_t sum = 0;
    for (int i = 0; i < DITHER_TABLE_SIZE; i++) {
        sum += table[i];
    }
    return sum;
}

void setUp(void) {
    POWER_PWM_Set(0);
}

void tearDown(void) {
}

// The fixed/non-switching leg must never reach the literal top of the
// physical PWM range (MAX_DUTY_CYCLE_TICKS) - it must always keep
// BOOTSTRAP_REFRESH_TICKS of low-side conduction available so its bootstrap
// cap keeps recharging, in deep buck, deep boost, at crossover, and at the
// extremes alike. This is the regression this suite exists to catch.
void test_bootstrap_ceiling_never_exceeded_across_full_range(void) {
    int32_t testDuties[] = {
        PWM_DEAD_BAND_TICKS + 10,                                   // just past hard-zero dead-band (deep buck)
        CEILING_TICKS / 4,                                          // deep buck
        CEILING_TICKS / 2,                                          // mid buck
        CEILING_TICKS - 200,                                        // buck side, outside crossover snap zone
        CEILING_TICKS,                                              // exact crossover
        CEILING_TICKS + 200,                                        // boost side, outside crossover snap zone
        CEILING_TICKS + (maxDutyCycle_ticks - CEILING_TICKS) / 2,    // mid boost
        maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS - 10,               // deep boost extreme
    };

    for (size_t i = 0; i < sizeof(testDuties) / sizeof(testDuties[0]); i++) {
        POWER_PWM_Set(testDuties[i]);

        uint32_t ch1 = sumDither(ditherTableCH1);
        uint32_t ch2 = sumDither(ditherTableCH2);

        TEST_ASSERT_LESS_OR_EQUAL_UINT32(CEILING_TICKS, ch1);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(CEILING_TICKS, ch2);
    }
}

// Deep buck: buck leg (CH2) modulates, boost leg (CH1) sits pinned at
// CEILING_TICKS for the entire time spent here - not literal 100% - so it
// still gets a refresh window on every single cycle.
void test_deep_buck_pins_boost_leg_at_ceiling(void) {
    int32_t duty = CEILING_TICKS / 2;
    POWER_PWM_Set(duty);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)duty, sumDither(ditherTableCH2));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)CEILING_TICKS, sumDither(ditherTableCH1));
}

// Deep boost: buck leg (CH2) pinned at ceiling, boost leg (CH1) modulates.
void test_deep_boost_pins_buck_leg_at_ceiling(void) {
    int32_t duty = CEILING_TICKS + (maxDutyCycle_ticks - CEILING_TICKS) / 2;
    POWER_PWM_Set(duty);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)CEILING_TICKS, sumDither(ditherTableCH2));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(2 * CEILING_TICKS - duty), sumDither(ditherTableCH1));
}

// The buck->boost handoff must be a single shared point, not a flat
// resolution-losing plateau: CH1 and CH2 should move by the same tick-for-tick
// amount immediately on either side of CEILING_TICKS, with no skipped band.
void test_crossover_is_single_point_not_plateau(void) {
    POWER_PWM_Set(CEILING_TICKS - 100);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(CEILING_TICKS - 100), sumDither(ditherTableCH2));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)CEILING_TICKS, sumDither(ditherTableCH1));

    POWER_PWM_Set(CEILING_TICKS);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)CEILING_TICKS, sumDither(ditherTableCH1));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)CEILING_TICKS, sumDither(ditherTableCH2));

    POWER_PWM_Set(CEILING_TICKS + 100);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)CEILING_TICKS, sumDither(ditherTableCH2));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(CEILING_TICKS - 100), sumDither(ditherTableCH1));
}

// POWER_CalculateVoltageMatchDuty must aim at the same logical crossover
// point (CEILING_TICKS) as the hardware map, or startup precharge duty is
// mis-aimed and risks a backflow fault.
void test_voltage_match_duty_uses_ceiling_reference(void) {
    // Buck mode: Vout <= Vin
    int32_t buckDuty = POWER_CalculateVoltageMatchDuty(20000, 10000);
    TEST_ASSERT_EQUAL_INT32(CEILING_TICKS / 2, buckDuty);

    // Boost mode: Vout > Vin
    int32_t boostDuty = POWER_CalculateVoltageMatchDuty(10000, 20000);
    TEST_ASSERT_EQUAL_INT32(CEILING_TICKS + CEILING_TICKS / 2, boostDuty);

    // Exact match (Vin == Vout) should land exactly on the crossover point.
    int32_t matchDuty = POWER_CalculateVoltageMatchDuty(15000, 15000);
    TEST_ASSERT_EQUAL_INT32(CEILING_TICKS, matchDuty);
}

// CH1Value must never go negative at the deep-boost extreme - this is the
// behavioral counterpart to the compile-time static_assert on
// 2*CEILING_TICKS >= maxDutyCycle_ticks. The top-of-range safety dead-band
// clamps hardwareDuty to (maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS) before
// the map runs, so that's the true worst case actually reachable.
void test_ch1_never_negative_at_max_boost_duty(void) {
    POWER_PWM_Set(maxDutyCycle_ticks);

    int32_t clampedDuty = maxDutyCycle_ticks - PWM_DEAD_BAND_TICKS;
    uint32_t expectedCh1 = (uint32_t)(2 * CEILING_TICKS - clampedDuty);

    TEST_ASSERT_EQUAL_UINT32(expectedCh1, sumDither(ditherTableCH1));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32((uint32_t)CEILING_TICKS, sumDither(ditherTableCH1));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_bootstrap_ceiling_never_exceeded_across_full_range);
    RUN_TEST(test_deep_buck_pins_boost_leg_at_ceiling);
    RUN_TEST(test_deep_boost_pins_buck_leg_at_ceiling);
    RUN_TEST(test_crossover_is_single_point_not_plateau);
    RUN_TEST(test_voltage_match_duty_uses_ceiling_reference);
    RUN_TEST(test_ch1_never_negative_at_max_boost_duty);
    return UNITY_END();
}
