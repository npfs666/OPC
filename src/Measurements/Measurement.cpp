#include "Measurements/Measurement.h"

Measurement::Measurement()
{
}

void Measurement::begin(const char* name, const char* unit)
{
    begin(name, name, unit);
}

void Measurement::begin(
    const char* key,
    const char* name,
    const char* unit)
{
    beginConfiguration(key);
    Displayable::begin(name);
    this->unit = unit;
}

double_t Measurement::getValue() const
{
    return value;
}

const char* Measurement::getUnit() const
{
    return unit;
}

bool Measurement::isValid() const
{
    return valid;
}

void Measurement::setValue(double_t value)
{
    this->value = value;
}

void Measurement::setValid(bool valid)
{
    this->valid = valid;
}

double_t Measurement::printValue() const {
    return value;
}
