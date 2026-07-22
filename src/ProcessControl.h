#ifndef PROCESSCONTROL_H
#define PROCESSCONTROL_H

#include <Arduino.h>

#include <Hardware/pinout.h>

#include <Measurements/Measurement.h>
#include <Regulator/Regulator.h>
#include <Outputs/Actuator.h>

class ProcessControl
{
public:

    ProcessControl();

    void add(Measurement& measurement);
    void add(Regulator& regulator);
    void add(Actuator& actuator);

    void update(uint32_t now);

    Measurement* getMeasurement(uint8_t id);

    void print(Stream& stream) const;

private:

    Measurement* measurements[MAX_MEASUREMENTS];
    uint8_t measurementCount;

    Regulator* regulators[MAX_REGULATORS];
    uint8_t regulatorCount;

    Actuator* actuators[MAX_ACTUATORS];
    uint8_t actuatorCount;
};

#endif