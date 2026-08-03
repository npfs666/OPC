#include "MinimalInstallation.h"

#include <Hardware/SensorBoard.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>

#include <Measurements/MeasurementSnapshot.h>
#include <hmi/DisplayTextCodec.h>
#include <hmi/HomeScreen.h>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
}

const char* MinimalInstallation::name() const
{
    return "Installation minimale";
}

const char* MinimalInstallation::configurationKey() const
{
    return "minimal_installation";
}

bool MinimalInstallation::begin(
    SensorBoard& board,
    Adafruit_BME280& bme,
    ProcessControl& process)
{
    (void)bme;

    temperatureInput.begin(
        "minimal_temperature_input",
        "Sonde PT100",
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::FourWire,
        16,
        0.0f);

    if (!board.addRTD(temperatureInput))
        return false;

    temperatureResistance.begin(
        "Resistance PT100",
        board,
        temperatureInput);

    temperature.begin(
        "Temperature PT100",
        temperatureResistance);

    if (!process.add(temperatureResistance) ||
        !process.add(temperature))
    {
        return false;
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    return !parameterList.hasError();
}

void MinimalInstallation::printHomeScreen(
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
        display.setCursor(8, 8);
        display.print("Temperature");
    }

    display.fillRect(
        8,
        48,
        display.width() - 16,
        32,
        COLOR_BLACK);

    display.setCursor(8, 48);
    display.setTextSize(3);
    display.setTextColor(
        COLOR_WHITE,
        COLOR_BLACK);

    const MeasurementSample* sample =
        context.measurements.find(temperature);

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

    char unit[8] = {};

    DisplayTextCodec::utf8ToCp437(
        sample->unit,
        unit,
        sizeof(unit));

    display.print(unit);
}
