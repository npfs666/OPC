#include "Measurements/Measurement.h"

#include <cstring>

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
    return status == MeasurementStatus::Ok;
}

MeasurementStatus Measurement::getStatus() const
{
    return status;
}

void Measurement::setValue(double_t value)
{
    this->value = value;
}

void Measurement::setValid(bool valid)
{
    setStatus(valid ? MeasurementStatus::Ok : MeasurementStatus::Invalid);
}

void Measurement::setStatus(MeasurementStatus status)
{
    this->status = status;
}

void Measurement::print(Stream& stream) const
{
    if (status == MeasurementStatus::Ok)
    {
        Displayable::print(stream);
        return;
    }

    if (!display)
        return;

    // Même mise en colonnes que Displayable::print().
    stream.print(getName());

    size_t len = std::strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");
    stream.println(measurementStatusLabel(status));
}

double_t Measurement::printValue() const {
    return value;
}
