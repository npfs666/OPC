#ifndef REGULATOR_H
#define REGULATOR_H

#include <Arduino.h>
#include <hmi/Displayable.h>
#include <Configurable.h>

class Regulator : public Displayable, public Configurable
{
public:
    Regulator();

    void begin(const char* name);
    void begin(
        const char* key,
        const char* name);

    virtual ~Regulator() = default;

    virtual void update(uint32_t now) = 0;

    virtual void resume(uint32_t now);

    double_t readCommand() const;

    double_t printValue() const override;

    const char* getUnit() const override;

    void registerParameters(ParameterList& list) override {};

protected:

    const char* getKey() const;

    void writeCommand(double_t value);

    const char* key = "";
    double_t command = 0;
};

#endif
