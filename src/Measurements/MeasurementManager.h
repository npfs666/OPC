#ifndef MEASUREMENTMANAGER_H
#define MEASUREMENTMANAGER_H

#include "Hardware/pinout.h"

class Measurement;

class MeasurementManager
{
public:

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
