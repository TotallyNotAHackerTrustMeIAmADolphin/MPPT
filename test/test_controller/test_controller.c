#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "system_config.h"
#include "system_types.h"

#ifndef TIMER_PERIOD
#define TIMER_PERIOD 240
#endif

// Mocks
static Measurements_t mock_m = {0};
static DeviceLimits_t mock_l = {0};
static uint32_t mock_tick = 0;
static bool mock_calibrating = false;

// Mock Sensors
const Measurements_t* SENSORS_GetMeasurements(void) { return &mock_m; }

// Mock Settings
DeviceLimits_t* SETTINGS_GetLimits(void) { return &mock_l; }
bool SETTINGS_IsCalibrating(void) { return mock_calibrating; }
bool SETTINGS_IsCalHighSideOn(void) { return false; }

// Mock Power
static int32_t last_duty = 0;
void POWER_PWM_Set(int32_t duty) { last_duty = duty; }
int32_t POWER_PWM_GetMax(void) { return 240 * 16; }
void POWER_Start(void) {}
void POWER_Shutdown(void) {}
int32_t POWER_CalculateVoltageMatchDuty(int32_t vin, int32_t vout) { return 0; }

// Mock HAL
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, uint8_t state) {}
void HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin) {}
uint8_t HAL_GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin) { return 0; }

// Mock MPPT
static int32_t mock_mppt_delta = 0;
void MPPT_ResetSweep(int32_t startDuty) {}
void MPPT_StartTracking(const Measurements_t* m) {}
int32_t MPPT_RunSweep(const Measurements_t* m, const DeviceLimits_t* limits, bool* finished) { return 0; }
int32_t MPPT_PerturbAndObserve(const Measurements_t* m, const DeviceLimits_t* l) { return mock_mppt_delta; }
int32_t MPPT_IncrementalConductance(const Measurements_t* m, const DeviceLimits_t* l) { return mock_mppt_delta; }
uint32_t MPPT_GetInterval(void) { return 100; }
int32_t MPPT_GetLastStep(void) { return 13; }

// Mock Comms
void COMMS_SendTelemetry(const Measurements_t* m) {}

// Mock HAL
uint32_t HAL_GetTick(void) { return mock_tick; }
void HAL_Delay(uint32_t delay) { mock_tick += delay; }

// Include controller.c
#include "../../Core/Src/controller.c"

void setUp(void) {
    mock_tick = 0;
    mock_calibrating = false;
    last_duty = 0;
    mock_mppt_delta = 0;
    memset(&mock_m, 0, sizeof(mock_m));
    mock_m.voltageIn_mV = 20000; // Healthy input
    
    // Default Limits
    mock_l.mode = MODE_MPPT;
    mock_l.vOutMax_mV = 24000;
    mock_l.iOutMax_mA = 2000;
    mock_l.vInMin_mV = 14000;
    mock_l.vInMax_mV = 80000;
    mock_l.iInMin_mA = -500;
    mock_l.iOutMin_mA = -500;

    // Reset state
    currentState = STATE_IDLE;
    currentFaultReason = FAULT_REASON_NONE;
    activeSoftLimit = LIMIT_NONE;
    globalDutyIntegral = 0;
    targetDuty_ticks = 0;
    lastVout = 0;
    lastIout = 0;
    softLimitHoldTimer = 0;
    faultCounter = 0;
}

void tearDown(void) {}

void test_controller_transition_idle_to_active(void) {
    mock_m.voltageIn_mV = 20000;
    mock_l.mode = MODE_POWER_SUPPLY; // Avoid sweeping
    
    CONTROLLER_UpdateHighRate();
    
    TEST_ASSERT_EQUAL(STATE_ACTIVE, currentState);
}

void test_controller_cv_limit(void) {
    // Force active state
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000; // Some duty
    
    // Set Vout to limit
    mock_l.vOutMax_mV = 24000;
    mock_m.voltageOut_mV = 24100; // Over limit
    lastVout = 24050; // Increasing
    
    CONTROLLER_UpdateHighRate();
    
    TEST_ASSERT_EQUAL(LIMIT_V_OUT_MAX, activeSoftLimit);
    TEST_ASSERT_LESS_THAN(1000, last_duty); // Duty should have decreased
}

void test_controller_cc_limit(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000;
    
    mock_l.iOutMax_mA = 2000;
    mock_m.currentOut_mA = 2100; // Over limit
    lastIout = 2050; // Increasing
    
    CONTROLLER_UpdateHighRate();
    
    TEST_ASSERT_EQUAL(LIMIT_I_OUT_MAX, activeSoftLimit);
    TEST_ASSERT_LESS_THAN(1000, last_duty);
}

void test_controller_brownout_limit(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000;
    
    mock_l.vInMin_mV = 14000;
    mock_m.voltageIn_mV = 13900; // Below floor
    
    CONTROLLER_UpdateHighRate();
    
    TEST_ASSERT_EQUAL(LIMIT_V_IN_MIN, activeSoftLimit);
    TEST_ASSERT_LESS_THAN(1000, last_duty);
}

void test_controller_reverse_limit_output(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000;
    mock_l.mode = MODE_BIDIRECTIONAL; // Allow reverse current

    mock_l.iOutMin_mA = -500;
    mock_m.currentOut_mA = -600; // More negative than limit

    CONTROLLER_UpdateHighRate();

    TEST_ASSERT_EQUAL(LIMIT_I_OUT_MIN, activeSoftLimit);
    // Reverse (regen/charge) flow: increasing duty reduces reverse current.
    TEST_ASSERT_GREATER_THAN(1000, last_duty);
}

void test_controller_reverse_limit_input(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000;
    mock_l.mode = MODE_BIDIRECTIONAL; // Allow reverse current

    mock_l.iInMin_mA = -500;
    mock_m.currentIn_mA = -600; // More negative than limit - protects the source

    CONTROLLER_UpdateHighRate();

    TEST_ASSERT_EQUAL(LIMIT_I_IN_MIN, activeSoftLimit);
    // Reverse (regen/charge) flow: increasing duty reduces reverse current.
    TEST_ASSERT_GREATER_THAN(1000, last_duty);
}

void test_controller_reverse_cv_limit(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000;
    mock_l.mode = MODE_BIDIRECTIONAL;

    mock_l.vInMax_mV = 40000;
    mock_m.voltageIn_mV = 41000; // Over the reverse-charge ceiling

    CONTROLLER_UpdateHighRate();

    TEST_ASSERT_EQUAL(LIMIT_V_IN_MAX, activeSoftLimit);
    // Equilibrium-duty physics: increasing duty corrects a reverse overvoltage.
    TEST_ASSERT_GREATER_THAN(1000, last_duty);
}

void test_controller_mppt_tracking(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000;
    mock_l.mode = MODE_MPPT;
    
    // All limits are satisfied
    mock_m.voltageOut_mV = 20000;
    mock_m.currentOut_mA = 1000;
    mock_m.voltageIn_mV = 18000;
    lastVout = 20000;
    lastIout = 1000;
    
    // Trigger MPPT interval
    mock_tick = 200; 
    lastMPPTTick = 0;
    mock_mppt_delta = 5; // MPPT wants to increase duty
    
    CONTROLLER_UpdateHighRate();
    
    TEST_ASSERT_EQUAL(LIMIT_NONE, activeSoftLimit);
    // CC delta (2000) is smaller than MPPT delta (5000), so it wins the selector
    // 1000 + 2000/1000 = 1002
    TEST_ASSERT_EQUAL(1002, last_duty);
}

void test_controller_min_selector_priority(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 1000 * 1000;
    
    // CV wants to decrease duty slightly
    mock_l.vOutMax_mV = 24000;
    mock_m.voltageOut_mV = 24010; 
    lastVout = 24010; // delta_Vout will be GAIN_KI * -10 = -20
    
    // CC wants to decrease duty HEAVILY
    mock_l.iOutMax_mA = 2000;
    mock_m.currentOut_mA = 3000; // +1000mA error -> delta_Iout = GAIN_KI * -1000 = -2000
    lastIout = 3000;
    
    CONTROLLER_UpdateHighRate();
    
    // CC should win
    TEST_ASSERT_EQUAL(LIMIT_I_OUT_MAX, activeSoftLimit);
    TEST_ASSERT_EQUAL(1000 - 2, last_duty); // -2000 / 1000 = -2 ticks
}

void test_controller_backflow_hard_fault_input(void) {
    // 0 = no backflow allowed at all; hard backstop trips HARD_BACKFLOW_MARGIN_MA
    // beyond the configured limit, uniformly in every mode (no more mode-gating).
    mock_l.mode = MODE_BIDIRECTIONAL;
    mock_l.iInMin_mA = 0;
    currentState = STATE_ACTIVE;

    mock_m.currentIn_mA = -(HARD_BACKFLOW_MARGIN_MA + 100); // Past the backstop margin

    // BACKFLOW uses the standard 3-frame integration window, giving the soft
    // Reverse CC constraint a chance to correct first.
    for (int i = 0; i < FAULT_THRESHOLD_FRAMES; i++) CONTROLLER_UpdateHighRate();

    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);
    TEST_ASSERT_EQUAL(FAULT_REASON_BACKFLOW, currentFaultReason);
}

void test_controller_backflow_hard_fault_output(void) {
    mock_l.mode = MODE_POWER_SUPPLY;
    mock_l.iOutMin_mA = 0;
    currentState = STATE_ACTIVE;

    mock_m.currentOut_mA = -(HARD_BACKFLOW_MARGIN_MA + 100);

    for (int i = 0; i < FAULT_THRESHOLD_FRAMES; i++) CONTROLLER_UpdateHighRate();

    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);
    TEST_ASSERT_EQUAL(FAULT_REASON_BACKFLOW, currentFaultReason);
}

void test_controller_hard_safety_input_oc(void) {
    mock_m.voltageIn_mV = 20000;
    currentState = STATE_ACTIVE;

    // INPUT_OC uses a 3-frame integration window (not immediate)
    mock_m.currentIn_mA = HARD_LIMIT_IIN_MAX_MA + 1000; // > +20A
    for (int i = 0; i < FAULT_THRESHOLD_FRAMES; i++) CONTROLLER_UpdateHighRate();
    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);
    TEST_ASSERT_EQUAL(FAULT_REASON_INPUT_OC, currentFaultReason);

    // Reverse direction should trip it too
    CONTROLLER_ResetFault();
    currentState = STATE_ACTIVE;
    mock_m.currentIn_mA = -(HARD_LIMIT_IIN_MAX_MA + 1000); // < -20A
    for (int i = 0; i < FAULT_THRESHOLD_FRAMES; i++) CONTROLLER_UpdateHighRate();
    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);
    TEST_ASSERT_EQUAL(FAULT_REASON_INPUT_OC, currentFaultReason);
}

void test_controller_hard_safety_output_oc(void) {
    mock_m.voltageIn_mV = 20000;
    currentState = STATE_ACTIVE;

    mock_m.currentOut_mA = HARD_LIMIT_IOUT_MAX_MA + 1000; // > +20A
    for (int i = 0; i < FAULT_THRESHOLD_FRAMES; i++) CONTROLLER_UpdateHighRate();
    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);
    TEST_ASSERT_EQUAL(FAULT_REASON_OUTPUT_OC, currentFaultReason);

    CONTROLLER_ResetFault();
    currentState = STATE_ACTIVE;
    mock_m.currentOut_mA = -(HARD_LIMIT_IOUT_MAX_MA + 1000); // < -20A
    for (int i = 0; i < FAULT_THRESHOLD_FRAMES; i++) CONTROLLER_UpdateHighRate();
    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);
    TEST_ASSERT_EQUAL(FAULT_REASON_OUTPUT_OC, currentFaultReason);
}

void test_controller_hard_safety_uv_trip(void) {
    mock_m.voltageIn_mV = 20000;
    mock_l.mode = MODE_POWER_SUPPLY;
    currentState = STATE_ACTIVE;

    // Drop Vin below the mandatory 12.5V hard floor (HARD_LIMIT_VIN_MIN_MV)
    mock_m.voltageIn_mV = 12400;
    
    CONTROLLER_UpdateHighRate();
    
    // Should trigger FAULT immediately (1 frame)
    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);
    TEST_ASSERT_EQUAL(FAULT_REASON_INPUT_UV, currentFaultReason);
}

void test_controller_hard_safety_uv_recovery(void) {
    currentState = STATE_FAULT;
    currentFaultReason = FAULT_REASON_INPUT_UV;
    
    // 1. Below recovery threshold (13.5V)
    mock_m.voltageIn_mV = 13500;
    CONTROLLER_UpdateHighRate();
    TEST_ASSERT_EQUAL(STATE_FAULT, currentState);

    // 2. Above recovery threshold (14.1V)
    mock_m.voltageIn_mV = 14100;
    CONTROLLER_UpdateHighRate();
    TEST_ASSERT_EQUAL(STATE_IDLE, currentState);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_controller_transition_idle_to_active);
    RUN_TEST(test_controller_cv_limit);
    RUN_TEST(test_controller_cc_limit);
    RUN_TEST(test_controller_brownout_limit);
    RUN_TEST(test_controller_reverse_limit_output);
    RUN_TEST(test_controller_reverse_limit_input);
    RUN_TEST(test_controller_reverse_cv_limit);
    RUN_TEST(test_controller_mppt_tracking);
    RUN_TEST(test_controller_min_selector_priority);
    RUN_TEST(test_controller_backflow_hard_fault_input);
    RUN_TEST(test_controller_backflow_hard_fault_output);
    RUN_TEST(test_controller_hard_safety_input_oc);
    RUN_TEST(test_controller_hard_safety_output_oc);
    RUN_TEST(test_controller_hard_safety_uv_trip);
    RUN_TEST(test_controller_hard_safety_uv_recovery);
    return UNITY_END();
}
