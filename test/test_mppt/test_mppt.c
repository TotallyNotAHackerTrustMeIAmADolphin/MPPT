#include <unity.h>
#include "../../Core/Src/mppt.c" // Include source directly for native test
#include "system_config.h"
#include "system_types.h"

// Mock for power.c functions needed by mppt.c
static int32_t mock_duty = 0;
void POWER_PWM_Set(int32_t duty) { mock_duty = duty; }
int32_t POWER_PWM_Get(void) { return mock_duty; }
int32_t POWER_PWM_GetMax(void) { return 240 * 8; }

void setUp(void) {
    mock_duty = 500;
    MPPT_SetStepSize(13);
    MPPT_SetThreshold(20000);
}

void tearDown(void) {
}

void test_mppt_increase_power(void) {
    Measurements_t m = {0};
    DeviceLimits_t l = {0};
    
    m.voltageIn_mV = 20000;
    m.powerOut_uW = 1000000; // 1W
    
    // Initial tracking baseline
    MPPT_StartTracking(&m);
    
    // Power increases significantly (> threshold)
    m.powerOut_uW = 2000000; // 2W
    int32_t delta = MPPT_PerturbAndObserve(&m, &l);
    
    // Should return positive delta (direction is true)
    TEST_ASSERT_GREATER_THAN(0, delta);
    TEST_ASSERT_EQUAL(13, delta);
}

void test_mppt_decrease_power(void) {
    Measurements_t m = {0};
    DeviceLimits_t l = {0};
    
    m.voltageIn_mV = 20000;
    m.powerOut_uW = 2000000; // 2W
    
    // Initial tracking baseline
    MPPT_StartTracking(&m);
    
    // Start with a small increase to ensure 'direction' is stable
    MPPT_PerturbAndObserve(&m, &l); // This updates previousPowerIn_uW to 2W
    
    // Now power decreases significantly (> threshold)
    m.powerOut_uW = 1000000; // 1W
    int32_t delta = MPPT_PerturbAndObserve(&m, &l);
    
    // dP = 1W - 2W = -1W. dP < 0, so direction should flip from true to false.
    // Result should be negative delta
    TEST_ASSERT_LESS_THAN(0, delta);
    TEST_ASSERT_EQUAL(-13, delta);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_mppt_increase_power);
    RUN_TEST(test_mppt_decrease_power);
    return UNITY_END();
}
