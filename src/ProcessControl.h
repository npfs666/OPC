#ifndef PROCESSCONTROL_H
#define PROCESSCONTROL_H

#include <Arduino.h>

#include <Hardware/pinout.h>

class Actuator;
class Measurement;
class MeasurementSnapshot;
class Output;
class ParameterEditor;
class ParameterList;
class Regulator;

class ProcessControl
{
public:

    ProcessControl();

    bool add(Measurement& measurement);
    bool add(Regulator& regulator);
    bool add(Actuator& actuator);
    bool add(Output& output);

    void updateMeasurementsAndRegulators(
        uint32_t now);

    void poll(uint32_t now);

    void resume(uint32_t now);

    bool beginOutputs();

    bool applyOutputSettings();

    void forceSafeOutputs();

    bool outputsHealthy() const;

    Measurement* getMeasurement(uint8_t id);

    void captureMeasurements(
        MeasurementSnapshot& destination,
        uint32_t now) const;

    void print(Stream& stream) const;

    void printCSVPsychro(Stream& stream) const;

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

    Output* outputs[MAX_REGISTERED_OUTPUTS];
    uint8_t outputCount;
};

#endif
