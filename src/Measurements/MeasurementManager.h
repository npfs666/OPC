#ifndef MEASUREMENTMANAGER_H
#define MEASUREMENTMANAGER_H

#include "Measurement.h"

class MeasurementManager
{
public:

    static constexpr uint8_t MAX_MEASUREMENTS = 16;

    MeasurementManager();

    bool add(Measurement& measurement);

    void update();

    Measurement* get(uint8_t index);
    const Measurement* get(uint8_t index) const;

    Measurement& operator[](uint8_t index);
    const Measurement& operator[](uint8_t index) const;

    uint8_t getCount() const;

private:

    Measurement* measurements[MAX_MEASUREMENTS];

    uint8_t count;
};

#endif