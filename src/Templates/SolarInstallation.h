// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SOLAR_INSTALLATION_H
#define SOLAR_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

#include <Regulator/Comparator.h>
#include <Regulator/LimitAlarm.h>
#include <Regulator/LogicCommand.h>
#include <Regulator/Thermostat.h>
#include <Regulator/TimeSchedule.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Exemple d'installation pour un chauffe-eau solaire.
 *
 * - Relais 1 : pompe solaire. Les comparateurs donnent les conditions, la
 *   glue (processLogic()) les combine : charge quand le capteur est plus
 *   chaud que le bas du ballon, décharge nocturne en mode vacances
 *   (programme « Décharge nuit ») ;
 * - Relais 2 : résistance d'appoint, thermostat sur le haut du ballon actif
 *   uniquement pendant le programme « Heures creuses ».
 *
 * Ce template n'est pas sélectionné par OPC. Il peut être copié,
 * renommé et adapté avant de remplacer l'installation utilisateur.
 */
class SolarInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void processLogic(uint32_t now) override;

    void resumeLogic(uint32_t now) override;

    void captureHomeScreenState() override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    // Entrées physiques
    Sensor collectorInput;
    Sensor tankTopInput;
    Sensor tankBottomInput;

    // Mesures
    Resistance collectorResistance;
    Resistance tankTopResistance;
    Resistance tankBottomResistance;

    TemperatureRTD collectorTemperature;
    TemperatureRTD tankTopTemperature;
    TemperatureRTD tankBottomTemperature;

    // Conditions de la charge solaire
    Comparator chargeDelta;         // capteur - bas du ballon
    Comparator tankMaximum;         // haut du ballon trop chaud
    Comparator collectorMinimum;    // capteur assez chaud

    // Conditions de la décharge nocturne (mode vacances)
    bool holidayMode = false;
    TimeSchedule holidaySchedule;
    Comparator dischargeTank;       // bas du ballon encore chaud
    Comparator dischargeDelta;      // bas du ballon - capteur

    // Commande de la pompe, écrite par la glue
    LogicCommand pumpCommand;
    ActuatorOnOff pump;
    RelayOutput pumpRelay;

    // Décharge en cours (glue)
    bool discharging = false;

    // Appoint électrique en heures creuses
    TimeSchedule offPeakSchedule;
    Thermostat backupHeater;
    ActuatorOnOff heater;
    RelayOutput heaterRelay;

    // Alarme (affichée seulement, désactivée par défaut)
    LimitAlarm tankAlarm;

    // Copie pour le cœur UI, faite par captureHomeScreenState().
    struct HomeState
    {
        bool discharging = false;
        bool holidayMode = false;
        double_t startDelta = 0.0;

        // Programme heures creuses : état connu (heure valide) et en cours.
        bool offPeakKnown = false;
        bool offPeak = false;
    };

    HomeState homeState;
};

#endif
