// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THERMOSTAT_INSTALLATION_H
#define THERMOSTAT_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

#include <Regulator/LimitAlarm.h>
#include <Regulator/LoopBreakAlarm.h>
#include <Regulator/Thermostat.h>

class Adafruit_BMP5xx;
class Adafruit_GFX;
struct MeasurementSample;
class ProcessControl;
class SensorBoard;

/**
 * Exemple d'installation pour un thermostat à relais.
 *
 * Ce template n'est pas sélectionné par OPC. Il peut être copié,
 * renommé et adapté avant de remplacer l'installation utilisateur.
 */
class ThermostatInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void captureHomeScreenState() override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    // Entrée physique
    Sensor temperatureInput;

    // Mesures
    Resistance temperatureResistance;
    TemperatureRTD temperature;

    // Régulation
    Thermostat thermostat;

    // Commande du relais
    ActuatorOnOff relayActuator;
    RelayOutput relayOutput;

    // Alarmes (affichées seulement, désactivées par défaut)
    LimitAlarm alarm;
    LoopBreakAlarm loopAlarm;

    // Copie pour le cœur UI, faite par captureHomeScreenState().
    struct HomeState
    {
        Thermostat::Mode mode = Thermostat::Mode::Heating;
        double_t setpoint = NAN;    // consigne active
        double_t hysteresis = 0.0;
        bool ramping = false;
        bool commandValid = false;
        bool fallback = false;      // maintien sur défaut capteur
        bool manual = false;        // relais forcé par l'opérateur
    };

    HomeState homeState;

    // Dernier état dessiné de la jauge (cœur UI) : seul ce qui change est
    // redessiné. Positions en pixels, -1 si absent.
    struct GaugeState
    {
        int16_t setpointX = -1;
        int16_t bandStart = -1;
        int16_t bandEnd = -1;
        int16_t markerX = -1;
        bool drawn = false;
    };

    GaugeState gauge;

    // Mesure dessinée en nombre (true) ou en libellé d'état (false).
    bool measurementDrawn = false;
    bool measurementAsNumber = false;

    void printMeasurement(
        Adafruit_GFX& display,
        const MeasurementSample* sample);

    void printGauge(
        Adafruit_GFX& display,
        const MeasurementSample* sample);

    static void drawGaugeColumn(
        Adafruit_GFX& display,
        const GaugeState& state,
        int16_t x);
};

#endif
