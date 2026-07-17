#ifndef ANALOGMUX_H
#define ANALOGMUX_H

#include <Arduino.h>
#include <Hardware/pinout.h>

/**
 * This class includes all the multiplexers.
 *  -> One is in front of the ADC to set up the 2,3,4 wire measurement.
 *  -> And three more (RTD_MAX), one per input channel.
 */
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