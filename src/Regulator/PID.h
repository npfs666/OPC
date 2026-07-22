#ifndef PID_H
#define PID_H

#include <Arduino.h>

#include <Measurements/Measurement.h>
#include <Regulator/Regulator.h>

class PID : public Regulator
{
public:

    struct Settings
    {
        double_t setpoint;

        double_t kp;
        double_t ki;
        double_t kd;

        double_t outputMin;
        double_t outputMax;
    };

    Settings settings;

    PID(
        const char* name,
        Measurement& measurement);

    void reset();

    void update(uint32_t now) override;

private:

    Measurement& measurement;

    double_t integral;

    double_t previousMeasurement;

    uint32_t previousTime;

    bool initialized;
};

#endif