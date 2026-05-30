#include <unity.h>
#include "controller.h"
#include "system_types.h"
#include "power.h"
#include <stdio.h>
#include <stdlib.h>

/* Mocks */
static Measurements_t mock_m;
static DeviceLimits_t mock_l;
static int32_t last_duty = 0;

/* External variables from controller.c for whitebox testing */
extern SystemState_t currentState;
extern int64_t globalDutyIntegral;
extern int32_t lastVout;
extern int32_t lastIout;

/* Mock Implementations */
const Measurements_t* SENSORS_GetMeasurements(void) { return &mock_m; }
const DeviceLimits_t* SETTINGS_GetLimits(void) { return &mock_l; }
bool SETTINGS_IsCalibrating(void) { return false; }
void POWER_PWM_Set(int32_t duty) { last_duty = duty; }
int32_t POWER_PWM_GetMax(void) { return 240 * 8; }
void POWER_Shutdown(void) { last_duty = 0; }
void MPPT_StartTracking(const Measurements_t *m) {}
const char* CONTROLLER_GetStateString(void) { return "ACTIVE"; }
const char* CONTROLLER_GetFaultReasonString(void) { return "NONE"; }

void setUp(void) {
    CONTROLLER_Init();
    mock_l.mode = MODE_BIDIRECTIONAL;
    mock_l.vOutMax_mV = 25000; // 25V
    mock_l.iOutMax_mA = 5000;  // 5A forward limit
    mock_l.vInMax_mV = 20000;  // 20V reverse flow limit
    mock_l.iOutMin_mA = -2000;   // 2A reverse current limit
    
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
    RUN_TEST(test_ebike_reverse_flow);
    RUN_TEST(test_ebike_soft_disconnect);
    return UNITY_END();
}
