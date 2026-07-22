#ifndef REGULATOR_H
#define REGULATOR_H

#include <Arduino.h>
#include <hmi/Displayable.h>

class Regulator : public Displayable
{
public:

    Regulator(const char* name);

    virtual ~Regulator() = default;

    virtual void update(uint32_t now) = 0;

    double_t readCommand() const;

    double_t printValue() const override;

    const char* getUnit() const override;

protected:

    void writeCommand(double_t value);

    double_t command;
};

#endif