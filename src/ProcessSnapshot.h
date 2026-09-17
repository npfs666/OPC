#ifndef PROCESS_SNAPSHOT_H
#define PROCESS_SNAPSHOT_H

#include <Hardware/pinout.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

class Measurement;
class DigitalInput;
class Output;
class ProcessControl;
class ProcessSnapshot;

struct DigitalInputSample
{
    bool active = false;
    bool valid = false;
    // Date de dernière lecture, indépendante de capturedAt().
    uint32_t sampledAt = 0;

private:
    friend class ProcessSnapshot;

    const DigitalInput* source = nullptr;
};

struct MeasurementSample
{
    double_t value = 0.0;
    const char* unit = "";
    uint8_t decimals = 3;
    bool valid = false;

private:
    friend class ProcessSnapshot;

    const Measurement* source = nullptr;
};

struct OutputSample
{
    double_t appliedCommand = 0.0;
    bool healthy = false;

private:
    friend class ProcessSnapshot;

    const Output* source = nullptr;
};

class ProcessSnapshot
{
public:
    size_t inputCount() const;

    const DigitalInputSample* inputAt(size_t index) const;

    const DigitalInputSample* find(const DigitalInput& input) const;

    size_t measurementCount() const;

    const MeasurementSample* measurementAt(
        size_t index) const;

    size_t outputCount() const;

    const OutputSample* outputAt(
        size_t index) const;

    const MeasurementSample* find(
        const Measurement& measurement) const;

    const OutputSample* find(
        const Output& output) const;

    uint32_t capturedAt() const;

private:
    friend class ProcessControl;

    void clear(uint32_t now);

    bool add(const DigitalInput& input);

    bool add(
        const Measurement& measurement);

    bool add(
        const Output& output);

    MeasurementSample measurementSamples[MAX_MEASUREMENTS] = {};
    DigitalInputSample digitalInputSamples[MAX_DIGITAL_INPUTS] = {};
    size_t digitalInputSampleCount = 0;
    OutputSample outputSamples[MAX_REGISTERED_OUTPUTS] = {};
    size_t measurementSampleCount = 0;
    size_t outputSampleCount = 0;
    uint32_t captureTime = 0;
};

#endif
