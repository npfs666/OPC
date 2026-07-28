#ifndef MEASUREMENTSNAPSHOT_H
#define MEASUREMENTSNAPSHOT_H

#include <Arduino.h>

#include <Hardware/pinout.h>

class Measurement;
class MeasurementSnapshot;
class ProcessControl;

struct MeasurementSample
{
    double_t value = 0.0;
    const char* unit = "";
    bool valid = false;

private:
    friend class MeasurementSnapshot;

    const Measurement* source = nullptr;
};

class MeasurementSnapshot
{
public:
    size_t count() const;

    const MeasurementSample* at(
        size_t index) const;

    const MeasurementSample* find(
        const Measurement& measurement) const;

    uint32_t capturedAt() const;

private:
    friend class ProcessControl;

    void clear(uint32_t now);

    bool add(
        const Measurement& measurement);

    MeasurementSample samples[MAX_MEASUREMENTS] = {};
    size_t sampleCount = 0;
    uint32_t captureTime = 0;
};

#endif
