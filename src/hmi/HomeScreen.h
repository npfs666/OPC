#ifndef HOMESCREEN_H
#define HOMESCREEN_H

#include <cstdint>

class Adafruit_GFX;
class MeasurementSnapshot;

struct HomeScreenContext
{
    Adafruit_GFX& display;
    const MeasurementSnapshot& measurements;
    uint32_t now;
    bool fullRefresh;
};

#endif
