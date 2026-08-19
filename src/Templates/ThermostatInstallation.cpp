#include <Templates/ThermostatInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>
#include <PrintSize.h>

#include <hmi/DisplayTextCodec.h>
#include <hmi/HomeScreen.h>
#include <ProcessSnapshot.h>

#include <cmath>
#include <cstring>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_GREEN = 0x07E0;
    constexpr uint16_t COLOR_RED = 0xF800;
    constexpr uint16_t COLOR_ORANGE = 0xFD20;

    constexpr int16_t BORDER_SIZE = 2;
    constexpr int16_t ACTUAL_TEMPERATURE_Y = 15;
    constexpr int16_t SETPOINT_Y = 58;
    constexpr int16_t OUTPUT_Y = 103;

    constexpr const char* RAMP_OWNER_KEY =
        "thermostat.ramp";

    constexpr const char* RAMP_OWNER_NAME =
        "Rampe thermostat";

    void printCenteredText(
        Adafruit_GFX& display,
        int16_t y,
        uint8_t textSize,
        uint16_t color,
        const char* text)
    {
        display.setTextSize(textSize);

        int16_t boundsX = 0;
        int16_t boundsY = 0;
        uint16_t boundsWidth = 0;
        uint16_t boundsHeight = 0;

        display.getTextBounds(
            text,
            0,
            y,
            &boundsX,
            &boundsY,
            &boundsWidth,
            &boundsHeight);

        const int16_t x =
            static_cast<int16_t>(
                (display.width() -
                 boundsWidth) /
                2);

        display.setCursor(x, y);
        display.setTextColor(
            color,
            COLOR_BLACK);
        display.print(text);
    }

    void printCenteredTemperature(
        Adafruit_GFX& display,
        int16_t y,
        uint8_t textSize,
        uint16_t color,
        double_t value,
        uint8_t decimals,
        bool valid)
    {
        char unit[8] = {};

        DisplayTextCodec::utf8ToCp437(
            "°C",
            unit,
            sizeof(unit));

        const bool valueIsValid =
            valid &&
            std::isfinite(value);

        PrintSize printSize;

        const size_t valueLength =
            valueIsValid
                ? printSize.print(value, decimals)
                : std::strlen("--.-");

        const size_t textLength =
            valueLength +
            1 +
            std::strlen(unit);

        constexpr int16_t CHARACTER_WIDTH = 6;

        const int16_t textWidth =
            static_cast<int16_t>(
                textLength *
                CHARACTER_WIDTH *
                textSize);

        const int16_t x =
            textWidth < display.width()
                ? (display.width() - textWidth) / 2
                : 0;

        display.setCursor(x, y);
        display.setTextSize(textSize);
        display.setTextColor(
            color,
            COLOR_BLACK);

        if (valueIsValid)
            display.print(value, decimals);
        else
            display.print("--.-");

        display.print(' ');
        display.print(unit);
    }

    void printActualTemperature(
        Adafruit_GFX& display,
        const MeasurementSample* sample)
    {
        display.fillRect(
            BORDER_SIZE,
            ACTUAL_TEMPERATURE_Y - 2,
            display.width() -
                2 * BORDER_SIZE,
            30,
            COLOR_BLACK);

        printCenteredTemperature(
            display,
            ACTUAL_TEMPERATURE_Y,
            3,
            COLOR_RED,
            sample != nullptr
                ? sample->value
                : 0.0,
            sample != nullptr
                ? sample->decimals
                : 1,
            sample != nullptr &&
                sample->valid);
    }

    void printSetpoint(
        Adafruit_GFX& display,
        double_t setpoint)
    {
        display.fillRect(
            BORDER_SIZE,
            SETPOINT_Y - 2,
            display.width() -
                2 * BORDER_SIZE,
            30,
            COLOR_BLACK);

        printCenteredTemperature(
            display,
            SETPOINT_Y,
            3,
            COLOR_GREEN,
            setpoint,
            1,
            std::isfinite(setpoint));
    }

    void printOutputState(
        Adafruit_GFX& display,
        bool outputIsOn)
    {
        display.fillRect(
            BORDER_SIZE,
            OUTPUT_Y - 2,
            display.width() -
                2 * BORDER_SIZE,
            22,
            COLOR_BLACK);

        if (!outputIsOn)
            return;

        printCenteredText(
            display,
            OUTPUT_Y,
            2,
            COLOR_ORANGE,
            "OUTPUT 1");
    }
}

const char* ThermostatInstallation::name() const
{
    return "Thermostat";
}

const char* ThermostatInstallation::configurationKey() const
{
    return "thermostat";
}

bool ThermostatInstallation::begin(
    SensorBoard& board,
    Adafruit_BME280& bme,
    ProcessControl& process)
{
    (void)bme;

    temperatureInput.begin(
        "thermostat_input",
        "Sonde thermostat",
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::FourWire,
        16,
        0.0f);

    if (!board.addRTD(
            temperatureInput))
    {
        return false;
    }

    temperatureResistance.begin(
        "Resistance thermostat",
        board,
        temperatureInput);

    temperature.begin(
        "Temperature thermostat",
        temperatureResistance);

    if (!process.add(
            temperatureResistance) ||
        !process.add(
            temperature))
    {
        return false;
    }

    thermostat.begin(
        "thermostat",
        "Thermostat",
        temperature);

    if (!process.add(thermostat))
        return false;

    relayActuator.begin(
        "thermostat_relay_actuator",
        "Commande thermostat",
        thermostat);

    if (!process.add(relayActuator))
        return false;

    relayOutput.begin(
        "thermostat_relay",
        "Output 1",
        RELAIS_1,
        true,
        false);

    if (!process.connect(
            relayActuator,
            relayOutput))
    {
        return false;
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (!thermostat.setpointRamp.registerParameters(
            parameterList,
            RAMP_OWNER_KEY,
            RAMP_OWNER_NAME,
            "°C/min"))
    {
        return false;
    }

    if (parameterList.hasError())
    {
        Serial.println(
            "Thermostat installation parameter registration failed");
        return false;
    }

    return true;
}

void ThermostatInstallation::printHomeScreen(
    HomeScreenContext& context)
{
    Adafruit_GFX& display = context.display;

    display.cp437(true);
    display.setTextWrap(false);

    if (context.fullRefresh)
    {
        display.fillScreen(COLOR_BLACK);

        display.drawRect(
            0,
            0,
            display.width(),
            display.height(),
            COLOR_WHITE);

        display.drawRect(
            1,
            1,
            display.width() - 2,
            display.height() - 2,
            COLOR_WHITE);
    }

    printActualTemperature(
        display,
        context.snapshot.find(
            temperature));

    printSetpoint(
        display,
        thermostat.setpointRamp.hasActiveSetpoint()
            ? thermostat.setpointRamp.activeSetpoint()
            : thermostat.settings.setpoint);

    const OutputSample* relaySample =
        context.snapshot.find(
            relayOutput);

    const bool outputIsOn =
        relaySample != nullptr &&
        relaySample->healthy &&
        relaySample->appliedCommand >= 0.5;

    printOutputState(
        display,
        outputIsOn);
}
