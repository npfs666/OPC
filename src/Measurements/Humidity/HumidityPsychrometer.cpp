// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Measurements/Humidity/HumidityPsychrometer.h>

#include <Measurements/Psychrometer.h>
#include <hmi/ParameterList.h>

namespace
{
    constexpr ParameterOption FROZEN_BULB_OPTIONS[] = {
        {static_cast<int32_t>(Psychrometer::FrozenBulb::Ice), "Glace"},
        {static_cast<int32_t>(Psychrometer::FrozenBulb::Supercooled), "Eau surfondue"}
    };
}

HumidityPsychrometer::HumidityPsychrometer()
{
}

void HumidityPsychrometer::begin(
    const char* name,
    Psychrometer& psychrometer)
{
    Humidity::begin(name);
    this->psychrometer = &psychrometer;
}

void HumidityPsychrometer::update()
{
    if (psychrometer == nullptr)
    {
        setValid(false);
        return;
    }

    const Psychrometer::Result result =
        psychrometer->evaluate();

    if (result.status != MeasurementStatus::Ok)
    {
        setStatus(result.status);
        return;
    }

    setValue(result.relativeHumidity);
    setValid(true);
}

void HumidityPsychrometer::registerParameters(ParameterList& list)
{
    if (psychrometer == nullptr)
        return;

    auto parameters = list.forOwner({
        "measurements",
        "Mesures",
        getConfigurationKey(),
        getName()
    });

    Psychrometer::Settings& settings =
        psychrometer->settings();

    // 6,53 : ventilé (Assmann) ; 8 à 12 : non ventilé.
    parameters.addDouble(
        "coefficient",
        "Coef. psychro.",
        settings.coefficient,
        5.0,
        12.0,
        0.01,
        2,
        "e-4/K");

    parameters.addSelection(
        "frozen_bulb",
        "Bulbe < 0°C",
        settings.frozenBulb,
        FROZEN_BULB_OPTIONS);
}
