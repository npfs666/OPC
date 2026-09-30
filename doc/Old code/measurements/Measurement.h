#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include<Arduino.h>

class Measurement
{
public:

    Measurement(const char* name,
                const char* unit)
        : _name(name),
          _unit(unit)
    {}

    virtual ~Measurement() = default;

    virtual void update() = 0;

    double_t value() const
    {
        return _value;
    }

    const char* name() const
    {
        return _name;
    }

    const char* unit() const
    {
        return _unit;
    }

protected:

    double_t _value = NAN;

private:

    const char* _name;
    const char* _unit;
};

#endif