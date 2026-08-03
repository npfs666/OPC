#ifndef HOMESCREEN_H
#define HOMESCREEN_H

#include <cstdint>

class Adafruit_GFX;
class ProcessSnapshot;

struct HomeScreenContext
{
    Adafruit_GFX& display;
    const ProcessSnapshot& snapshot;
    uint32_t now;
    bool fullRefresh;
};

#endif
