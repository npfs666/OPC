#ifndef ANALOGMUX_H
#define ANALOGMUX_H

#include <cstdint>
#include <Adafruit_MCP23X17.h>

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

    void resetMeasurementMode();
    void resetMeasurementType();

    void set2Wire();
    void set3Wire();
    void set4Wire();
    void setTC();

    void setPT100();
    void setPT1000();

private:
    Adafruit_MCP23X17 mcp;
};

 #endif
