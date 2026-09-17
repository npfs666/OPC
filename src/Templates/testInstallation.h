#ifndef TESTINSTALLATION_H
#define TESTINSTALLATION_H

#include "Installation.h"

#include <Hardware/Sensor.h>

#include "Measurements/Temperature/TemperatureBME.h"
#include "Measurements/Temperature/TemperatureTC.h"
#include "Measurements/Humidity/HumidityBME.h"
#include "Measurements/Pressure/PressureBMP580.h"

#include "Measurements/Resistance.h"
#include "Measurements/Temperature/TemperatureRTD.h"

#include "Measurements/Psychrometer.h"
#include "Measurements/Humidity/HumidityPsychrometer.h"

#include <Regulator/Thermostat.h>
#include <Regulator/PID.h>
#include "Regulator/SolarRegulator.h"

#include <Outputs/ActuatorOnOff.h>
#include "Outputs/TimeProportionalActuator.h"
#include <Outputs/RelayOutput.h>

class SensorBoard;
class Adafruit_BMP5xx;
class ProcessControl;

class TestInstallation : public Installation
{
public:

    //static constexpr size_t MAX_PARAMETERS = 32;

    TestInstallation();

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

    // Entrées

    Sensor input1, input2;

    // ---------- Mesures ----------

    PressureBMP580 pressureBMP580;

    TemperatureTC tcTemp;

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

    TimeProportionalActuator pump;

    // ---------- Sorties ----------

    RelayOutput relayHeater;
};

#endif
