#include "testInstallation.h"

#include "Hardware/SensorBoard.h"
#include "ProcessControl.h"

#include <Adafruit_GFX.h>

#include <hmi/DisplayTextCodec.h>
#include <hmi/HomeScreen.h>
#include <ProcessSnapshot.h>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
    constexpr uint16_t COLOR_GREY = 0x8410;

    void printHomeValue(
        Adafruit_GFX& display,
        int16_t y,
        const MeasurementSample* sample)
    {
        display.fillRect(
            55,
            y,
            display.width() - 55,
            20,
            COLOR_BLACK);

        display.setCursor(60, y);
        display.setTextColor(
            COLOR_WHITE,
            COLOR_BLACK);

        if (sample == nullptr ||
            !sample->valid)
        {
            display.print("--.-");
            return;
        }

        display.print(
            sample->value,
            sample->decimals);
        display.print(' ');

        char unit[12] = {};

        DisplayTextCodec::utf8ToCp437(
            sample->unit,
            unit,
            sizeof(unit));

        display.print(unit);
    }
}

TestInstallation::TestInstallation()
{
}

const char* TestInstallation::name() const
{
    return "Installation de test pour le développement";
}

const char* TestInstallation::configurationKey() const
{
    return "test_installation";
}

bool TestInstallation::requiresBME280() const
{
    return true;
}

void TestInstallation::printHomeScreen(
    HomeScreenContext& context)
{
    Adafruit_GFX& display = context.display;

    display.cp437(true);
    display.setTextWrap(false);

    if (context.fullRefresh)
    {
        display.fillScreen(COLOR_BLACK);

        display.setTextSize(2);
        display.setTextColor(
            COLOR_CYAN,
            COLOR_BLACK);
        display.setCursor(8, 5);
        display.print("OPC - Accueil");

        display.drawFastHLine(
            0,
            25,
            display.width(),
            COLOR_GREY);

        display.setTextColor(
            COLOR_WHITE,
            COLOR_BLACK);

        display.setCursor(8, 38);
        display.print("T1");

        display.setCursor(8, 70);
        display.print("T2");

        display.setCursor(8, 102);
        display.print("HR");
    }

    display.setTextSize(2);

    printHomeValue(
        display,
        38,
        context.snapshot.find(
            rtd1Temperature));

    printHomeValue(
        display,
        70,
        context.snapshot.find(
            rtd2Temperature));

    printHomeValue(
        display,
        102,
        context.snapshot.find(
            psychroHumidity));
}



bool TestInstallation::begin(
    SensorBoard& board,
    Adafruit_BME280& bme,
    ProcessControl& controller)
{
    // ----- Configuration du matériel -----
    input1.begin("input1", "Input 1", RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 16, 0);

    if (!board.addRTD(input1))
        return false;

    input2.begin("input2", "Input 2", RTDSensor::RTDType::Pt100, RTDSensor::RTDWiring::FourWire, 16, 0);

    if (!board.addRTD(input2))
        return false;

    // ----- Construction des objets -----
    tempBME.begin("BME", bme);
    humidityBME.begin("BME", bme);
    pressureBME.begin("BME", bme);

    rtd1Resistance.begin("RTD1", board, input1);
    rtd1Temperature.begin("TempRTD1", rtd1Resistance);

    rtd2Resistance.begin("RTD2", board, input2);
    rtd2Temperature.begin("TempRTD2", rtd2Resistance);

    psychrometer.begin(rtd1Temperature, rtd2Temperature, pressureBME);
    psychroHumidity.begin("RH psychrom",psychrometer);


    // ----- Enregistrement dans le framework -----

    if (!controller.add(tempBME) ||
        !controller.add(humidityBME) ||
        !controller.add(pressureBME) ||
        !controller.add(rtd1Resistance) ||
        !controller.add(rtd1Temperature) ||
        !controller.add(rtd2Resistance) ||
        !controller.add(rtd2Temperature) ||
        !controller.add(psychroHumidity))
    {
        return false;
    }

    thermostat.begin("thermostat", "Thermostats", rtd2Temperature);
    thermostat.settings.setpoint = 25;

    pid.begin("pid", "PID", rtd1Temperature);

    solar.begin("Reg Solaire", tempBME, rtd1Temperature, rtd2Temperature);

    if (!controller.add(thermostat) ||
        !controller.add(pid) ||
        !controller.add(solar))
    {
        return false;
    }

    heater.begin("heater", thermostat);

    pump.begin("pompe", thermostat, 10000);

    if (!controller.add(heater) ||
        !controller.add(pump))
    {
        return false;
    }

    relayHeater.begin(
        "relay_heater",
        "Relais chauffage",
        RELAIS_1,
        true,
        false);

    if (!controller.connect(
            heater,
            relayHeater))
    {
        return false;
    }

    board.registerParameters(parameterList);
    controller.registerParameters(parameterList);

    if (parameterList.hasError())
    {
        Serial.println(
            "Parameter registration failed");
        return false;
    }

    return true;
}
