#include <ProcessSnapshot.h>

#include <Measurements/Measurement.h>
#include <Inputs/DigitalInput.h>
#include <Outputs/Output.h>
#include <Regulator/LimitAlarm.h>

size_t ProcessSnapshot::inputCount() const
{
    return digitalInputSampleCount;
}

const DigitalInputSample* ProcessSnapshot::inputAt(size_t index) const
{
    if (index >= digitalInputSampleCount)
        return nullptr;

    return &digitalInputSamples[index];
}

const DigitalInputSample* ProcessSnapshot::find(
    const DigitalInput& input) const
{
    for (size_t i = 0; i < digitalInputSampleCount; i++)
    {
        if (digitalInputSamples[i].source == &input)
            return &digitalInputSamples[i];
    }

    return nullptr;
}

bool ProcessSnapshot::add(const DigitalInput& input)
{
    if (digitalInputSampleCount >= MAX_DIGITAL_INPUTS)
        return false;

    DigitalInputSample& sample =
        digitalInputSamples[digitalInputSampleCount++];

    sample.source = &input;
    sample.active = input.isActive();
    sample.valid = input.isValid();
    sample.sampledAt = input.sampledAt();
    return true;
}

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

const ClockSample& ProcessSnapshot::clock() const
{
    return clockSample;
}

bool ProcessSnapshot::clockRequired() const
{
    return clockNeeded;
}

void ProcessSnapshot::clear(uint32_t now)
{
    digitalInputSampleCount = 0;
    measurementSampleCount = 0;
    outputSampleCount = 0;
    alarmSampleCount = 0;
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
    sample.status = measurement.getStatus();

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
    sample.name = output.getName();
    sample.healthy = output.isHealthy();
    sample.maintenanceDue =
        output.counters() != nullptr &&
        output.counters()->maintenanceDue();
    sample.waiting = output.isWaiting();

    return true;
}

size_t ProcessSnapshot::alarmCount() const
{
    return alarmSampleCount;
}

const AlarmSample* ProcessSnapshot::alarmAt(size_t index) const
{
    if (index >= alarmSampleCount)
        return nullptr;

    return &alarmSamples[index];
}

const AlarmSample* ProcessSnapshot::find(const LimitAlarm& alarm) const
{
    for (size_t i = 0; i < alarmSampleCount; i++)
    {
        if (alarmSamples[i].source == &alarm)
            return &alarmSamples[i];
    }

    return nullptr;
}

bool ProcessSnapshot::add(const LimitAlarm& alarm)
{
    if (alarmSampleCount >= MAX_ALARMS)
        return false;

    AlarmSample& sample = alarmSamples[alarmSampleCount++];

    sample.source = &alarm;
    sample.name = alarm.getName();
    sample.enabled = alarm.settings.enabled;
    sample.active = alarm.isActive();
    sample.latched = alarm.isLatched();
    return true;
}
