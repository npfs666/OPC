// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Resistance.h>

#include <Hardware/SensorBoard.h>
#include <Physics/PT100.h>

#include <cmath>

namespace
{
    /*
     * Seuil de court-circuit d'une PT100 (×10 en PT1000) : bien sous
     * R(-200 °C) = 18,52 Ω et au-dessus de la résistance des lignes d'un
     * montage 2 fils.
     */
    constexpr double_t PT100_SHORT_CIRCUIT_OHMS = 10.0;
}

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

    if (!std::isfinite(resistance))
    {
        // Acquisition non aboutie : le capteur en donne la cause. Une
        // acquisition lisible sans résistance (voie thermocouple, entrée non
        // reliée à la carte) est invalide.
        const MeasurementStatus acquisition =
            sensor->acquisitionStatus();

        setStatus(
            acquisition == MeasurementStatus::Ok
                ? MeasurementStatus::Invalid
                : acquisition);
        return;
    }

    setStatus(classify(resistance));
}

MeasurementStatus Resistance::classify(double_t ohms) const
{
    double_t scale = 1.0;

    switch (sensor->settings.type)
    {
    case Sensor::Type::Pt100:
        break;

    case Sensor::Type::Pt1000:
        scale = 10.0;
        break;

    default:
        return MeasurementStatus::Invalid;
    }

    if (ohms < PT100_SHORT_CIRCUIT_OHMS * scale)
        return MeasurementStatus::Short;

    if (ohms < PT100::getTemperatureToResistance(
                   PT100::MINIMUM_TEMPERATURE) * scale)
        return MeasurementStatus::UnderRange;

    if (ohms > PT100::getTemperatureToResistance(
                   PT100::MAXIMUM_TEMPERATURE) * scale)
        return MeasurementStatus::OverRange;

    return MeasurementStatus::Ok;
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
