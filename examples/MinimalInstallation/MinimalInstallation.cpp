// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MinimalInstallation.h"

#include <Hardware/SensorBoard.h>
#include <ProcessControl.h>

#include <ProcessSnapshot.h>
#include <hmi/HomeScreen.h>

// Le dessin n'est compilé que pour la carte : les tests sur l'hôte
// vérifient le reste de l'installation.
#ifndef OPC_HOST_TEST
#include <Adafruit_GFX.h>

#include <hmi/MeasurementDisplay.h>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
}
#endif

const char* MinimalInstallation::name() const
{
    return "Installation minimale";
}

const char* MinimalInstallation::configurationKey() const
{
    return "minimal_installation";
}

bool MinimalInstallation::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    temperatureInput.begin(
        "minimal_temperature_input",
        "Sonde PT100",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0.0f);

    // fail() affiche la cause sur l'écran d'erreur de démarrage.
    if (!board.addSensor(temperatureInput))
        return fail("Sonde PT100 : entrée analogique indisponible");

    temperatureResistance.begin(
        "Resistance PT100",
        board,
        temperatureInput);

    temperature.begin(
        "Temperature PT100",
        temperatureResistance);

    if (!process.add(temperatureResistance) ||
        !process.add(temperature))
    {
        return fail("Mesures PT100 non enregistrées");
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres invalides");

    return true;
}

void MinimalInstallation::printHomeScreen(
    HomeScreenContext& context)
{
#ifdef OPC_HOST_TEST
    (void)context;
#else
    Adafruit_GFX& display = context.display;

    display.cp437(true);
    display.setTextWrap(false);

    if (context.fullRefresh)
    {
        display.fillScreen(COLOR_BLACK);
        display.setTextSize(2);
        display.setTextColor(
            COLOR_CYAN,
            COLOR_BLACK);
        display.setCursor(8, 8);
        display.print("Temperature");
    }

    display.fillRect(
        8,
        48,
        display.width() - 16,
        32,
        COLOR_BLACK);

    display.setCursor(8, 48);
    display.setTextSize(3);

    // Valeur et unité, ou état de la mesure ("RUPTURE"...) en couleur.
    MeasurementDisplay::print(
        display,
        context.snapshot.find(temperature),
        COLOR_WHITE,
        COLOR_BLACK);
#endif
}
