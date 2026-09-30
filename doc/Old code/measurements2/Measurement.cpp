#include "Measurements/Measurement.h"

Measurement::Measurement(const char* name,
                         const char* unit)
    :
    name(name),
    unit(unit)
{
}

double_t Measurement::value() const
{
    return value;
}

const char* Measurement::name() const
{
    return name;
}

const char* Measurement::unit() const
{
    return unit;
}

bool Measurement::valid() const
{
    return valid;
}

void Measurement::setValue(double_t value)
{
    value = value;
}

void Measurement::setValid(bool valid)
{
    valid = valid;
}