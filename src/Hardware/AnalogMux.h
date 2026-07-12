#ifndef ANALOGMUX_H
#define ANALOGMUX_H

#include <Arduino.h>
#include <Hardware/pinout.h>

class AnalogMux
{
public:
    void begin();

    void enableChannel(uint8_t channel);
    void disableChannel(uint8_t channel);

    void set3Wire();
    void set4Wire();

private:
    void selectChannel(uint8_t channel);
};

 #endif