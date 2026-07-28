#ifndef TESTINSTALLATION_H
#define TESTINSTALLATION_H

#include "Installation.h"

#include <Hardware/RTDSensor.h>

#include "Measurements/Temperature/TemperatureBME.h"
#include "Measurements/Humidity/HumidityBME.h"
#include "Measurements/Pressure/PressureBME.h"

#include "Measurements/Resistance.h"
#include "Measurements/Temperature/TemperatureRTD.h"

#include "Measurements/Psychrometer.h"
#include "Measurements/Humidity/HumidityPsychrometer.h"

#include <Regulator/Thermostat.h>
#include <Regulator/PID.h>
#include "Regulator/SolarRegulator.h"

#include <Outputs/ActuatorOnOff.h>

class SensorBoard;
class Adafruit_BME280;
class ProcessControl;
class Storage;

class TemperatureBME;
class HumidityBME;
class PressureBME;

class Resistance;
class TemperatureRTD;

class Psychrometer;
class HumidityPsychrometer;

class TestInstallation : public Installation
{
public:

    //static constexpr size_t MAX_PARAMETERS = 32;

    TestInstallation();

    const char* name() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BME280& bme,
        ProcessControl& process) override;

    void printHomeScreen(
        HomeScreenContext& context) override;

    void load(Storage& storage) override;

    void save(Storage& storage) override;

    void factoryReset() override;


private:

    // Entrées

    RTDSensor input1, input2;

    // ---------- Mesures ----------

    TemperatureBME tempBME;
    HumidityBME humidityBME;
    PressureBME pressureBME;

    Resistance rtd1Resistance;
    TemperatureRTD rtd1Temperature;

    Resistance rtd2Resistance;
    TemperatureRTD rtd2Temperature;

    Psychrometer psychrometer;
    HumidityPsychrometer psychroHumidity;

    // ---------- Régulateurs ----------

    Thermostat thermostat;

    PID pid;

    SolarRegulator solar;

    // ---------- Actionneurs ----------

    ActuatorOnOff heater;

    //TimeProportionalActuator pump;

    // ---------- Sorties ----------

    //RelayOutput relayPump;
};

#endif
