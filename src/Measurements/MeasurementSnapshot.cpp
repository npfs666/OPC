#include <Measurements/MeasurementSnapshot.h>

#include <Measurements/Measurement.h>

size_t MeasurementSnapshot::count() const
{
    return sampleCount;
}

const MeasurementSample* MeasurementSnapshot::at(
    size_t index) const
{
    if (index >= sampleCount)
        return nullptr;

    return &samples[index];
}

const MeasurementSample* MeasurementSnapshot::find(
    const Measurement& measurement) const
{
    for (size_t i = 0; i < sampleCount; i++)
    {
        if (samples[i].source == &measurement)
            return &samples[i];
    }

    return nullptr;
}

uint32_t MeasurementSnapshot::capturedAt() const
{
    return captureTime;
}

void MeasurementSnapshot::clear(uint32_t now)
{
    sampleCount = 0;
    captureTime = now;
}

bool MeasurementSnapshot::add(
    const Measurement& measurement)
{
    if (sampleCount >= MAX_MEASUREMENTS)
        return false;

    MeasurementSample& sample =
        samples[sampleCount++];

    sample.source = &measurement;
    sample.value = measurement.getValue();
    sample.unit = measurement.getUnit();
    sample.valid = measurement.isValid();

    return true;
}
