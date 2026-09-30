#ifndef TESTINSTALLATION_H
#define TESTINSTALLATION_H

#include <Installation.h>

#include <Hardware/Sensor.h>

#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Measurements/Temperature/TemperatureTC.h>
#include <Measurements/Pressure/PressureBMP580.h>
#include <Measurements/Psychrometer.h>
#include <Measurements/Humidity/HumidityPsychrometer.h>

class Adafruit_BMP5xx;
class ProcessControl;
class SensorBoard;

/**
 * Installation de développement : psychromètre.
 *
 * - Entrée 1 : température sèche (PT100 4 fils ou thermocouple K) ;
 * - Entrée 2 : température humide (PT100 4 fils) ;
 * - BMP580   : pression atmosphérique.
 *
 * Aucune régulation ni sortie n'est pilotée.
 */
class TestInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    bool requiresBMP580() const override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    // Température réellement mesurée sur l'entrée 1 (PT100 ou thermocouple).
    const Temperature& dryBulbTemperature() const;

    // Entrées physiques
    Sensor input1;
    Sensor input2;

    // Entrée 1 : température sèche
    Resistance rtd1Resistance;
    TemperatureRTD rtd1Temperature;
    TemperatureTC tcTemperature;

    // Entrée 2 : température humide
    Resistance rtd2Resistance;
    TemperatureRTD rtd2Temperature;

    // Pression et humidité
    PressureBMP580 pressure;
    Psychrometer psychrometer;
    HumidityPsychrometer humidity;
};

#endif
