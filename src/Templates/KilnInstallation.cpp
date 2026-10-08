// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Templates/KilnInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <hmi/HomeScreen.h>
#include <ProcessSnapshot.h>

#include <cmath>
#include <cstdio>
#include <initializer_list>

// Le dessin n'est compilé que pour la carte : les tests sur l'hôte
// vérifient la logique du template, pas l'écran.
#ifndef OPC_HOST_TEST
#include <Adafruit_GFX.h>

#include <hmi/AlarmDisplay.h>
#include <hmi/MeasurementDisplay.h>
#include <hmi/TextField.h>

namespace
{
    using State = SetpointProgram::State;

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
    constexpr int16_t MEASUREMENT_Y = 42;   // taille 4
    constexpr int16_t SETPOINT_Y = 84;      // taille 2, tous les 24 px
    constexpr int16_t TARGET_Y = 108;
    constexpr int16_t TIME_Y = 132;
    constexpr int16_t POWER_Y = 156;
    constexpr int16_t ALARM_Y = 184;        // taille 2
    constexpr int16_t FOOTER_Y = 222;       // taille 1

    constexpr size_t MEASUREMENT_CHARS = 9; // 9 × 24 px = 216 px
    constexpr size_t LABEL_CHARS = 8;       // "Consigne"
    constexpr size_t VALUE_CHARS = 9;       // "-123.4 °C"
    constexpr size_t TITLE_CHARS = 8;       // "MAINTIEN"
    constexpr size_t STEP_CHARS = 9;        // "P1 S8/8"
    constexpr size_t FOOTER_CHARS = 38;

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

    void printLabel(
        Adafruit_GFX& display,
        int16_t y,
        const char* label)
    {
        TextField::print(
            display, MARGIN, y, 2, COLOR_WHITE, label, LABEL_CHARS);
    }

    void printCelsius(
        Adafruit_GFX& display,
        int16_t y,
        uint16_t color,
        double_t value)
    {
        char text[24];
        snprintf(text, sizeof(text), "%.0f \xF8" "C", value);
        printValue(display, y, color, text);
    }

    // Durée en h:mm (arrondie à la minute supérieure).
    void printDuration(
        Adafruit_GFX& display,
        int16_t y,
        uint32_t seconds)
    {
        const uint32_t minutes = (seconds + 59) / 60;

        char text[24];
        snprintf(
            text,
            sizeof(text),
            "%lu:%02lu",
            static_cast<unsigned long>(minutes / 60),
            static_cast<unsigned long>(minutes % 60));

        printValue(display, y, COLOR_WHITE, text);
    }

    const char* title(State state)
    {
        switch (state)
        {
        case State::Delayed:
            return "DIFFERE";

        case State::Waiting:
            return "ATTENTE";

        case State::Ramp:
            return "RAMPE";

        case State::Soak:
            return "PALIER";

        case State::Hold:
            return "MAINTIEN";

        case State::Finished:
            return "TERMINE";

        default:
            return "ARRET";
        }
    }

    uint16_t titleColor(State state)
    {
        switch (state)
        {
        case State::Ramp:
        case State::Soak:
        case State::Hold:
            return COLOR_ORANGE;

        case State::Delayed:
        case State::Waiting:
            return COLOR_CYAN;

        case State::Finished:
            return COLOR_GREEN;

        default:
            return COLOR_GREY;
        }
    }
}
#endif

namespace
{
    // Programmes d'exemple : {vitesse °C/h (0 = pleine puissance),
    // cible °C, palier min}.
    void setProgram(
        SetpointProgram::Program& program,
        std::initializer_list<SetpointProgram::Segment> segments)
    {
        program.segmentCount = 0;

        for (const auto& segment : segments)
        {
            if (program.segmentCount >= SetpointProgram::MAX_SEGMENTS)
                break;

            program.segments[program.segmentCount++] = segment;
        }
    }
}

const char* KilnInstallation::name() const
{
    return "Four céramique";
}

const char* KilnInstallation::configurationKey() const
{
    return "kiln";
}

bool KilnInstallation::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    this->process = &process;

    // ----- Thermocouple -----

    kilnInput.begin(
        "kiln_input",
        "Thermocouple",
        Sensor::Type::Tc,
        Sensor::Wiring::TwoWire,
        16,
        0.0f);

    if (!board.addSensor(kilnInput))
        return fail("Thermocouple : entrée analogique indisponible");

    temperature.begin("Four", kilnInput);

    if (!process.add(temperature))
        return fail("Mesure du four non enregistrée");

    // ----- Programme : biscuit, émail faïence, émail grès -----

    program.begin("kiln_program", "Cuisson", temperature, 3);
    program.setLimits(0.0, 1300.0, 999.0);

    setProgram(program.settings.programs[0], {
        {50.0, 200.0, 0},       // séchage
        {100.0, 600.0, 0},      // quartz, déshydratation
        {150.0, 950.0, 15}
    });

    setProgram(program.settings.programs[1], {
        {100.0, 600.0, 0},
        {200.0, 1050.0, 10}
    });

    setProgram(program.settings.programs[2], {
        {100.0, 600.0, 0},
        {150.0, 1160.0, 0},
        {60.0, 1250.0, 15}
    });

    // ----- PID des résistances -----

    pid.begin("kiln_pid", "PID four", temperature);
    pid.followSetpoint(program);
    pid.setSetpointLimits(0.0, 1300.0);

    // Point de départ : bande proportionnelle de 33 °C, Ti 10 min. À affiner
    // à la mise en service.
    pid.setTunings(0.03, 600.0, 0.0);

    // Thermocouple en défaut : jamais de maintien de la puissance.
    pid.lockFaultAction(Regulator::FaultAction::SafeState);

    // Le programme avant le PID : il lit la consigne du même cycle.
    if (!process.add(program) ||
        !process.add(pid))
    {
        return fail("Programme ou PID non enregistré");
    }

    heater.begin(
        "kiln_heater",
        "Résistances",
        pid,
        10000,
        500);   // impulsion minimale : 5 % de la période

    heaterOutput.begin(
        "kiln_ssr",
        "Relais statique",
        Board::Rp2040::OUTPUT_3,
        true,
        0.0);

    heaterOutput.lockSafeCommand(0.0);

    if (!process.add(heater) ||
        !process.connect(heater, heaterOutput))
    {
        return fail("Relais statique non relié");
    }

    // ----- Alarmes (après le PID : consigne du même cycle) -----

    // Surchauffe : active, mémorisée. La glue arrête la cuisson.
    overTemperature.begin("kiln_overtemp", "Surchauffe", temperature);
    overTemperature.setLimitRange(0.0, 1400.0);
    overTemperature.settings.enabled = true;
    overTemperature.settings.type = LimitAlarm::Type::Max;
    overTemperature.settings.limit = 1300.0;
    overTemperature.settings.hysteresis = 10.0;
    overTemperature.settings.delay = 10;
    overTemperature.settings.latching = true;

    // Écart haut, mémorisée : relais statique collé. Inhibée par la glue
    // pendant une rampe descendante, où le four est normalement au-dessus
    // de la consigne.
    deviation.begin("kiln_deviation", "Écart haut", temperature);
    deviation.setReference(pid);
    deviation.allowInhibit();
    deviation.settings.enabled = true;
    deviation.settings.type = LimitAlarm::Type::DeviationHigh;
    deviation.settings.limit = 50.0;
    deviation.settings.hysteresis = 5.0;
    deviation.settings.delay = 120;
    deviation.settings.latching = true;

    // Boucle ouverte (résistance coupée, thermocouple hors du four), à
    // activer au menu : met le PID en sécurité.
    loopAlarm.begin("kiln_loop", "Boucle four", temperature, pid);

    if (!process.add(overTemperature) ||
        !process.add(deviation) ||
        !process.add(loopAlarm))
    {
        return fail("Alarmes du four non enregistrées");
    }

    // ----- Contacteur de sécurité -----

    contactorCommand.begin("kiln_contactor_cmd", "Cde contacteur");
    contactorCommand.disableManualMode();
    contactorCommand.lockFaultAction(Regulator::FaultAction::SafeState);

    contactor.begin("kiln_contactor", "Contacteur", contactorCommand);

    contactorRelay.begin(
        "kiln_contactor_relay",
        "Relais contact.",
        Board::Rp2040::OUTPUT_1,
        true,
        false);

    contactorRelay.lockSafeState(false);

    if (!contactorCommand.dependsOn(temperature) ||
        !contactorCommand.dependsOn(pid) ||
        !process.add(contactorCommand) ||
        !process.add(contactor) ||
        !process.connect(contactor, contactorRelay))
    {
        return fail("Contacteur non relié");
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres du four invalides");

    return true;
}

void KilnInstallation::processLogic(uint32_t now)
{
    // Surchauffe : fin de la cuisson. Le contacteur reste ouvert et la
    // cuisson ne redémarre pas tant que l'alarme n'est pas acquittée.
    if (overTemperature.isActive() && program.isRunning())
    {
        program.stop();
        process->logEvent(
            now, EventKind::Alarm, true, "Cuisson arrêtée : surchauffe");
    }

    const SetpointProgram::State state = program.state();

    if (state == SetpointProgram::State::Finished &&
        previousState != SetpointProgram::State::Finished)
    {
        process->logEvent(
            now, EventKind::Info, true,
            "Cuisson P%u terminée",
            static_cast<unsigned>(program.runningProgram()));
    }

    previousState = state;

    // Four au-dessus de la consigne pendant une descente : normal.
    deviation.inhibit(program.isCooling());

    const bool heating =
        program.isCommandValid() &&
        program.readCommand() >= 0.5;

    contactorCommand.setOn(
        heating &&
        !overTemperature.isActive() &&
        !deviation.isActive());
}

bool KilnInstallation::addMenuActions(
    MenuBuilder& menu) const
{
    const MenuBuilder::GroupId group =
        menu.findGroupForOwner("kiln_program");

    return
        group != MenuBuilder::INVALID_GROUP &&
        menu.addAction(group, START_ACTION, "kiln_start", "Démarrer") &&
        menu.addAction(group, STOP_ACTION, "kiln_stop", "Arrêter") &&
        menu.addAction(group, SKIP_ACTION, "kiln_skip", "Segment suivant");
}

bool KilnInstallation::executeMenuAction(
    MenuBuilder::ActionId actionId)
{
    switch (actionId)
    {
    case START_ACTION:
        // Pas de cuisson sur une alarme non acquittée : le contacteur
        // resterait ouvert.
        return !overTemperature.isActive() &&
               !deviation.isActive() &&
               program.start();

    case STOP_ACTION:
        program.stop();
        return true;

    case SKIP_ACTION:
        return program.skipSegment();

    default:
        return false;
    }
}

void KilnInstallation::onMenuActionSaveFailed(
    MenuBuilder::ActionId actionId)
{
    if (actionId == START_ACTION)
        program.stop();
}

void KilnInstallation::captureHomeScreenState()
{
    homeState.state = program.state();
    homeState.program = program.runningProgram();
    homeState.selected = program.settings.selected;
    homeState.segment = program.segment();
    homeState.segmentCount = program.segmentCount();
    homeState.setpointValid = program.readSetpoint(homeState.setpoint);
    homeState.targetValid = program.readSegmentTarget(homeState.target);
    homeState.heldBack = program.isHeldBack();
    homeState.delayRemaining = program.delayRemainingSeconds();
    homeState.soakRemaining = program.soakRemainingSeconds();
    homeState.remaining = program.remainingSeconds();
    homeState.elapsed = program.elapsedSeconds();
    homeState.outputValid = pid.isCommandValid();
    homeState.output = pid.readCommand();
}

void KilnInstallation::printHomeScreen(
    HomeScreenContext& context)
{
#ifdef OPC_HOST_TEST
    (void)context;
#else
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

        printLabel(display, TARGET_Y, "Cible");
        printLabel(display, POWER_Y, "Puiss.");
    }

    char text[48];
    const State state = homeState.state;

    // ----- En-tête : état du programme, programme et segment -----

    TextField::print(
        display,
        MARGIN,
        HEADER_Y,
        2,
        titleColor(state),
        title(state),
        TITLE_CHARS);

    if (homeState.segment > 0)
    {
        snprintf(
            text,
            sizeof(text),
            "P%u S%u/%u",
            homeState.program,
            homeState.segment,
            homeState.segmentCount);
    }
    else
    {
        // À l'arrêt : le programme que Démarrer lancera.
        snprintf(
            text,
            sizeof(text),
            "P%u",
            homeState.program > 0 ? homeState.program : homeState.selected);
    }

    TextField::print(
        display,
        RIGHT - TextField::pixelWidth(STEP_CHARS, 2),
        HEADER_Y,
        2,
        COLOR_WHITE,
        text,
        STEP_CHARS,
        Align::Right);

    // ----- Mesure : valeur ou état du thermocouple, en grand -----

    const MeasurementSample* sample =
        context.snapshot.find(temperature);

    if (sample != nullptr)
    {
        MeasurementSample rounded = *sample;
        rounded.decimals = 0;
        MeasurementDisplay::format(
            &rounded, text, sizeof(text), MEASUREMENT_CHARS);
    }
    else
    {
        MeasurementDisplay::format(
            nullptr, text, sizeof(text), MEASUREMENT_CHARS);
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

    // ----- Consigne ("Attente" : temps figé par l'écart maxi) -----

    printLabel(
        display,
        SETPOINT_Y,
        homeState.heldBack ? "Attente" : "Consigne");

    if (homeState.setpointValid)
    {
        printCelsius(
            display,
            SETPOINT_Y,
            homeState.heldBack ? COLOR_ORANGE : COLOR_GREEN,
            homeState.setpoint);
    }
    else
    {
        printValue(display, SETPOINT_Y, COLOR_GREY, "--");
    }

    // ----- Cible du segment -----

    if (homeState.targetValid)
        printCelsius(display, TARGET_Y, COLOR_WHITE, homeState.target);
    else
        printValue(display, TARGET_Y, COLOR_GREY, "--");

    // ----- Temps : départ différé, palier, reste du programme, durée -----

    switch (state)
    {
    case State::Delayed:
        printLabel(display, TIME_Y, "Depart");
        printDuration(display, TIME_Y, homeState.delayRemaining);
        break;

    case State::Soak:
        printLabel(display, TIME_Y, "Palier");
        printDuration(display, TIME_Y, homeState.soakRemaining);
        break;

    case State::Ramp:
        printLabel(display, TIME_Y, "Reste");
        printDuration(display, TIME_Y, homeState.remaining);
        break;

    case State::Hold:
    case State::Finished:
        printLabel(display, TIME_Y, "Duree");
        printDuration(display, TIME_Y, homeState.elapsed);
        break;

    default:
        printLabel(display, TIME_Y, "Reste");
        printValue(display, TIME_Y, COLOR_GREY, "--");
        break;
    }

    // ----- Puissance demandée par le PID -----

    if (homeState.outputValid && std::isfinite(homeState.output))
    {
        snprintf(
            text,
            sizeof(text),
            "%.0f %%",
            constrain(homeState.output, 0.0, 1.0) * 100.0);
        printValue(display, POWER_Y, COLOR_WHITE, text);
    }
    else
    {
        printValue(display, POWER_Y, COLOR_GREY, "--");
    }

    // ----- Alarmes -----

    AlarmDisplay::printBanner(display, context.snapshot, ALARM_Y);

    // ----- Contacteur et durée écoulée -----

    const OutputSample* relay =
        context.snapshot.find(contactorRelay);

    const char* contactorText = "--";

    if (relay != nullptr && relay->healthy)
        contactorText = relay->appliedCommand >= 0.5 ? "ferme" : "ouvert";

    const uint32_t minutes = homeState.elapsed / 60;

    snprintf(
        text,
        sizeof(text),
        "Contacteur %s   Ecoule %lu:%02lu",
        contactorText,
        static_cast<unsigned long>(minutes / 60),
        static_cast<unsigned long>(minutes % 60));

    TextField::print(
        display,
        (display.width() -
            TextField::pixelWidth(FOOTER_CHARS, 1)) / 2,
        FOOTER_Y,
        1,
        COLOR_GREY,
        text,
        FOOTER_CHARS,
        Align::Center);
#endif
}
