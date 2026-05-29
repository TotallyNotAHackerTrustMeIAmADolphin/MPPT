#include <unity.h>
#include <stdio.h>
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
int32_t POWER_PWM_GetMax(void) { return 240 * 8; }
void POWER_Start(void) {}
void POWER_Shutdown(void) {}
void POWER_PID_Compute(PID_t *pid) {} // Not used in unified
int32_t POWER_CalculateVoltageMatchDuty(int32_t vin, int32_t vout) { return 0; }

// Mock HAL
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, uint8_t state) {}
void HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin) {}
uint8_t HAL_GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin) { return 0; }

// Mock MPPT
void MPPT_ResetSweep(int32_t startDuty) {}
void MPPT_StartTracking(const Measurements_t* m) {}
int32_t MPPT_RunSweep(const Measurements_t* m, const DeviceLimits_t* limits, bool* finished) { return 0; }
int32_t MPPT_PerturbAndObserve(const Measurements_t* m, const DeviceLimits_t* l) { return 0; }
int32_t MPPT_IncrementalConductance(const Measurements_t* m, const DeviceLimits_t* l) { return 0; }
uint32_t MPPT_GetInterval(void) { return 100; }

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
    
    // Reset limits
    mock_l.mode = MODE_BIDIRECTIONAL;
    mock_l.vOutMax_mV = 30000; // 30V target
    mock_l.vInMin_mV = 11200;  // 11.2V LVD
    mock_l.iOutMax_mA = 5000;   // 5A drive limit
    mock_l.vInMax_mV = 20000;  // 20V regen limit
    mock_l.iOutMin_mA = -2000;   // 2A regen current limit
    
    // Reset state
    currentState = STATE_IDLE;
    currentFaultReason = FAULT_REASON_NONE;
    globalDutyIntegral = 0;
    lastVout = 0;
    lastIout = 0;
}

void tearDown(void) {}

void test_ebike_forward_drive(void) {
    // Start from IDLE with good battery
    mock_m.voltageIn_mV = 20000; // Above MIN_INPUT_VOLTAGE_MPPT_MV (18000)
    CONTROLLER_UpdateHighRate(); // Transition to ACTIVE
    TEST_ASSERT_EQUAL(STATE_ACTIVE, currentState);
    
    // Initial conditions after transition
    mock_m.voltageOut_mV = 20000;
    lastVout = 20000;
    
    // Run several iterations to see duty cycle increase
    bool reachedTarget = false;
    for(int i=0; i<500; i++) {
        CONTROLLER_UpdateHighRate();
        printf("Iter %d: Vout=%d, Duty=%d, Integral=%ld\n", i, (int)mock_m.voltageOut_mV, (int)last_duty, (long)globalDutyIntegral);
        mock_m.voltageOut_mV += (last_duty / 5);
        if (mock_m.voltageOut_mV >= 30000) reachedTarget = true;
    }
    
    // Should have reached the target voltage
    TEST_ASSERT_TRUE(reachedTarget);
}

void test_ebike_regen_braking(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 800 * 1000; // Start with some duty
    mock_m.voltageIn_mV = 13000;
    mock_m.voltageOut_mV = 35000; // Motor overspeed/braking
    lastVout = 35000;
    
    // Initial current is zero, now it becomes negative (regen)
    mock_m.currentOut_mA = -3000; // 3A regen (exceeds 2A limit)
    lastIout = -3000;
    
    CONTROLLER_UpdateHighRate();
    int32_t duty1 = last_duty;
    
    CONTROLLER_UpdateHighRate();
    int32_t duty2 = last_duty;
    
    // Duty should decrease to reduce regen current
    TEST_ASSERT_LESS_THAN(duty1, duty2); 
}

void test_ebike_soft_disconnect(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 500 * 1000;
    mock_m.voltageIn_mV = 11000; // Below 11.2V LVD
    mock_m.voltageOut_mV = 20000;
    lastVout = 20000;
    
    CONTROLLER_UpdateHighRate();
    int32_t duty1 = last_duty;
    TEST_ASSERT_GREATER_THAN(0, duty1);
    
    // Try to increase duty by lowering Vout
    mock_m.voltageOut_mV = 15000;
    lastVout = 15000;
    CONTROLLER_UpdateHighRate();
    
    // Duty should not increase because of soft disconnect
    TEST_ASSERT_LESS_OR_EQUAL(duty1, last_duty);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ebike_forward_drive);
    RUN_TEST(test_ebike_regen_braking);
    RUN_TEST(test_ebike_soft_disconnect);
    return UNITY_END();
}
