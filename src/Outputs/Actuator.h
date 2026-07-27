#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <Arduino.h>
#include <Outputs/Output.h>
#include <Hardware/pinout.h>
#include <Regulator/Regulator.h>
#include <hmi/Displayable.h>
#include <Configurable.h>


class Actuator : public Displayable, Configurable
{
public:

    Actuator();

    void begin(const char* name,Regulator& regulator);
        
    virtual ~Actuator() = default;

    void addOutput(Output& output);

    virtual void update(uint32_t now) = 0;

    void registerParameters(ParameterList& list) override {};

protected:

    Output* outputs[MAX_OUTPUTS];

    uint8_t outputCount;

    Regulator* regulator;
};

#endif