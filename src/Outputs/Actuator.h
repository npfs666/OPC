#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <Arduino.h>
#include <Outputs/Output.h>
#include <Hardware/pinout.h>
#include <Regulator/Regulator.h>
#include <hmi/Displayable.h>
#include <Configurable.h>


class Actuator : public Displayable, public Configurable
{
public:

    Actuator();

    void begin(
        const char* name,
        Regulator& regulator);

    void begin(
        const char* key,
        const char* name,
        Regulator& regulator);
        
    virtual ~Actuator() = default;

    bool addOutput(Output& output);

    virtual void update(uint32_t now) = 0;

    virtual void resume(uint32_t now);

    void registerParameters(
        ParameterList& list) override;

protected:

    double_t printValue() const override;
    const char* getUnit() const override;

    Output* outputs[MAX_OUTPUTS] = {};

    uint8_t outputCount = 0;

    Regulator* regulator = nullptr;
};

#endif
