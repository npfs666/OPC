#ifndef SOLAR_INSTALLATION_H
#define SOLAR_INSTALLATION_H

#include <Installation.h>

#include <Hardware/RTDSensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

#include <Regulator/SolarRegulator.h>

class Adafruit_BME280;
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

    bool begin(
        SensorBoard& board,
        Adafruit_BME280& bme,
        ProcessControl& process) override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    class PumpStateMeasurement final : public Measurement
    {
    public:
        void begin(RelayOutput& relay);
        void update() override;
        UpdatePhase updatePhase() const override;
        uint8_t printDecimals() const override;

    private:
        RelayOutput* relay = nullptr;
    };

    // Entrées physiques
    RTDSensor collectorInput;
    RTDSensor tankTopInput;
    RTDSensor tankBottomInput;

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
    PumpStateMeasurement pumpState;
};

#endif
