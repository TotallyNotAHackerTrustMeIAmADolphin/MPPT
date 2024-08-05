// PID automated tuning (Ziegler-Nichols/relay method) for Arduino and compatible boards
// Copyright (c) 2016-2020 jackw01
// This code is distributed under the MIT License, see LICENSE for details

#include "pidautotuner.h"

void PIDAutotuner_init(PIDAutotuner *tuner) {
    tuner->targetInputValue = 0;
    tuner->loopInterval = 0;
    tuner->minOutput = 0;
    tuner->maxOutput = 0;
    tuner->znMode = ZNModeNoOvershoot;
    tuner->cycles = 10;
    tuner->i = 0;
    tuner->output = 0;
    tuner->outputValue = 0;
    tuner->microseconds = 0;
    tuner->t1 = 0;
    tuner->t2 = 0;
    tuner->tHigh = 0;
    tuner->tLow = 0;
    tuner->max = -1000000000000.0;
    tuner->min = 1000000000000.0;
    tuner->pAverage = 0;
    tuner->iAverage = 0;
    tuner->dAverage = 0;
    tuner->kp = 0;
    tuner->ki = 0;
    tuner->kd = 0;
}

void PIDAutotuner_setTargetInputValue(PIDAutotuner *tuner, float target) {
    tuner->targetInputValue = target;
}

void PIDAutotuner_setLoopInterval(PIDAutotuner *tuner, long interval) {
    tuner->loopInterval = interval;
}

void PIDAutotuner_setOutputRange(PIDAutotuner *tuner, float min, float max) {
    tuner->minOutput = min;
    tuner->maxOutput = max;
}

void PIDAutotuner_setZNMode(PIDAutotuner *tuner, ZNMode zn) {
    tuner->znMode = zn;
}

void PIDAutotuner_setTuningCycles(PIDAutotuner *tuner, int tuneCycles) {
    tuner->cycles = tuneCycles;
}

void PIDAutotuner_startTuningLoop(PIDAutotuner *tuner, unsigned long us) {
    tuner->i = 0;
    tuner->output = 1;
    tuner->outputValue = tuner->maxOutput;
    tuner->t1 = tuner->t2 = us;
    tuner->microseconds = tuner->tHigh = tuner->tLow = 0;
    tuner->max = -1000000000000.0;
    tuner->min = 1000000000000.0;
    tuner->pAverage = tuner->iAverage = tuner->dAverage = 0;
}

float PIDAutotuner_tunePID(PIDAutotuner *tuner, float input, unsigned long us) {
    tuner->microseconds = us;
    tuner->max = (tuner->max > input) ? tuner->max : input;
    tuner->min = (tuner->min < input) ? tuner->min : input;

    if (tuner->output && input > tuner->targetInputValue) {
        tuner->output = 0;
        tuner->outputValue = tuner->minOutput;
        tuner->t1 = us;
        tuner->tHigh = tuner->t1 - tuner->t2;
        tuner->max = tuner->targetInputValue;
    }

    if (!tuner->output && input < tuner->targetInputValue) {
        tuner->output = 1;
        tuner->outputValue = tuner->maxOutput;
        tuner->t2 = us;
        tuner->tLow = tuner->t2 - tuner->t1;

        float ku = (4.0 * ((tuner->maxOutput - tuner->minOutput) / 2.0)) / 
                   (M_PI * (tuner->max - tuner->min) / 2.0);
        float tu = tuner->tLow + tuner->tHigh;

        float kpConstant, tiConstant, tdConstant;
        if (tuner->znMode == ZNModeBasicPID) {
            kpConstant = 0.6;
            tiConstant = 0.5;
            tdConstant = 0.125;
        } else if (tuner->znMode == ZNModeLessOvershoot) {
            kpConstant = 0.33;
            tiConstant = 0.5;
            tdConstant = 0.33;
        } else {
            kpConstant = 0.2;
            tiConstant = 0.5;
            tdConstant = 0.33;
        }

        tuner->kp = kpConstant * ku;
        tuner->ki = (tuner->kp / (tiConstant * tu)) * tuner->loopInterval;
        tuner->kd = (tdConstant * tuner->kp * tu) / tuner->loopInterval;

        if (tuner->i > 1) {
            tuner->pAverage += tuner->kp;
            tuner->iAverage += tuner->ki;
            tuner->dAverage += tuner->kd;
        }

        tuner->min = tuner->targetInputValue;
        tuner->i++;
    }

    if (tuner->i >= tuner->cycles) {
        tuner->output = 0;
        tuner->outputValue = tuner->minOutput;
        tuner->kp = tuner->pAverage / (tuner->i - 1);
        tuner->ki = tuner->iAverage / (tuner->i - 1);
        tuner->kd = tuner->dAverage / (tuner->i - 1);
    }

    return tuner->outputValue;
}

float PIDAutotuner_getKp(PIDAutotuner *tuner) { 
    return tuner->kp; 
}

float PIDAutotuner_getKi(PIDAutotuner *tuner) { 
    return tuner->ki; 
}

float PIDAutotuner_getKd(PIDAutotuner *tuner) { 
    return tuner->kd; 
}

int PIDAutotuner_isFinished(PIDAutotuner *tuner) {
    return (tuner->i >= tuner->cycles);
}

int PIDAutotuner_getCycle(PIDAutotuner *tuner) {
    return tuner->i;
}
