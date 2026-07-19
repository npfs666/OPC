#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <Arduino.h>
#include <Outputs/Output.h>
#include <Hardware/pinout.h>


class Actuator
{
public:

    Actuator(const char* name);
    virtual ~Actuator() = default;

    void addOutput(Output& output);

    virtual void update(uint32_t now) = 0;

    virtual void writeCommand(double_t value);

    double_t readCommand() const;

    const char* readName() const;

protected:

    const char* actuatorName;

    double_t command;

    Output* outputs[MAX_OUTPUTS];

    uint8_t outputCount;
};

#endif