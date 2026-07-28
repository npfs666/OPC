#ifndef PROCESSCONTROL_H
#define PROCESSCONTROL_H

#include <Arduino.h>

#include <Hardware/pinout.h>

#include <Measurements/Measurement.h>
#include <Regulator/Regulator.h>
#include <Outputs/Actuator.h>

#include <ParameterList.h>

class MeasurementSnapshot;
class ParameterEditor;

class ProcessControl
{
public:

    ProcessControl();

    void add(Measurement& measurement);
    void add(Regulator& regulator);
    void add(Actuator& actuator);

    void update(uint32_t now);

    void resume(uint32_t now);

    Measurement* getMeasurement(uint8_t id);

    void captureMeasurements(
        MeasurementSnapshot& destination,
        uint32_t now) const;

    void print(Stream& stream) const;

    void printCSVPsychro(uint32_t now);

    void registerParameters(ParameterList& list);

    bool validateParameters(
        const ParameterEditor& editor) const;

private:

    Measurement* measurements[MAX_MEASUREMENTS];
    uint8_t measurementCount;

    Regulator* regulators[MAX_REGULATORS];
    uint8_t regulatorCount;

    Actuator* actuators[MAX_ACTUATORS];
    uint8_t actuatorCount;
};

#endif
