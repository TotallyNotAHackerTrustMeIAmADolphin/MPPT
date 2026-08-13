#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "system_config.h"
#include "system_types.h"

#ifndef TIMER_PERIOD
#define TIMER_PERIOD 240
#endif

/* Mocks */
static Measurements_t mock_m;
static DeviceLimits_t mock_l;
static uint32_t mock_tick = 0;
static int32_t last_duty = 0;

/* Mock Sensors */
const Measurements_t* SENSORS_GetMeasurements(void) { return &mock_m; }

/* Mock Settings */
DeviceLimits_t* SETTINGS_GetLimits(void) { return &mock_l; }
bool SETTINGS_IsCalibrating(void) { return false; }
bool SETTINGS_IsCalHighSideOn(void) { return false; }

/* Mock Power */
void POWER_PWM_Set(int32_t duty) { last_duty = duty; }
int32_t POWER_PWM_GetMax(void) { return 240 * 16; }
void POWER_Start(void) {}
void POWER_Shutdown(void) { last_duty = 0; }
int32_t POWER_CalculateVoltageMatchDuty(int32_t vin, int32_t vout) { return 0; }

/* Mock HAL */
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, uint8_t state) {}
void HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin) {}
uint8_t HAL_GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin) { return 0; }
uint32_t HAL_GetTick(void) { return mock_tick; }
void HAL_Delay(uint32_t delay) { mock_tick += delay; }

/* Mock MPPT */
void MPPT_ResetSweep(int32_t startDuty) {}
void MPPT_StartTracking(const Measurements_t* m) {}
int32_t MPPT_RunSweep(const Measurements_t* m, const DeviceLimits_t* limits, bool* finished) { return 0; }
int32_t MPPT_PerturbAndObserve(const Measurements_t* m, const DeviceLimits_t* l) { return 0; }
int32_t MPPT_IncrementalConductance(const Measurements_t* m, const DeviceLimits_t* l) { return 0; }
uint32_t MPPT_GetInterval(void) { return 100; }
int32_t MPPT_GetLastStep(void) { return 0; }

/* Mock Comms */
void COMMS_SendTelemetry(const Measurements_t* m) {}

/* Include controller.c for whitebox access to its internal state
 * (currentState, globalDutyIntegral, lastVout, lastIout) */
#include "../../Core/Src/controller.c"

void setUp(void) {
    CONTROLLER_Init();
    softLimitHoldTimer = 0;
    mock_tick = 0;
    last_duty = 0;
    memset(&mock_m, 0, sizeof(mock_m));
    memset(&mock_l, 0, sizeof(mock_l));

    mock_l.mode = MODE_BIDIRECTIONAL;
    mock_l.vOutMax_mV = 25000; // 25V
    mock_l.iOutMax_mA = 5000;  // 5A forward limit
    mock_l.vInMax_mV = 20000;  // 20V reverse flow limit
    mock_l.iInMin_mA = -2000;   // 2A reverse current limit (input side)
    mock_l.iOutMin_mA = -2000;  // 2A reverse current limit (output side)

    mock_m.voltageIn_mV = 15000;
    mock_m.voltageOut_mV = 20000;
    mock_m.currentIn_mA = 0;
    mock_m.currentOut_mA = 0;
}

void tearDown(void) {}

void test_ebike_forward_drive(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 100 * 1000; // Start with low duty
    mock_m.voltageOut_mV = 10000;    // Output far below setpoint
    lastVout = 10000;

    // Simulate 50 frames of regulation
    bool reachedTarget = false;
    for(int i=0; i<50; i++) {
        CONTROLLER_UpdateHighRate();
        if (last_duty > 100) reachedTarget = true;
    }

    TEST_ASSERT_TRUE(reachedTarget);
}

void test_ebike_reverse_flow(void) {
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 800 * 1000; // Start with some duty
    mock_m.voltageIn_mV = 13000;
    mock_m.voltageOut_mV = 35000; // Motor overspeed/braking
    lastVout = 35000;

    // Initial current is zero, now it becomes negative (reverse flow)
    mock_m.currentOut_mA = -3000; // 3A reverse (exceeds 2A limit)
    lastIout = -3000;

    CONTROLLER_UpdateHighRate();
    int32_t duty1 = last_duty;

    CONTROLLER_UpdateHighRate();
    int32_t duty2 = last_duty;

    // Duty should increase to reduce reverse current (buck regulator)
    // Actually, in our min-max selector, reverse limits INCREASE duty.
    TEST_ASSERT_GREATER_THAN(duty1, duty2);
}

void test_ebike_soft_disconnect(void) {
    // Below the hard 12.5V UV floor (HARD_LIMIT_VIN_MIN_MV), the input-UV hard
    // fault always trips first and shuts down power outright, regardless of
    // mode - so a bidirectional-mode "soft LVD" can only ever apply above
    // that floor. Exercise the soft brownout constraint (LIMIT_V_IN_MIN) in
    // that reachable band instead.
    currentState = STATE_ACTIVE;
    globalDutyIntegral = 500 * 1000;
    mock_l.vInMin_mV = 12800;    // Bidirectional-mode brownout floor
    mock_m.voltageIn_mV = 12700; // Below the soft floor, still above the 12.5V hard floor
    mock_m.voltageOut_mV = 20000;
    lastVout = 20000;

    CONTROLLER_UpdateHighRate();

    // Brownout protection should pull duty down, not hard-fault
    TEST_ASSERT_EQUAL(STATE_ACTIVE, currentState);
    TEST_ASSERT_EQUAL(LIMIT_V_IN_MIN, activeSoftLimit);
    TEST_ASSERT_LESS_THAN(500, last_duty);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ebike_forward_drive);
    RUN_TEST(test_ebike_reverse_flow);
    RUN_TEST(test_ebike_soft_disconnect);
    return UNITY_END();
}
