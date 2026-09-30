#include <Measurements/Resistance.h>

#include <Hardware/SensorBoard.h>

#include <cmath>

Resistance::Resistance()
{
}

void Resistance::begin(const char* name,
                       SensorBoard& board,
                       Sensor& sensor)
{
    Measurement::begin(name, "Ω");

    this->board = &board;
    this->sensor = &sensor;
}

void Resistance::update()
{
    if (board == nullptr || sensor == nullptr)
    {
        setValid(false);
        return;
    }

    const double_t resistance =
        board->computeResistance(*sensor);

    setValue(resistance);

    // NaN : saturation, entrée non reliée à la carte ou valeur non physique.
    setValid(std::isfinite(resistance));
}

Sensor& Resistance::getSensor()
{
    return *sensor;
}

const Sensor& Resistance::getSensor() const
{
    return *sensor;
}

uint8_t Resistance::printDecimals() const
{
    return 3;
}
