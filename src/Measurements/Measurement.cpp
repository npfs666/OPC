#include "Measurements/Measurement.h"

Measurement::Measurement(const char* name,
                         const char* unit)
    :
    name(name),
    unit(unit)
{
}

double_t Measurement::getValue() const
{
    return value;
}

const char* Measurement::getName() const
{
    return name;
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

void Measurement::printSerial()
{
    char str[50];

    // %NAME% is %VALUE% %UNIT%
    sprintf ( str, "%s is %.3lf %s", name, value, unit );
    Serial.println(str);
}