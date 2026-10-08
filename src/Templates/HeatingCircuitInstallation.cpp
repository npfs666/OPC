// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Templates/HeatingCircuitInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <hmi/HomeScreen.h>
#include <ProcessSnapshot.h>

#include <cmath>
#include <cstdio>

// Le dessin n'est compilé que pour la carte : les tests sur l'hôte
// vérifient la logique du template, pas l'écran.
#ifndef OPC_HOST_TEST
#include <Adafruit_GFX.h>

#include <hmi/AlarmDisplay.h>
#include <hmi/MeasurementDisplay.h>
#include <hmi/TextField.h>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_GREY = 0x8410;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
    constexpr uint16_t COLOR_GREEN = 0x07E0;
    constexpr uint16_t COLOR_ORANGE = 0xFD20;
    constexpr uint16_t COLOR_YELLOW = 0xFFE0;

    using TextField::Align;

    // Écran d'accueil 240 × 240, champs à largeur fixe (voir TextField) ;
    // toutes les lignes en taille 2, tous les 24 px.
    constexpr int16_t MARGIN = 8;
    constexpr int16_t RIGHT = 232;          // bord droit des valeurs

    constexpr int16_t HEADER_Y = 8;
    constexpr int16_t SEPARATOR_1_Y = 30;
    constexpr int16_t OUTDOOR_Y = 40;
    constexpr int16_t ROOM_Y = 64;
    constexpr int16_t FLOW_Y = 88;
    constexpr int16_t SETPOINT_Y = 112;
    constexpr int16_t SEPARATOR_2_Y = 136;
    constexpr int16_t VALVE_Y = 146;
    constexpr int16_t PUMP_Y = 170;
    constexpr int16_t ALARM_Y = 200;

    constexpr size_t LABEL_CHARS = 8;       // "Consigne"
    constexpr size_t VALUE_CHARS = 9;       // "-123.4 °C", "RECALAGE"
    constexpr size_t TITLE_CHARS = 8;       // "HORS-GEL"
    constexpr size_t TIME_CHARS = 5;        // "14:32"

    // Champ de valeur aligné sur le bord droit.
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

    void printCelsius(
        Adafruit_GFX& display,
        int16_t y,
        uint16_t color,
        double_t value)
    {
        char text[24];
        snprintf(text, sizeof(text), "%.1f \xF8" "C", value);
        printValue(display, y, color, text);
    }

    const char* title(HeatingCurve::State state)
    {
        switch (state)
        {
        case HeatingCurve::State::Comfort:
            return "CONFORT";

        case HeatingCurve::State::Reduced:
            return "REDUIT";

        case HeatingCurve::State::Summer:
            return "ETE";

        case HeatingCurve::State::Off:
            return "ARRET";

        case HeatingCurve::State::Frost:
            return "HORS-GEL";

        case HeatingCurve::State::ClockInvalid:
            return "HEURE ?";

        default:
            return "ATTENTE";
        }
    }

    uint16_t titleColor(HeatingCurve::State state)
    {
        switch (state)
        {
        case HeatingCurve::State::Comfort:
            return COLOR_GREEN;

        case HeatingCurve::State::Reduced:
        case HeatingCurve::State::Frost:
            return COLOR_CYAN;

        case HeatingCurve::State::ClockInvalid:
            return COLOR_ORANGE;

        default:
            return COLOR_GREY;
        }
    }
}
#endif

const char* HeatingCircuitInstallation::name() const
{
    return "Circuit de chauffage";
}

const char* HeatingCircuitInstallation::configurationKey() const
{
    return "heating_circuit";
}

bool HeatingCircuitInstallation::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    // ----- Sondes -----

    flowInput.begin(
        "heating_flow_input",
        "Départ",
        Sensor::Type::Pt100,
        Sensor::Wiring::ThreeWire,
        16,
        0.0f);

    // Sonde extérieure : Pt1000 deux fils, insensible au câble long.
    outdoorInput.begin(
        "heating_outdoor_input",
        "Extérieure",
        Sensor::Type::Pt1000,
        Sensor::Wiring::TwoWire,
        16,
        0.0f);

    if (!board.addSensor(flowInput) ||
        !board.addSensor(outdoorInput))
    {
        return fail("Sondes chauffage : entrées analogiques indisponibles");
    }

    flowResistance.begin("Resistance depart", board, flowInput);
    outdoorResistance.begin("Resistance exterieure", board, outdoorInput);

    flowTemperature.begin("Départ", flowResistance);
    outdoorTemperature.begin("Extérieure", outdoorResistance);

    if (!process.add(flowResistance) ||
        !process.add(flowTemperature) ||
        !process.add(outdoorResistance) ||
        !process.add(outdoorTemperature))
    {
        return fail("Mesures chauffage non enregistrées");
    }

    // ----- Loi d'eau et programme -----

    comfortSchedule.begin(
        "heating_comfort",
        "Confort",
        process.clock());

    comfortSchedule.settings.slots[0] = {
        TimeSchedule::Days::Everyday, 6 * 60, 22 * 60
    };

    curve.begin("heating_curve", "Loi d'eau", outdoorTemperature);
    curve.setSchedule(comfortSchedule, 17.0);

    // Avant le PID : il lit la consigne de départ du même cycle.
    if (!process.add(comfortSchedule) ||
        !process.add(curve))
    {
        return fail("Loi d'eau non enregistrée");
    }

    // ----- Départ : PID et vanne 3 points -----

    flowControl.begin("heating_flow", "Départ", flowTemperature);
    flowControl.followSetpoint(curve);

    // Point de départ prudent pour une vanne de 2 min : 5 %/K, Ti 4 min.
    // À affiner par l'autotune ou à la mise en service.
    flowControl.setTunings(0.05, 240.0, 0.0);

    valve.begin(
        "heating_valve",
        "Vanne",
        flowControl,
        valveOpen,
        valveClose,
        120);

    valveOpen.begin(
        "heating_valve_open",
        "Vanne ouvrir",
        Board::Rp2040::OUTPUT_1);

    // État sûr ON : la vanne se ferme en défaut.
    valveClose.begin(
        "heating_valve_close",
        "Vanne fermer",
        Board::Rp2040::OUTPUT_2,
        true,
        true);

    if (!process.add(flowControl) ||
        !process.add(valve) ||
        !process.connect(valve, valveOpen) ||
        !process.connect(valve, valveClose))
    {
        return fail("Vanne non reliée");
    }

    // ----- Pompe : demande de chauffe, post-circulation -----

    pumpOverrun.begin(
        "heating_pump_overrun",
        "Post-circulation",
        DelayTimer::Mode::OffDelay,
        5,
        DelayTimer::Unit::Minutes);
    pumpOverrun.setSource(curve);

    pump.begin("heating_pump", "Pompe", pumpOverrun);

    // Commande de sécurité 1 : la pompe tourne en défaut (gel).
    pumpOutput.begin(
        "heating_pump_output",
        "Sortie pompe",
        Board::Rp2040::OUTPUT_3,
        true,
        1.0);

    if (!process.add(pumpOverrun) ||
        !process.add(pump) ||
        !process.connect(pump, pumpOutput))
    {
        return fail("Pompe non reliée");
    }

    // ----- Alarme de départ haut, à activer pour un plancher -----

    flowAlarm.begin("heating_flow_alarm", "Départ haut", flowTemperature);
    flowAlarm.settings.limit = 55.0;

    if (!process.add(flowAlarm))
        return fail("Alarme départ non enregistrée");

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres chauffage invalides");

    // Consigne d'ambiance réglable à l'encodeur depuis l'écran d'accueil.
    if (!setHomeSetpoint("heating_curve", "room_setpoint"))
        return fail("Consigne d'accueil introuvable");

    return true;
}

void HeatingCircuitInstallation::captureHomeScreenState()
{
    homeState.state = curve.state();
    homeState.outdoor = curve.outdoorTemperature();
    homeState.outdoorFallback = curve.isOutdoorFallback();
    homeState.roomSetpoint = curve.settings.roomSetpoint;
    homeState.flowSetpointValid = curve.readSetpoint(homeState.flowSetpoint);
    homeState.valvePosition = valve.position();
    homeState.valveKnown = valve.isPositionKnown();
}

void HeatingCircuitInstallation::printHomeScreen(
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
        display.drawFastHLine(MARGIN, SEPARATOR_1_Y, RIGHT - MARGIN, COLOR_GREY);
        display.drawFastHLine(MARGIN, SEPARATOR_2_Y, RIGHT - MARGIN, COLOR_GREY);

        const struct
        {
            int16_t y;
            const char* label;
        } labels[] = {
            {OUTDOOR_Y, "Ext."},
            {ROOM_Y, "Ambiance"},
            {FLOW_Y, "Depart"},
            {SETPOINT_Y, "Consigne"},
            {VALVE_Y, "Vanne"},
            {PUMP_Y, "Pompe"}
        };

        for (const auto& row : labels)
        {
            TextField::print(
                display, MARGIN, row.y, 2, COLOR_WHITE, row.label, LABEL_CHARS);
        }
    }

    char text[24];

    // ----- En-tête : état de la loi d'eau et heure -----

    TextField::print(
        display,
        MARGIN,
        HEADER_Y,
        2,
        titleColor(homeState.state),
        title(homeState.state),
        TITLE_CHARS);

    const ClockSample& clock = context.snapshot.clock();

    if (clock.valid)
    {
        snprintf(
            text,
            sizeof(text),
            "%02u:%02u",
            clock.dateTime.hour,
            clock.dateTime.minute);
    }
    else
    {
        snprintf(text, sizeof(text), "--:--");
    }

    TextField::print(
        display,
        RIGHT - TextField::pixelWidth(TIME_CHARS, 2),
        HEADER_Y,
        2,
        COLOR_WHITE,
        text,
        TIME_CHARS);

    // ----- Extérieur : valeur de la courbe, en orange sur secours -----

    if (homeState.state == HeatingCurve::State::Waiting)
        printValue(display, OUTDOOR_Y, COLOR_GREY, "--");
    else
    {
        printCelsius(
            display,
            OUTDOOR_Y,
            homeState.outdoorFallback ? COLOR_ORANGE : COLOR_WHITE,
            homeState.outdoor);
    }

    // ----- Ambiance : réglable à l'encodeur, en jaune pendant le réglage -----

    printCelsius(
        display,
        ROOM_Y,
        context.editingSetpoint ? COLOR_YELLOW : COLOR_GREEN,
        context.editingSetpoint
            ? context.editedSetpoint
            : homeState.roomSetpoint);

    // ----- Départ mesuré et consigne -----

    const MeasurementSample* flow =
        context.snapshot.find(flowTemperature);

    if (flow != nullptr)
    {
        MeasurementSample rounded = *flow;
        rounded.decimals = 1;
        MeasurementDisplay::format(&rounded, text, sizeof(text), VALUE_CHARS);
    }
    else
    {
        MeasurementDisplay::format(nullptr, text, sizeof(text), VALUE_CHARS);
    }

    printValue(
        display,
        FLOW_Y,
        MeasurementDisplay::color(flow, COLOR_WHITE),
        text);

    if (homeState.flowSetpointValid)
        printCelsius(display, SETPOINT_Y, COLOR_GREEN, homeState.flowSetpoint);
    else
        printValue(display, SETPOINT_Y, COLOR_GREY, "--");

    // ----- Vanne : position estimée, ou recalage du démarrage -----

    if (!homeState.valveKnown)
        printValue(display, VALVE_Y, COLOR_ORANGE, "RECALAGE");
    else
    {
        snprintf(
            text,
            sizeof(text),
            "%.0f %%",
            homeState.valvePosition * 100.0);
        printValue(display, VALVE_Y, COLOR_WHITE, text);
    }

    // ----- Pompe -----

    const OutputSample* pumpSample =
        context.snapshot.find(pumpOutput);

    if (pumpSample == nullptr || !pumpSample->healthy)
        printValue(display, PUMP_Y, COLOR_GREY, "--");
    else if (pumpSample->appliedCommand >= 0.5)
        printValue(display, PUMP_Y, COLOR_GREEN, "ON");
    else
        printValue(display, PUMP_Y, COLOR_GREY, "OFF");

    // ----- Alarmes -----

    AlarmDisplay::printBanner(display, context.snapshot, ALARM_Y);
#endif
}
