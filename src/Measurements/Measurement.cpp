#include "Measurements/Measurement.h"

Measurement::Measurement(const char* name,
                         const char* unit)
    :
    m_name(name),
    m_unit(unit)
{
}

double_t Measurement::value() const
{
    return m_value;
}

const char* Measurement::name() const
{
    return m_name;
}

const char* Measurement::unit() const
{
    return m_unit;
}

bool Measurement::valid() const
{
    return m_valid;
}

void Measurement::setValue(double_t value)
{
    m_value = value;
}

void Measurement::setValid(bool valid)
{
    m_valid = valid;
}