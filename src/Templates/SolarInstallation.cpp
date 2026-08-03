#include <Templates/SolarInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>

#include <hmi/DisplayTextCodec.h>
#include <hmi/HomeScreen.h>
#include <Measurements/MeasurementSnapshot.h>

#include <cstring>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_GREEN = 0x07E0;
    constexpr uint16_t COLOR_RED = 0xF800;
    constexpr uint16_t COLOR_ORANGE = 0xFD20;

    constexpr int16_t BORDER_SIZE = 2;
    constexpr int16_t LABEL_X = 10;
    constexpr int16_t VALUE_X = 108;
    constexpr int16_t VALUE_HEIGHT = 20;

    constexpr int16_t COLLECTOR_Y = 12;
    constexpr int16_t TANK_TOP_Y = 41;
    constexpr int16_t TANK_BOTTOM_Y = 70;
    constexpr int16_t PUMP_Y = 103;

    void printTemperature(
        Adafruit_GFX& display,
        int16_t y,
        const MeasurementSample* sample)
    {
        display.fillRect(
            VALUE_X,
            y,
            display.width() -
                VALUE_X -
                BORDER_SIZE,
            VALUE_HEIGHT,
            COLOR_BLACK);

        display.setCursor(VALUE_X, y);
        display.setTextColor(
            COLOR_ORANGE,
            COLOR_BLACK);

        if (sample == nullptr ||
            !sample->valid)
        {
            display.print("--.-");
        }
        else
        {
            display.print(
                sample->value,
                sample->decimals);
        }

        display.print(' ');

        char unit[8] = {};

        DisplayTextCodec::utf8ToCp437(
            sample != nullptr
                ? sample->unit
                : "°C",
            unit,
            sizeof(unit));

        display.print(unit);
    }

    void printPumpState(
        Adafruit_GFX& display,
        bool pumpIsOn)
    {
        display.fillRect(
            BORDER_SIZE,
            PUMP_Y - 2,
            display.width() -
                (2 * BORDER_SIZE),
            VALUE_HEIGHT + 4,
            COLOR_BLACK);

        const char* text =
            pumpIsOn
                ? "POMPE : ON"
                : "POMPE : OFF";

        constexpr int16_t CHARACTER_WIDTH = 12;

        const int16_t textWidth =
            static_cast<int16_t>(
                std::strlen(text) *
                CHARACTER_WIDTH);

        display.setCursor(
            (display.width() - textWidth) / 2,
            PUMP_Y);

        display.setTextColor(
            pumpIsOn
                ? COLOR_GREEN
                : COLOR_RED,
            COLOR_BLACK);

        display.print(text);
    }
}

const char* SolarInstallation::name() const
{
    return "Regulateur solaire";
}

const char* SolarInstallation::configurationKey() const
{
    return "solar_regulator";
}

void SolarInstallation::PumpStateMeasurement::begin(
    RelayOutput& relay)
{
    Measurement::begin(
        "solar_pump_state",
        "Etat pompe",
        "");

    this->relay = &relay;
    display = false;
}

void SolarInstallation::PumpStateMeasurement::update()
{
    if (relay == nullptr ||
        !relay->isHealthy())
    {
        setValid(false);
        return;
    }

    setValue(
        relay->appliedCommand() >= 0.5
            ? 1.0
            : 0.0);

    setValid(true);
}

Measurement::UpdatePhase SolarInstallation::
    PumpStateMeasurement::updatePhase() const
{
    return UpdatePhase::AfterOutputs;
}

uint8_t SolarInstallation::
    PumpStateMeasurement::printDecimals() const
{
    return 0;
}

bool SolarInstallation::begin(
    SensorBoard& board,
    Adafruit_BME280& bme,
    ProcessControl& process)
{
    (void)bme;

    collectorInput.begin(
        "solar_collector_input",
        "Capteur solaire",
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::FourWire,
        16,
        0.0f);

    tankTopInput.begin(
        "tank_top_input",
        "Haut ballon",
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::FourWire,
        16,
        0.0f);

    tankBottomInput.begin(
        "tank_bottom_input",
        "Bas ballon",
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::ThreeWire,
        16,
        0.0f);

    if (!board.addRTD(collectorInput) ||
        !board.addRTD(tankTopInput) ||
        !board.addRTD(tankBottomInput))
    {
        return false;
    }

    collectorResistance.begin(
        "Resistance capteur",
        board,
        collectorInput);

    tankTopResistance.begin(
        "Resistance haut ballon",
        board,
        tankTopInput);

    tankBottomResistance.begin(
        "Resistance bas ballon",
        board,
        tankBottomInput);

    collectorTemperature.begin(
        "Capteur solaire",
        collectorResistance);

    tankTopTemperature.begin(
        "Haut ballon",
        tankTopResistance);

    tankBottomTemperature.begin(
        "Bas ballon",
        tankBottomResistance);

    if (!process.add(collectorResistance) ||
        !process.add(collectorTemperature) ||
        !process.add(tankTopResistance) ||
        !process.add(tankTopTemperature) ||
        !process.add(tankBottomResistance) ||
        !process.add(tankBottomTemperature))
    {
        return false;
    }

    solarRegulator.begin(
        "solar_regulator",
        "Regulateur solaire",
        collectorTemperature,
        tankTopTemperature,
        tankBottomTemperature);

    if (!process.add(solarRegulator))
        return false;

    pump.begin(
        "solar_pump",
        "Pompe solaire",
        solarRegulator);

    if (!process.add(pump))
        return false;

    pumpRelay.begin(
        "solar_pump_relay",
        "Relais pompe",
        RELAIS_1,
        true,
        false);

    if (!process.connect(
            pump,
            pumpRelay))
    {
        return false;
    }

    pumpState.begin(pumpRelay);

    if (!process.add(pumpState))
        return false;

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
    {
        Serial.println(
            "Solar installation parameter registration failed");
        return false;
    }

    return true;
}

void SolarInstallation::printHomeScreen(
    HomeScreenContext& context)
{
    Adafruit_GFX& display = context.display;

    display.cp437(true);
    display.setTextWrap(false);
    display.setTextSize(2);

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

        display.setTextColor(
            COLOR_WHITE,
            COLOR_BLACK);

        display.setCursor(
            LABEL_X,
            COLLECTOR_Y);
        display.print("T cap. :");

        display.setCursor(
            LABEL_X,
            TANK_TOP_Y);
        display.print("T haut :");

        display.setCursor(
            LABEL_X,
            TANK_BOTTOM_Y);
        display.print("T bas  :");
    }

    printTemperature(
        display,
        COLLECTOR_Y,
        context.measurements.find(
            collectorTemperature));

    printTemperature(
        display,
        TANK_TOP_Y,
        context.measurements.find(
            tankTopTemperature));

    printTemperature(
        display,
        TANK_BOTTOM_Y,
        context.measurements.find(
            tankBottomTemperature));

    const MeasurementSample* pumpSample =
        context.measurements.find(
            pumpState);

    const bool pumpIsOn =
        pumpSample != nullptr &&
        pumpSample->valid &&
        pumpSample->value >= 0.5;

    printPumpState(
        display,
        pumpIsOn);
}
