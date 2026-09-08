#ifndef SOLAR_INSTALLATION_H
#define SOLAR_INSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

#include <Regulator/SolarRegulator.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Exemple d'installation pour un chauffe-eau solaire.
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
};

#endif
