// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Resistance.h>
#include <Hardware/Sensor.h>

#include <Physics/PT100.h>
#include <cmath>

TemperatureRTD::TemperatureRTD()
{
}

void TemperatureRTD::begin(const char* name,
                         Resistance& resistance)
{
    Temperature::begin(name);
    this->resistance = &resistance;
}



void TemperatureRTD::applyFilter(uint32_t now)
{
    if (resistance == nullptr)
        return;

    filterValue(now, resistance->getSensor().settings.filterTime);
}

void TemperatureRTD::update()
{
    if(!resistance->isValid())
    {
        setStatus(resistance->getStatus());
        return;
    }

    double temperature = 0.0;

    switch(resistance->getSensor().settings.type)
    {
        case Sensor::Type::Pt100:
            temperature = PT100::getResistanceToTemperatureNewton(
                resistance->getValue());
            break;

        case Sensor::Type::Pt1000:
            // Même courbe que la PT100, avec une résistance dix fois plus grande.
            temperature = PT100::getResistanceToTemperatureNewton(
                resistance->getValue() / 10.0);
            break;

        default:
            setValid(false);
            return;
    }

    temperature += resistance->getSensor().settings.offset;

    if (!std::isfinite(temperature))
    {
        setValid(false);
        return;
    }

    setValue(temperature);
    setValid(true);
}
