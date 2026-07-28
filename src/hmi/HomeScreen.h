#ifndef HOMESCREEN_H
#define HOMESCREEN_H

#include <Arduino.h>

#include <Measurements/MeasurementSnapshot.h>

class Adafruit_GFX;

struct HomeScreenContext
{
    Adafruit_GFX& display;
    const MeasurementSnapshot& measurements;
    uint32_t now;
    bool fullRefresh;
};

#endif
