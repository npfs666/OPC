// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Templates/ColdRoomInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <hmi/HomeScreen.h>
#include <ProcessSnapshot.h>

#include <Arduino.h>

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
    constexpr uint16_t COLOR_YELLOW = 0xFFE0;   // consigne en réglage

    using TextField::Align;

    // Écran d'accueil 240 × 240, champs à largeur fixe (voir TextField) ;
    // toutes les lignes en taille 2, tous les 24 px.
    constexpr int16_t MARGIN = 8;
    constexpr int16_t RIGHT = 232;          // bord droit des valeurs

    constexpr int16_t HEADER_Y = 8;
    constexpr int16_t SEPARATOR_1_Y = 30;
    constexpr int16_t AMBIENT_Y = 40;
    constexpr int16_t SETPOINT_Y = 64;
    constexpr int16_t EVAPORATOR_Y = 88;
    constexpr int16_t SEPARATOR_2_Y = 112;
    constexpr int16_t COMPRESSOR_Y = 122;
    constexpr int16_t FANS_Y = 146;
    constexpr int16_t DEFROST_Y = 170;
    constexpr int16_t ALARM_Y = 200;

    constexpr size_t LABEL_CHARS = 9;       // "Compress."
    constexpr size_t VALUE_CHARS = 9;       // "-123.4 °C", "C-CIRCUIT"
    constexpr size_t TITLE_CHARS = 9;       // "DEGIVRAGE"
    constexpr size_t TIME_CHARS = 5;        // "14:32"

    constexpr const char* DEGREES_C = "\xF8" "C"; // "°C" en CP437

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

    // Température à une décimale, ou état du capteur en couleur.
    void printTemperature(
        Adafruit_GFX& display,
        int16_t y,
        const MeasurementSample* sample)
    {
        char text[24];

        if (sample != nullptr)
        {
            MeasurementSample rounded = *sample;
            rounded.decimals = 1;
            MeasurementDisplay::format(&rounded, text, sizeof(text), VALUE_CHARS);
        }
        else
        {
            MeasurementDisplay::format(nullptr, text, sizeof(text), VALUE_CHARS);
        }

        printValue(
            display,
            y,
            MeasurementDisplay::color(sample, COLOR_WHITE),
            text);
    }

    // Durée restante : "5h12" au-delà d'une heure, sinon "12m30".
    void formatRemaining(char* text, size_t size, uint32_t ms)
    {
        const uint32_t seconds = (ms + 999) / 1000;

        if (seconds >= 3600)
        {
            snprintf(
                text, size, "%luh%02lu",
                static_cast<unsigned long>(seconds / 3600),
                static_cast<unsigned long>((seconds % 3600) / 60));
        }
        else
        {
            snprintf(
                text, size, "%lum%02lu",
                static_cast<unsigned long>(seconds / 60),
                static_cast<unsigned long>(seconds % 60));
        }
    }

    // État d'une sortie tout-ou-rien.
    void printOutput(
        Adafruit_GFX& display,
        int16_t y,
        const OutputSample* sample,
        uint16_t onColor)
    {
        if (sample == nullptr || !sample->healthy)
            printValue(display, y, COLOR_GREY, "--");
        else if (sample->appliedCommand >= 0.5)
            printValue(display, y, onColor, "ON");
        else if (sample->waitingToStart())
            printValue(display, y, COLOR_ORANGE, "ATTENTE");
        else
            printValue(display, y, COLOR_GREY, "OFF");
    }
}
#endif

const char* ColdRoomInstallation::name() const
{
    return "Chambre froide";
}

const char* ColdRoomInstallation::configurationKey() const
{
    return "cold_room";
}

bool ColdRoomInstallation::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    // ----- Sondes et mesures -----

    ambientInput.begin(
        "cold_room_ambient_input",
        "Sonde ambiance",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0.0f);

    evaporatorInput.begin(
        "cold_room_evaporator_input",
        "Sonde évaporateur",
        Sensor::Type::Pt100,
        Sensor::Wiring::ThreeWire,
        16,
        0.0f);

    if (!board.addSensor(ambientInput) ||
        !board.addSensor(evaporatorInput))
    {
        return fail("Sondes : entrées analogiques indisponibles");
    }

    ambientResistance.begin("Résistance ambiance", board, ambientInput);
    evaporatorResistance.begin(
        "Résistance évaporateur", board, evaporatorInput);

    ambientTemperature.begin("Ambiance", ambientResistance);
    evaporatorTemperature.begin("Évaporateur", evaporatorResistance);

    if (!process.add(ambientResistance) ||
        !process.add(ambientTemperature) ||
        !process.add(evaporatorResistance) ||
        !process.add(evaporatorTemperature))
    {
        return fail("Mesures non enregistrées");
    }

    // Porte : entrée active quand la porte est ouverte.
    door.begin(
        "cold_room_door",
        "Porte",
        Board::Rp2040::DIGITAL_INPUT_1);

    if (!process.add(door))
        return fail("Entrée porte non enregistrée");

    // ----- Froid : thermostat, relais du compresseur -----

    thermostat.begin("cold_room_thermostat", "Thermostat", ambientTemperature);
    thermostat.settings.mode = Thermostat::Mode::Cooling;
    thermostat.settings.setpoint = 3.0;
    thermostat.settings.hysteresis = 2.0;

    // Consigne d'une chambre froide, à l'accueil comme au menu.
    thermostat.setSetpointLimits(-10.0, 20.0);

    compressor.begin("cold_room_compressor", "Cde compresseur", thermostat);

    compressorRelay.begin(
        "cold_room_compressor_relay",
        "Compresseur",
        Board::Rp2040::OUTPUT_1,
        true,
        false);

    // Anti-court-cycle : 3 min d'arrêt avant chaque redémarrage.
    compressorRelay.settings.minOffTime = 180;

    if (!process.add(thermostat) ||
        !process.add(compressor) ||
        !process.connect(compressor, compressorRelay))
    {
        return fail("Compresseur non relié");
    }

    // ----- Dégivrage : conditions et durées -----

    defrostEnd.begin(
        "cold_room_defrost_end",
        "Fin dégivrage",
        evaporatorTemperature,
        Comparator::Direction::Above,
        8.0,
        8.0);
    defrostEnd.useSingleThreshold();
    defrostEnd.setRange(0.0, 30.0);
    defrostEnd.setLabels("Temp. évapo.", nullptr);

    defrostInterval.begin(
        "cold_room_defrost_interval",
        "Intervalle",
        DelayTimer::Mode::OnDelay,
        6,
        DelayTimer::Unit::Hours);

    defrostMaximum.begin(
        "cold_room_defrost_maximum",
        "Durée max",
        DelayTimer::Mode::OnDelay,
        30,
        DelayTimer::Unit::Minutes);

    drip.begin(
        "cold_room_drip",
        "Égouttage",
        DelayTimer::Mode::OnDelay,
        2,
        DelayTimer::Unit::Minutes);

    fanDelay.begin(
        "cold_room_fan_delay",
        "Retard ventil.",
        DelayTimer::Mode::OnDelay,
        3,
        DelayTimer::Unit::Minutes);

    alarmMask.begin(
        "cold_room_alarm_mask",
        "Masquage alarme",
        DelayTimer::Mode::OnDelay,
        30,
        DelayTimer::Unit::Minutes);

    // Réglages rangés dans le menu « Dégivrage » du template.
    defrostEnd.setMenuParent("cold_room_defrost");

    for (DelayTimer* timer :
         {&defrostInterval, &defrostMaximum, &drip, &fanDelay, &alarmMask})
    {
        timer->setMenuParent("cold_room_defrost");
    }

    if (!process.add(defrostEnd) ||
        !process.add(defrostInterval) ||
        !process.add(defrostMaximum) ||
        !process.add(drip) ||
        !process.add(fanDelay) ||
        !process.add(alarmMask))
    {
        return fail("Dégivrage non enregistré");
    }

    // ----- Ventilateurs : relais 2 -----

    fanCommand.begin("cold_room_fans", "Ventilateurs");
    fans.begin("cold_room_fans_cmd", "Cde ventilateurs", fanCommand);

    fanRelay.begin(
        "cold_room_fans_relay",
        "Ventilateurs",
        Board::Rp2040::OUTPUT_2,
        true,
        false);

    if (!process.add(fanCommand) ||
        !process.add(fans) ||
        !process.connect(fans, fanRelay))
    {
        return fail("Ventilateurs non reliés");
    }

    // ----- Résistance de dégivrage : PWM 1, relais statique -----

    heaterCommand.begin("cold_room_heater", "Résistance");

    // Sans sonde d'évaporateur, la résistance ne chauffe jamais : le
    // dégivrage se termine alors par sa durée max, compresseur arrêté.
    if (!heaterCommand.dependsOn(evaporatorTemperature))
        return fail("Dépendance résistance");

    heaterCommand.disableManualMode();
    heaterCommand.lockFaultAction(Regulator::FaultAction::SafeState);

    heater.begin("cold_room_heater_cmd", "Cde résistance", heaterCommand);

    heaterOutput.begin(
        "cold_room_heater_output",
        "Résistance",
        Board::Rp2040::OUTPUT_3,
        true,
        0.0);

    heaterOutput.lockSafeCommand(0.0);

    if (!process.add(heaterCommand) ||
        !process.add(heater) ||
        !process.connect(heater, heaterOutput))
    {
        return fail("Résistance non reliée");
    }

    // ----- Alarmes, à activer dans le menu Alarmes -----

    doorAlarm.begin("cold_room_door_alarm", "Porte ouverte", door);
    doorAlarm.settings.delay = 300;

    // Température haute : 4 K au-dessus de la consigne pendant 15 min.
    // Masquée pendant le cycle de dégivrage (inhibée par la glue).
    highAlarm.begin("cold_room_high_alarm", "Temp. haute", ambientTemperature);
    highAlarm.settings.type = LimitAlarm::Type::DeviationHigh;
    highAlarm.settings.limit = 4.0;
    highAlarm.settings.delay = 900;
    highAlarm.setReference(thermostat);
    highAlarm.allowInhibit();

    // Les alarmes sont évaluées avant la glue : masquée dès le premier
    // cycle, avant que la glue ne pose son inhibition.
    highAlarm.inhibit(true);

    if (!process.add(doorAlarm) ||
        !process.add(highAlarm))
    {
        return fail("Alarmes non enregistrées");
    }

    // ----- Paramètres -----

    board.registerParameters(parameterList);

    // Avant les régulateurs : son menu « Dégivrage » contient les durées.
    auto defrost = parameterList.forOwner({
        "regulators",
        "Regulateur",
        "cold_room_defrost",
        "Dégivrage"
    });

    defrost.addBool("enabled", "Dégivrage auto", defrostEnabled);

    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres chambre froide invalides");

    if (!setHomeSetpoint("cold_room_thermostat", "setpoint"))
        return fail("Consigne d'accueil introuvable");

    return true;
}

/*
 * Glue : cycle Froid -> Dégivrage -> Égouttage -> Reprise -> Froid.
 *
 * - Froid : le thermostat pilote le compresseur, les ventilateurs tournent
 *   sauf porte ouverte. Dégivrage après « Intervalle » de froid.
 * - Dégivrage : thermostat inhibé, résistance en marche, ventilateurs
 *   arrêtés. Fin quand l'évaporateur atteint « Temp. évapo. », ou au bout
 *   de « Durée max ».
 * - Égouttage : tout arrêté, l'eau s'écoule.
 * - Reprise : le compresseur repart ; les ventilateurs attendent que
 *   l'évaporateur refroidisse.
 *
 * L'alarme de température haute est masquée hors froid, et pendant
 * « Masquage alarme » après le retour en froid.
 */
void ColdRoomInstallation::processLogic(uint32_t now)
{
    const bool intervalElapsed =
        defrostInterval.run(phase == Phase::Cooling && defrostEnabled, now);
    const bool defrostTooLong =
        defrostMaximum.run(phase == Phase::Defrost, now);
    const bool dripDone =
        drip.run(phase == Phase::Drip, now);
    const bool fansDelayDone =
        fanDelay.run(phase == Phase::Recovery, now);
    const bool alarmUnmasked =
        alarmMask.run(phase == Phase::Cooling, now);

    switch (phase)
    {
    case Phase::Cooling:
        if (intervalElapsed)
            phase = Phase::Defrost;
        break;

    case Phase::Defrost:
        if (defrostEnd.isOn() || defrostTooLong)
            phase = Phase::Drip;
        break;

    case Phase::Drip:
        if (dripDone)
            phase = Phase::Recovery;
        break;

    case Phase::Recovery:
        if (fansDelayDone)
            phase = Phase::Cooling;
        break;
    }

    thermostat.inhibit(
        phase == Phase::Defrost ||
        phase == Phase::Drip);

    heaterCommand.setOn(phase == Phase::Defrost);

    // Porte en défaut : considérée fermée, l'alarme de porte le signale.
    fanCommand.setOn(phase == Phase::Cooling && !door.isActive());

    highAlarm.inhibit(phase != Phase::Cooling || !alarmUnmasked);
}

void ColdRoomInstallation::captureHomeScreenState()
{
    const uint32_t now = millis();

    homeState.phase = phase;
    homeState.defrostEnabled = defrostEnabled;
    homeState.manual = thermostat.isManual();
    homeState.doorOpen = door.isActive();
    homeState.setpoint = thermostat.settings.setpoint;

    switch (phase)
    {
    case Phase::Cooling:
        homeState.remainingMs = defrostInterval.remainingMs(now);
        break;
    case Phase::Defrost:
        homeState.remainingMs = defrostMaximum.remainingMs(now);
        break;
    case Phase::Drip:
        homeState.remainingMs = drip.remainingMs(now);
        break;
    case Phase::Recovery:
        homeState.remainingMs = fanDelay.remainingMs(now);
        break;
    }
}

void ColdRoomInstallation::printHomeScreen(
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
            {AMBIENT_Y, "Ambiance"},
            {EVAPORATOR_Y, "Evapo."},
            {COMPRESSOR_Y, "Compress."},
            {FANS_Y, "Ventil."},
            {DEFROST_Y, "Degivrage"}
        };

        for (const auto& row : labels)
        {
            TextField::print(
                display, MARGIN, row.y, 2, COLOR_WHITE, row.label, LABEL_CHARS);
        }
    }

    char text[24];

    // ----- En-tête : étape du cycle et heure -----

    const char* title = "FROID";
    uint16_t titleColor = COLOR_CYAN;

    switch (homeState.phase)
    {
    case Phase::Cooling:
        break;
    case Phase::Defrost:
        title = "DEGIVRAGE";
        titleColor = COLOR_ORANGE;
        break;
    case Phase::Drip:
        title = "EGOUTTAGE";
        titleColor = COLOR_ORANGE;
        break;
    case Phase::Recovery:
        title = "REPRISE";
        titleColor = COLOR_GREEN;
        break;
    }

    TextField::print(
        display, MARGIN, HEADER_Y, 2, titleColor, title, TITLE_CHARS);

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

    // ----- Températures et consigne -----

    printTemperature(
        display, AMBIENT_Y, context.snapshot.find(ambientTemperature));
    printTemperature(
        display, EVAPORATOR_Y, context.snapshot.find(evaporatorTemperature));

    TextField::print(
        display,
        MARGIN,
        SETPOINT_Y,
        2,
        COLOR_WHITE,
        "Consigne",
        LABEL_CHARS);

    const double_t setpoint =
        context.editingSetpoint ? context.editedSetpoint : homeState.setpoint;

    snprintf(text, sizeof(text), "%.1f %s", setpoint, DEGREES_C);
    printValue(
        display,
        SETPOINT_Y,
        context.editingSetpoint ? COLOR_YELLOW : COLOR_GREEN,
        text);

    // ----- Sorties -----

    if (homeState.manual)
        printValue(display, COMPRESSOR_Y, COLOR_ORANGE, "MANUEL");
    else
        printOutput(
            display,
            COMPRESSOR_Y,
            context.snapshot.find(compressorRelay),
            COLOR_CYAN);

    const OutputSample* fanSample = context.snapshot.find(fanRelay);

    if (homeState.doorOpen &&
        fanSample != nullptr &&
        fanSample->healthy &&
        fanSample->appliedCommand < 0.5)
    {
        printValue(display, FANS_Y, COLOR_ORANGE, "PORTE");
    }
    else
    {
        printOutput(display, FANS_Y, fanSample, COLOR_CYAN);
    }

    // ----- Dégivrage : prochain, ou fin de l'étape en cours -----

    if (homeState.phase == Phase::Cooling && !homeState.defrostEnabled)
    {
        printValue(display, DEFROST_Y, COLOR_GREY, "ARRET");
    }
    else
    {
        formatRemaining(text, sizeof(text), homeState.remainingMs);
        printValue(
            display,
            DEFROST_Y,
            homeState.phase == Phase::Cooling ? COLOR_WHITE : COLOR_ORANGE,
            text);
    }

    // ----- Alarmes -----

    AlarmDisplay::printBanner(display, context.snapshot, ALARM_Y);
#endif
}
