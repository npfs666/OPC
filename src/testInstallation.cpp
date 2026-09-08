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

bool TestInstallation::requiresBMP580() const
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
    Adafruit_BMP5xx& bmp580,
    ProcessControl& controller)
{
    // ----- Configuration du matériel -----
    input1.begin("input1", "Input 1", Sensor::Type::Tc, Sensor::Wiring::FourWire, 16, 0);

    if (!board.addSensor(input1))
        return false;

    input2.begin("input2", "Input 2", Sensor::Type::Pt100, Sensor::Wiring::FourWire, 16, 0);

    if (!board.addSensor(input2))
        return false;

    // ----- Construction des objets -----
    pressureBMP580.begin("BMP580", bmp580);

    //rtd1Resistance.begin("RTD1", board, input1);
    //rtd1Temperature.begin("TempRTD1", rtd1Resistance);
    tcTemp.begin("TempTC", input1);

    rtd2Resistance.begin("RTD2", board, input2);
    rtd2Temperature.begin("TempRTD2", rtd2Resistance);

    psychrometer.begin(tcTemp, rtd2Temperature, pressureBMP580);
    psychroHumidity.begin("RH psychrom",psychrometer);


    // ----- Enregistrement dans le framework -----

    if (!controller.add(pressureBMP580) ||
        //!controller.add(rtd1Resistance) ||
        !controller.add(tcTemp) ||
        !controller.add(rtd2Resistance) ||
        !controller.add(rtd2Temperature) ||
        !controller.add(psychroHumidity))
    {
        return false;
    }

    /*thermostat.begin("thermostat", "Thermostats", rtd2Temperature);
    thermostat.settings.setpoint = 25;


    if (!controller.add(thermostat))
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
        Board::Rp2040::OUTPUT_1,
        true,
        false);

    if (!controller.connect(
            heater,
            relayHeater))
    {
        return false;
    }*/

    board.registerParameters(parameterList);
    controller.registerParameters(parameterList);

    /*if (!thermostat.setpointRamp.registerParameters(
            parameterList,
            "thermostat.ramp",
            "Rampe thermostat",
            "°C/min"))
    {
        return false;
    }*/

    if (parameterList.hasError())
    {
        Serial.println(
            "Parameter registration failed");
        return false;
    }

    return true;
}
