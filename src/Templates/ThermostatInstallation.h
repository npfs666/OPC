#ifndef THERMOSTAT_INSTALLATION_H
#define THERMOSTAT_INSTALLATION_H

#include <Installation.h>

#include <Hardware/RTDSensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

#include <Regulator/Thermostat.h>

class Adafruit_BME280;
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
        Adafruit_BME280& bme,
        ProcessControl& process) override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    // Entrée physique
    RTDSensor temperatureInput;

    // Mesures
    Resistance temperatureResistance;
    TemperatureRTD temperature;

    // Régulation
    Thermostat thermostat;

    // Commande du relais
    ActuatorOnOff relayActuator;
    RelayOutput relayOutput;
};

#endif
