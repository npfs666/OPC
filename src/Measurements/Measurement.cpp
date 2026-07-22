#include "Measurements/Measurement.h"
#include <PrintSize.h>

Measurement::Measurement(const char* name,
                         const char* unit)
    :
    Displayable(name),
    unit(unit)
{
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