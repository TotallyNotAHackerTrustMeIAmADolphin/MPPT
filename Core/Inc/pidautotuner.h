// PID automated tuning (Ziegler-Nichols/relay method) for Arduino and compatible boards
// Copyright (c) 2016-2020 jackw01
// This code is distributed under the MIT License, see LICENSE for details

#ifndef PIDAUTOTUNER_H
#define PIDAUTOTUNER_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    ZNModeBasicPID,
    ZNModeLessOvershoot,
    ZNModeNoOvershoot
} ZNMode;

typedef struct {
    // Configurable parameters
    float targetInputValue;
    float loopInterval;
    float minOutput;
    float maxOutput;
    ZNMode znMode;
    int cycles;

    // Tuning variables
    int i;
    int output;
    float outputValue;
    long microseconds;
    long t1, t2, tHigh, tLow;
    float max, min;
    float pAverage, iAverage, dAverage;

    // PID constants
    float kp, ki, kd;
} PIDAutotuner;

// Function prototypes
void PIDAutotuner_init(PIDAutotuner *tuner);
void PIDAutotuner_setTargetInputValue(PIDAutotuner *tuner, float target);
void PIDAutotuner_setLoopInterval(PIDAutotuner *tuner, long interval);
void PIDAutotuner_setOutputRange(PIDAutotuner *tuner, float min, float max);
void PIDAutotuner_setZNMode(PIDAutotuner *tuner, ZNMode zn);
void PIDAutotuner_setTuningCycles(PIDAutotuner *tuner, int tuneCycles);
void PIDAutotuner_startTuningLoop(PIDAutotuner *tuner, unsigned long us);
float PIDAutotuner_tunePID(PIDAutotuner *tuner, float input, unsigned long us);
float PIDAutotuner_getKp(PIDAutotuner *tuner);
float PIDAutotuner_getKi(PIDAutotuner *tuner);
float PIDAutotuner_getKd(PIDAutotuner *tuner);
int PIDAutotuner_isFinished(PIDAutotuner *tuner);
int PIDAutotuner_getCycle(PIDAutotuner *tuner);

#endif
