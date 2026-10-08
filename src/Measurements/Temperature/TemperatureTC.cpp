// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Temperature/TemperatureTC.h>
#include <Hardware/SensorBoard.h>

#include <cmath>

namespace
{
    namespace Tc = Physics::Thermocouple;

    /**
     * Cause d'une conversion impossible avec une tension et une jonction
     * froide finies : f.é.m. totale (référencée à 0 °C) hors du domaine de
     * mesure du type, ou jonction froide hors de son propre domaine.
     */
    MeasurementStatus rangeStatus(
        Tc::Type type,
        double voltageMv,
        double coldJunctionC)
    {
        const double coldJunctionMv =
            Tc::temperatureToMillivolts(type, coldJunctionC);

        const Tc::Range range = Tc::measurementRange(type);

        const double minimumMv =
            Tc::temperatureToMillivolts(type, range.minimum);
        const double maximumMv =
            Tc::temperatureToMillivolts(type, range.maximum);

        if (!std::isfinite(coldJunctionMv) ||
            !std::isfinite(minimumMv) ||
            !std::isfinite(maximumMv))
            return MeasurementStatus::Invalid;

        const double totalMv = voltageMv + coldJunctionMv;

        if (totalMv > maximumMv)
            return MeasurementStatus::OverRange;

        if (totalMv < minimumMv)
            return MeasurementStatus::UnderRange;

        return MeasurementStatus::Invalid;
    }
}

void TemperatureTC::begin(const char* name, Sensor& sensor)
{
    Temperature::begin(name);
    this->sensor = &sensor;
    setValue(NAN);
    setStatus(MeasurementStatus::NotReady);
}

void TemperatureTC::applyFilter(uint32_t now)
{
    if (sensor == nullptr)
        return;

    filterValue(now, sensor->settings.filterTime);
}

void TemperatureTC::update()
{
    setValue(NAN);

    if (sensor == nullptr || sensor->getBoard() == nullptr ||
        sensor->settings.type != Sensor::Type::Tc)
    {
        setStatus(MeasurementStatus::Invalid);
        return;
    }

    const auto& board = *sensor->getBoard();
    const double voltage = board.computeVoltage(*sensor);

    if (!std::isfinite(voltage))
    {
        // Thermocouple ouvert : la polarisation 1 MΩ sature l'entrée (Open).
        const MeasurementStatus acquisition =
            sensor->acquisitionStatus();

        setStatus(
            acquisition == MeasurementStatus::Ok
                ? MeasurementStatus::Invalid
                : acquisition);
        return;
    }

    const double coldJunction = board.getColdJunctionTemperature();

    const double temperature = Tc::compensatedTemperature(
        sensor->settings.thermocoupleType,
        voltage,
        coldJunction) + sensor->settings.offset;

    if (!std::isfinite(temperature))
    {
        setStatus(rangeStatus(
            sensor->settings.thermocoupleType,
            voltage,
            coldJunction));
        return;
    }

    setValue(temperature);
    setStatus(MeasurementStatus::Ok);
}
