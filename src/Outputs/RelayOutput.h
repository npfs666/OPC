#ifndef RELAYOUTPUT_H
#define RELAYOUTPUT_H

#include "Outputs/Output.h"

class RelayOutput : public Output
{
public:

    struct Settings
    {
        uint8_t pin;
        bool activeHigh;
    };

    Settings settings;

    RelayOutput(
        const char *name,
        uint8_t pin,
        bool activeHigh = true);

    void begin() override;

    void writeCommand(double_t value) override;
};

#endif