#ifndef SOLAR_INSTALLATION_H
#define SOLAR_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

#include <Regulator/SolarRegulator.h>
#include <Regulator/Thermostat.h>
#include <Regulator/TimeSchedule.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Exemple d'installation pour un chauffe-eau solaire.
 *
 * - Relais 1 : pompe solaire, avec décharge nocturne en mode vacances
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

    // Régulation
    SolarRegulator solarRegulator;

    // Commande de la pompe
    ActuatorOnOff pump;
    RelayOutput pumpRelay;

    // Décharge nocturne (mode vacances)
    TimeSchedule holidaySchedule;

    // Appoint électrique en heures creuses
    TimeSchedule offPeakSchedule;
    Thermostat backupHeater;
    ActuatorOnOff heater;
    RelayOutput heaterRelay;

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
