#include <ProcessSnapshot.h>

#include <Measurements/Measurement.h>
#include <Outputs/Output.h>

size_t ProcessSnapshot::measurementCount() const
{
    return measurementSampleCount;
}

const MeasurementSample* ProcessSnapshot::measurementAt(
    size_t index) const
{
    if (index >= measurementSampleCount)
        return nullptr;

    return &measurementSamples[index];
}

size_t ProcessSnapshot::outputCount() const
{
    return outputSampleCount;
}

const OutputSample* ProcessSnapshot::outputAt(
    size_t index) const
{
    if (index >= outputSampleCount)
        return nullptr;

    return &outputSamples[index];
}

const MeasurementSample* ProcessSnapshot::find(
    const Measurement& measurement) const
{
    for (size_t i = 0;
         i < measurementSampleCount;
         i++)
    {
        if (measurementSamples[i].source == &measurement)
            return &measurementSamples[i];
    }

    return nullptr;
}

const OutputSample* ProcessSnapshot::find(
    const Output& output) const
{
    for (size_t i = 0;
         i < outputSampleCount;
         i++)
    {
        if (outputSamples[i].source == &output)
            return &outputSamples[i];
    }

    return nullptr;
}

uint32_t ProcessSnapshot::capturedAt() const
{
    return captureTime;
}

void ProcessSnapshot::clear(uint32_t now)
{
    measurementSampleCount = 0;
    outputSampleCount = 0;
    captureTime = now;
}

bool ProcessSnapshot::add(
    const Measurement& measurement)
{
    if (measurementSampleCount >= MAX_MEASUREMENTS)
        return false;

    MeasurementSample& sample =
        measurementSamples[measurementSampleCount++];

    sample.source = &measurement;
    sample.value = measurement.getValue();
    sample.unit = measurement.getUnit();
    sample.decimals =
        measurement.printDecimals();
    sample.valid = measurement.isValid();

    return true;
}

bool ProcessSnapshot::add(
    const Output& output)
{
    if (outputSampleCount >= MAX_REGISTERED_OUTPUTS)
        return false;

    OutputSample& sample =
        outputSamples[outputSampleCount++];

    sample.source = &output;
    sample.appliedCommand =
        output.appliedCommand();
    sample.healthy = output.isHealthy();

    return true;
}
