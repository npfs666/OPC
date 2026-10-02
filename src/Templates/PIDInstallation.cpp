#include <Templates/PIDInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>
#include <Arduino.h>

#include <ProcessSnapshot.h>
#include <hmi/HomeScreen.h>
#include <hmi/MeasurementDisplay.h>
#include <hmi/TextField.h>

#include <cmath>
#include <cstdio>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_GREY = 0x8410;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
    constexpr uint16_t COLOR_GREEN = 0x07E0;
    constexpr uint16_t COLOR_ORANGE = 0xFD20;

    using TextField::Align;

    // Écran d'accueil 240 × 240, champs à largeur fixe (voir TextField).
    constexpr int16_t MARGIN = 8;
    constexpr int16_t RIGHT = 232;          // bord droit des valeurs

    constexpr int16_t HEADER_Y = 8;         // taille 2
    constexpr int16_t SEPARATOR_Y = 30;
    constexpr int16_t MEASUREMENT_Y = 44;   // taille 4
    constexpr int16_t SETPOINT_Y = 92;      // taille 2
    constexpr int16_t OUTPUT_Y = 120;       // taille 2
    constexpr int16_t BAR_Y = 144;
    constexpr int16_t BAR_HEIGHT = 14;
    constexpr int16_t RELAY_Y = 172;        // taille 2
    constexpr int16_t GAINS_Y = 216;        // taille 1

    constexpr size_t MEASUREMENT_CHARS = 9; // 9 × 24 px = 216 px
    constexpr size_t LABEL_CHARS = 8;       // "Consigne"
    constexpr size_t VALUE_CHARS = 9;       // "-123.4 °C"
    constexpr size_t STATUS_CHARS = 12;     // "TUNE ATTENTE"
    constexpr size_t GAINS_CHARS = 38;

    constexpr const char* DEGREES_C = "\xF8" "C"; // "°C" en CP437

    constexpr const char* AUTOTUNE_OWNER_KEY =
        "tune_pid.autotune";

    constexpr const char* AUTOTUNE_OWNER_NAME =
        "PID autotune";

    constexpr const char* RAMP_OWNER_KEY =
        "tune_pid.ramp";

    constexpr const char* RAMP_OWNER_NAME =
        "Rampe PID";

    // Champ de valeur de taille 2 aligné sur le bord droit.
    void printValue(
        Adafruit_GFX& display,
        int16_t y,
        uint16_t color,
        const char* text)
    {
        TextField::print(
            display,
            RIGHT - TextField::pixelWidth(VALUE_CHARS, 2),
            y,
            2,
            color,
            text,
            VALUE_CHARS,
            Align::Right);
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
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    temperatureInput.begin(
        "pid_tune_input",
        "Sonde PID",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0.0f);

    if (!board.addSensor(temperatureInput))
        return fail("Sonde PID : entrée analogique indisponible");

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
        return fail("Mesures PID non enregistrées");
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
        return fail("Régulateur PID non enregistré");

    actuator.begin(
        /* Clé historique : ne pas la renommer sans migration. */
        "pid_tune_heater",
        "Actionneur PID",
        pid,
        10000,
        500);   // impulsion minimale : 5 % de la période

    if (!process.add(actuator))
        return fail("Actionneur PID non enregistré");

    controlRelay.begin(
        "pid_tune_relay",
        "Relais PID",
        Board::Rp2040::OUTPUT_1,
        true,
        false);

    /* L'état sûr logique de l'équipement commandé est toujours OFF. */
    controlRelay.lockSafeState(false);

    if (!process.connect(
            actuator,
            controlRelay))
    {
        return fail("Relais PID non relié à l'actionneur");
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (!pid.setpointRamp.registerParameters(
            parameterList,
            RAMP_OWNER_KEY,
            RAMP_OWNER_NAME,
            "°C/min") ||
        !pid.registerAutoTuneParameters(
            parameterList,
            AUTOTUNE_OWNER_KEY,
            AUTOTUNE_OWNER_NAME))
    {
        return fail("Paramètres rampe ou autotune PID");
    }

    if (parameterList.hasError())
        return fail("Paramètres PID invalides");

    return true;
}

void PIDInstallation::onParametersApplied()
{
    /* Appliquer des réglages interrompt l'essai ; la consultation le laisse tourner. */
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
    homeState.setpoint =
        pid.setpointRamp.hasActiveSetpoint()
            ? pid.setpointRamp.activeSetpoint()
            : pid.settings.setpoint;
    homeState.pidEnabled = pid.settings.enabled;

    homeState.autoTuneStatus =
        pid.getAutoTuneStatus();

    homeState.autoTuneActive =
        pid.isAutoTuneActive();

    homeState.completedCycles =
        pid.getAutoTuneCompletedCycles();

    homeState.requestedCycles =
        pid.autoTuneSettings.cycles;

    homeState.fallback = pid.isInFallback();
    homeState.output = pid.readCommand();
    homeState.outputValid = pid.isCommandValid();

    homeState.ramping =
        pid.setpointRamp.hasActiveSetpoint() &&
        std::fabs(
            pid.setpointRamp.activeSetpoint() -
            pid.settings.setpoint) > 0.05;

    homeState.kp = pid.settings.kp;
    homeState.ti = pid.settings.ti;
    homeState.td = pid.settings.td;
}

void PIDInstallation::printHomeScreen(
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
        display.drawFastHLine(
            MARGIN,
            SEPARATOR_Y,
            RIGHT - MARGIN,
            COLOR_GREY);
        display.drawRect(
            MARGIN,
            BAR_Y,
            RIGHT - MARGIN,
            BAR_HEIGHT,
            COLOR_GREY);

        TextField::print(display, MARGIN, OUTPUT_Y, 2, COLOR_WHITE, "Sortie", LABEL_CHARS);
        TextField::print(display, MARGIN, RELAY_Y, 2, COLOR_WHITE, "Relais", LABEL_CHARS);
    }

    const bool cooling =
        homeState.mode == PID::Mode::Cooling;

    const uint16_t modeColor =
        cooling ? COLOR_CYAN : COLOR_ORANGE;

    char text[48];

    // ----- En-tête : sens d'action et état de la régulation -----

    TextField::print(
        display,
        MARGIN,
        HEADER_Y,
        2,
        modeColor,
        cooling ? "FROID" : "CHAUD",
        5);

    const char* status = "ARRET";
    uint16_t statusColor = COLOR_WHITE;

    if (homeState.autoTuneActive)
    {
        statusColor = COLOR_ORANGE;

        if (homeState.autoTuneStatus ==
            PID::AutoTuneStatus::WaitingForMeasurement)
        {
            status = "TUNE ATTENTE";
        }
        else
        {
            snprintf(
                text,
                sizeof(text),
                "TUNE %u/%u",
                homeState.completedCycles,
                homeState.requestedCycles);
            status = text;
        }
    }
    else if (homeState.fallback)
    {
        status = "REPLI";
        statusColor = COLOR_ORANGE;
    }
    else if (homeState.pidEnabled)
    {
        status = "ACTIF";
        statusColor = COLOR_GREEN;
    }
    else if (homeState.autoTuneStatus ==
             PID::AutoTuneStatus::Succeeded)
    {
        status = "TUNE OK";
        statusColor = COLOR_GREEN;
    }
    else if (homeState.autoTuneStatus ==
             PID::AutoTuneStatus::Failed)
    {
        status = "TUNE ERREUR";
        statusColor = COLOR_ORANGE;
    }
    else if (homeState.autoTuneStatus ==
             PID::AutoTuneStatus::Cancelled)
    {
        status = "TUNE ANNULE";
    }

    TextField::print(
        display,
        RIGHT - TextField::pixelWidth(STATUS_CHARS, 2),
        HEADER_Y,
        2,
        statusColor,
        status,
        STATUS_CHARS,
        Align::Right);

    // ----- Mesure : valeur ou état du capteur, en grand -----

    const MeasurementSample* sample =
        context.snapshot.find(temperature);

    size_t length = MeasurementDisplay::format(
        sample,
        text,
        sizeof(text),
        MEASUREMENT_CHARS);

    // Les décimales sont réduites seulement si la valeur ne tient pas.
    if (sample != nullptr && length > MEASUREMENT_CHARS)
    {
        MeasurementSample shorter = *sample;

        while (length > MEASUREMENT_CHARS &&
               shorter.decimals > 0)
        {
            shorter.decimals--;
            length = MeasurementDisplay::format(
                &shorter,
                text,
                sizeof(text));
        }
    }

    TextField::print(
        display,
        (display.width() -
            TextField::pixelWidth(MEASUREMENT_CHARS, 4)) / 2,
        MEASUREMENT_Y,
        4,
        MeasurementDisplay::color(sample, COLOR_WHITE),
        text,
        MEASUREMENT_CHARS,
        Align::Center);

    // ----- Consigne active ("Rampe" pendant une rampe) -----

    TextField::print(
        display,
        MARGIN,
        SETPOINT_Y,
        2,
        COLOR_WHITE,
        homeState.ramping ? "Rampe" : "Consigne",
        LABEL_CHARS);

    if (std::isfinite(homeState.setpoint))
    {
        snprintf(
            text,
            sizeof(text),
            "%.1f %s",
            homeState.setpoint,
            DEGREES_C);
    }
    else
    {
        snprintf(text, sizeof(text), "--");
    }

    printValue(display, SETPOINT_Y, COLOR_GREEN, text);

    // ----- Commande du PID : valeur et barre -----

    const bool outputShown =
        homeState.outputValid &&
        std::isfinite(homeState.output);

    const double_t output =
        outputShown
            ? constrain(homeState.output, 0.0, 1.0)
            : 0.0;

    if (outputShown)
        snprintf(text, sizeof(text), "%.0f %%", output * 100.0);
    else
        snprintf(text, sizeof(text), "--");

    printValue(display, OUTPUT_Y, COLOR_WHITE, text);

    const int16_t barWidth = RIGHT - MARGIN - 2;
    const int16_t filled =
        static_cast<int16_t>(std::lround(output * barWidth));

    display.fillRect(
        MARGIN + 1,
        BAR_Y + 1,
        filled,
        BAR_HEIGHT - 2,
        modeColor);
    display.fillRect(
        MARGIN + 1 + filled,
        BAR_Y + 1,
        barWidth - filled,
        BAR_HEIGHT - 2,
        COLOR_BLACK);

    // ----- Relais -----

    const OutputSample* relaySample =
        context.snapshot.find(controlRelay);

    if (relaySample == nullptr || !relaySample->healthy)
        printValue(display, RELAY_Y, COLOR_GREY, "--");
    else if (relaySample->appliedCommand >= 0.5)
        printValue(display, RELAY_Y, COLOR_GREEN, "ON");
    else if (relaySample->waitingToStart())
        printValue(display, RELAY_Y, COLOR_ORANGE, "ATTENTE");
    else
        printValue(display, RELAY_Y, COLOR_GREY, "OFF");

    // ----- Gains -----

    snprintf(
        text,
        sizeof(text),
        "Kp %.3g  Ti %.0f s  Td %.0f s",
        homeState.kp,
        homeState.ti,
        homeState.td);

    TextField::print(
        display,
        (display.width() -
            TextField::pixelWidth(GAINS_CHARS, 1)) / 2,
        GAINS_Y,
        1,
        COLOR_GREY,
        text,
        GAINS_CHARS,
        Align::Center);
}
