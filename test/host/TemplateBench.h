// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEMPLATE_BENCH_H
#define TEMPLATE_BENCH_H

#include "TestHarness.h"
#include "InstallationTestAccess.h"

#include <Adafruit_BMP5xx.h>
#include <Hardware/SensorBoard.h>
#include <Physics/PT100.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>

/**
 * Banc d'essai d'un template complet sur l'hôte : démarré comme par OPC,
 * sondes simulées par leur résistance (dans l'ordre d'addSensor()),
 * réglages modifiés comme par le menu, cycles de mesure à la demande.
 */
template<typename InstallationType>
struct TemplateBench
{
    SensorBoard board;
    Adafruit_BMP5xx bmp580;
    ProcessControl process;
    InstallationType installation;
    ProcessSnapshot snapshot;
    ClockSample clock;
    uint32_t now = 0;
    bool started = false;

    TemplateBench()
    {
        // millis() repart de zéro : les tests précédents l'ont avancé.
        FakeTime::milliseconds = 0;
        started = InstallationTestAccess::start(
            installation, board, bmp580, process);
        setClock(1, 12);
    }

    void setClock(uint8_t dayOfWeek, uint8_t hour, bool valid = true)
    {
        clock.dateTime.dayOfWeek = dayOfWeek;
        clock.dateTime.hour = hour;
        clock.dateTime.minute = 0;
        clock.dateTime.second = 0;
        clock.valid = valid;
    }

    void setTemperature(size_t sensor, double_t celsius)
    {
        board.sensorResistances[sensor] =
            PT100::getTemperatureToResistance(celsius);
    }

    // Sonde en court-circuit.
    void shortSensor(size_t sensor)
    {
        board.sensorResistances[sensor] = 5.0;
    }

    // Un cycle de mesure, 1 s après le précédent. millis() suit, pour les
    // composants qui le lisent (temps minimaux des relais...).
    void cycle()
    {
        now += 1000;
        FakeTime::milliseconds = now;
        process.updateClock(clock);
        process.updateMeasurementsAndRegulators(now);
    }

    // Cycles de 1 s pendant la durée donnée.
    void advance(uint32_t seconds)
    {
        for (uint32_t i = 0; i < seconds; i++)
            cycle();
    }

    // Mesure d'indice index, dans l'ordre de process.add(), vue par l'écran.
    const MeasurementSample* measurement(size_t index)
    {
        process.captureSnapshot(snapshot, now);
        return snapshot.measurementAt(index);
    }

    const Parameter* parameter(const char* owner, const char* key)
    {
        return installation.getParameters().find(owner, key);
    }

    void setNumber(const char* owner, const char* key, double_t value)
    {
        const Parameter* found = parameter(owner, key);
        CHECK_TRUE(found != nullptr);

        if (found != nullptr)
            *found->value.number = value;
    }

    void setBool(const char* owner, const char* key, bool value)
    {
        const Parameter* found = parameter(owner, key);
        CHECK_TRUE(found != nullptr);

        if (found != nullptr)
            *found->value.boolean = value;
    }

    // Entier ou sélection.
    void setDiscrete(const char* owner, const char* key, int32_t value)
    {
        const Parameter* found = parameter(owner, key);
        CHECK_TRUE(found != nullptr);

        if (found != nullptr)
            found->discrete.write(found->discrete.target, value);
    }
};

#endif
