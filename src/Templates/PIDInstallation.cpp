#include <Templates/PIDInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include <ProcessSnapshot.h>
#include <hmi/DisplayTextCodec.h>
#include <hmi/HomeScreen.h>

#include <cmath>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
    constexpr uint16_t COLOR_GREEN = 0x07E0;
    constexpr uint16_t COLOR_ORANGE = 0xFD20;

    constexpr int16_t LABEL_X = 10;
    constexpr int16_t VALUE_X = 100;
    constexpr int16_t VALUE_WIDTH = 130;

    constexpr int16_t STATUS_X = 105;
    constexpr int16_t STATUS_Y = 8;
    constexpr int16_t STATUS_WIDTH = 125;
    constexpr int16_t STATUS_HEIGHT = 16;

    constexpr int16_t TEMPERATURE_Y = 39;
    constexpr int16_t SETPOINT_Y = 70;
    constexpr int16_t OUTPUT_Y = 101;

    constexpr const char* AUTOTUNE_OWNER_KEY =
        "tune_pid.autotune";

    constexpr const char* AUTOTUNE_OWNER_NAME =
        "PID autotune";

    void prepareValue(
        Adafruit_GFX& display,
        int16_t y,
        uint16_t color)
    {
        display.fillRect(
            VALUE_X,
            y,
            VALUE_WIDTH,
            20,
            COLOR_BLACK);

        display.setCursor(VALUE_X, y);
        display.setTextColor(
            color,
            COLOR_BLACK);
    }

    void printUnit(
        Adafruit_GFX& display,
        const char* unit)
    {
        char encodedUnit[8] = {};

        DisplayTextCodec::utf8ToCp437(
            unit,
            encodedUnit,
            sizeof(encodedUnit));

        display.print(' ');
        display.print(encodedUnit);
    }
}

const char* PIDInstallation::name() const
{
    return "Installation PID";
}

const char*
PIDInstallation::configurationKey() const
{
    /* Identifiant historique conservé pour relire les configurations. */
    return "pid_autotune_test";
}

bool PIDInstallation::begin(
    SensorBoard& board,
    Adafruit_BME280& bme,
    ProcessControl& process)
{
    (void)bme;

    temperatureInput.begin(
        "pid_tune_input",
        "Sonde PID",
        RTDSensor::RTDType::Pt100,
        RTDSensor::RTDWiring::FourWire,
        16,
        0.0f);

    if (!board.addRTD(temperatureInput))
        return false;

    temperatureResistance.begin(
        "Resistance PID",
        board,
        temperatureInput);

    temperature.begin(
        "Temperature PID",
        temperatureResistance);

    if (!process.add(
            temperatureResistance) ||
        !process.add(temperature))
    {
        return false;
    }

    pid.begin(
        "tune_pid",
        "PID",
        temperature);

    pid.settings.mode = PID::Mode::Heating;
    pid.settings.setpoint = 35.0;

    pid.autoTuneSettings.outputLow = 0.0;
    pid.autoTuneSettings.outputHigh = 1.0;
    pid.autoTuneSettings.noiseBand = 0.5;
    pid.autoTuneSettings.inputMin = 0.0;
    pid.autoTuneSettings.inputMax = 60.0;
    pid.autoTuneSettings.timeoutSeconds = 7200;
    pid.autoTuneSettings.minimumCycleSeconds = 30;
    pid.autoTuneSettings.stabilityTolerance = 0.20;
    pid.autoTuneSettings.cycles = 3;

    // Aucune commande avant une demande explicite de l'utilisateur.
    pid.stop();

    if (!process.add(pid))
        return false;

    actuator.begin(
        /* Clé historique : ne pas la renommer sans migration. */
        "pid_tune_heater",
        "Actionneur PID",
        pid,
        10000);

    if (!process.add(actuator))
        return false;

    controlRelay.begin(
        "pid_tune_relay",
        "Relais PID",
        RELAIS_1,
        true,
        false);

    /* L'état sûr logique de l'équipement commandé est toujours OFF. */
    controlRelay.lockSafeState(false);

    if (!process.connect(
            actuator,
            controlRelay))
    {
        return false;
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (!pid.registerAutoTuneParameters(
            parameterList,
            AUTOTUNE_OWNER_KEY,
            AUTOTUNE_OWNER_NAME))
    {
        return false;
    }

    if (parameterList.hasError())
    {
        Serial.println(
            "PID autotune installation parameter registration failed");
        return false;
    }

    return true;
}

void PIDInstallation::onMenuOpened()
{
    /* Ouvrir le menu interrompt toujours un essai en cours. */
    pid.cancelAutoTune();
}

bool PIDInstallation::addMenuActions(
    MenuBuilder& menu) const
{
    const MenuBuilder::GroupId group =
        menu.findGroupForOwner(
            AUTOTUNE_OWNER_KEY);

    return
        group != MenuBuilder::INVALID_GROUP &&
        menu.addAction(
            group,
            START_AUTOTUNE_ACTION,
            "pid_autotune_start",
            "Lancer autotune");
}

bool PIDInstallation::executeMenuAction(
    MenuBuilder::ActionId actionId)
{
    if (actionId == START_AUTOTUNE_ACTION)
        return pid.startAutoTune(millis());

    return false;
}

void PIDInstallation::onMenuActionSaveFailed(
    MenuBuilder::ActionId actionId)
{
    if (actionId == START_AUTOTUNE_ACTION)
        pid.cancelAutoTune();
}

bool PIDInstallation::takeConfigurationSaveRequest()
{
    return pid.takeAutoTuneTuningsApplied();
}

void PIDInstallation::captureHomeScreenState()
{
    homeState.mode = pid.settings.mode;
    homeState.setpoint = pid.settings.setpoint;
    homeState.pidEnabled = pid.settings.enabled;

    homeState.autoTuneStatus =
        pid.getAutoTuneStatus();

    homeState.autoTuneActive =
        pid.isAutoTuneActive();

    homeState.completedCycles =
        pid.getAutoTuneCompletedCycles();

    homeState.requestedCycles =
        pid.autoTuneSettings.cycles;
}

void PIDInstallation::printHomeScreen(
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

        display.setCursor(10, 8);
        display.setTextColor(
            COLOR_CYAN,
            COLOR_BLACK);
        display.print("PID");

        display.setTextSize(1);
        display.setCursor(52, 12);

        const bool heatingMode =
            homeState.mode ==
                PID::Mode::Heating;

        const bool coolingMode =
            homeState.mode ==
                PID::Mode::Cooling;

        display.setTextColor(
            heatingMode
                ? COLOR_ORANGE
                : coolingMode
                    ? COLOR_CYAN
                    : COLOR_WHITE,
            COLOR_BLACK);

        display.print(
            heatingMode
                ? "CHAUD"
                : coolingMode
                    ? "FROID"
                    : "ERR");

        display.setTextSize(2);

        display.setTextColor(
            COLOR_WHITE,
            COLOR_BLACK);

        display.setCursor(
            LABEL_X,
            TEMPERATURE_Y);
        display.print("Temp :");

        display.setCursor(
            LABEL_X,
            SETPOINT_Y);
        display.print("SP   :");

        display.setCursor(
            LABEL_X,
            OUTPUT_Y);
        display.print("Relais:");
    }

    display.fillRect(
        STATUS_X,
        STATUS_Y,
        STATUS_WIDTH,
        STATUS_HEIGHT,
        COLOR_BLACK);

    display.setTextSize(1);
    display.setCursor(
        STATUS_X,
        STATUS_Y + 4);

    const PID::AutoTuneStatus tuneStatus =
        homeState.autoTuneStatus;

    if (homeState.autoTuneActive)
    {
        display.setTextColor(
            COLOR_ORANGE,
            COLOR_BLACK);

        if (tuneStatus ==
            PID::AutoTuneStatus::WaitingForMeasurement)
        {
            display.print("TUNE ATTENTE");
        }
        else
        {
            display.print("TUNE ");
            display.print(
                homeState.completedCycles);
            display.print('/');
            display.print(
                homeState.requestedCycles);
        }
    }
    else if (homeState.pidEnabled)
    {
        display.setTextColor(
            COLOR_GREEN,
            COLOR_BLACK);
        display.print("PID ACTIF");
    }
    else if (tuneStatus ==
             PID::AutoTuneStatus::Succeeded)
    {
        display.setTextColor(
            COLOR_GREEN,
            COLOR_BLACK);
        display.print("TUNE OK");
    }
    else if (tuneStatus ==
             PID::AutoTuneStatus::Failed)
    {
        display.setTextColor(
            COLOR_ORANGE,
            COLOR_BLACK);
        display.print("TUNE ERREUR");
    }
    else if (tuneStatus ==
             PID::AutoTuneStatus::Cancelled)
    {
        display.setTextColor(
            COLOR_WHITE,
            COLOR_BLACK);
        display.print("TUNE ANNULE");
    }
    else
    {
        display.setTextColor(
            COLOR_WHITE,
            COLOR_BLACK);
        display.print("PID ARRET");
    }

    display.setTextSize(2);

    const MeasurementSample* temperatureSample =
        context.snapshot.find(temperature);

    prepareValue(
        display,
        TEMPERATURE_Y,
        COLOR_ORANGE);

    if (temperatureSample == nullptr ||
        !temperatureSample->valid ||
        !std::isfinite(
            temperatureSample->value))
    {
        display.print("--.-");
        printUnit(display, "°C");
    }
    else
    {
        display.print(
            temperatureSample->value,
            temperatureSample->decimals);
        printUnit(
            display,
            temperatureSample->unit);
    }

    prepareValue(
        display,
        SETPOINT_Y,
        COLOR_GREEN);

    display.print(
        homeState.setpoint,
        1);
    printUnit(display, "°C");

    const OutputSample* relaySample =
        context.snapshot.find(
            controlRelay);

    const bool relayIsOn =
        relaySample != nullptr &&
        relaySample->healthy &&
        relaySample->appliedCommand >= 0.5;

    prepareValue(
        display,
        OUTPUT_Y,
        relayIsOn
            ? COLOR_GREEN
            : COLOR_WHITE);

    display.print(
        relayIsOn
            ? "ON"
            : "OFF");
}
