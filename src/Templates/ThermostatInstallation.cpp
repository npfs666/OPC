#include <Templates/ThermostatInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>

#include <hmi/HomeScreen.h>
#include <hmi/AlarmDisplay.h>
#include <hmi/MeasurementDisplay.h>
#include <hmi/TextField.h>
#include <ProcessSnapshot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_GREY = 0x8410;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
    constexpr uint16_t COLOR_GREEN = 0x07E0;
    constexpr uint16_t COLOR_DARK_GREEN = 0x03E0;
    constexpr uint16_t COLOR_ORANGE = 0xFD20;
    constexpr uint16_t COLOR_YELLOW = 0xFFE0;   // consigne en réglage

    using TextField::Align;

    // Écran d'accueil 240 × 240, champs à largeur fixe (voir TextField).
    constexpr int16_t SCREEN_WIDTH = 240;
    constexpr int16_t MARGIN = 8;
    constexpr int16_t RIGHT = 232;              // bord droit des valeurs

    constexpr int16_t HEADER_Y = 8;             // taille 2
    constexpr int16_t SEPARATOR_Y = 30;
    constexpr int16_t MEASUREMENT_Y = 42;       // zone de 48 px
    constexpr int16_t MEASUREMENT_HEIGHT = 48;
    constexpr int16_t SETPOINT_Y = 104;         // taille 2
    constexpr int16_t GAUGE_Y = 138;
    constexpr int16_t GAUGE_HEIGHT = 14;
    constexpr int16_t MARKER_OVERHANG = 5;      // repère plus haut que la jauge
    constexpr int16_t RELAY_Y = 172;            // taille 2
    constexpr int16_t ALARM_Y = 190;            // taille 2
    constexpr int16_t SAFETY_Y = 208;           // taille 2

    /*
     * Mesure : nombre en taille 6 (36 × 48 px par caractère) et unité en
     * taille 3, en exposant. Un défaut capteur s'affiche en taille 4 pour
     * garder son libellé complet ("RUPTURE", "C-CIRCUIT").
     */
    constexpr uint8_t NUMBER_SIZE = 6;
    constexpr size_t NUMBER_CHARS = 5;          // "123.4", "-12.5"
    constexpr uint8_t UNIT_SIZE = 3;
    constexpr int16_t UNIT_GAP = 6;
    constexpr int16_t NUMBER_X =
        (SCREEN_WIDTH -
         TextField::pixelWidth(NUMBER_CHARS, NUMBER_SIZE) -
         UNIT_GAP -
         TextField::pixelWidth(2, UNIT_SIZE)) / 2;
    constexpr uint8_t STATUS_LABEL_SIZE = 4;
    constexpr size_t STATUS_LABEL_CHARS = 9;

    constexpr size_t LABEL_CHARS = 8;           // "Consigne"
    constexpr size_t VALUE_CHARS = 9;           // "-123.4 °C"
    constexpr size_t TIME_CHARS = 5;            // "14:32"
    constexpr size_t SAFETY_CHARS = 8;          // "SECURITE"

    /*
     * Jauge : bande d'hystérésis (vert foncé) autour de la consigne (trait
     * vert), repère blanc à la mesure. L'échelle couvre consigne ± 2 ×
     * hystérésis, au moins ± 1 K ; au-delà, le repère reste en butée.
     */
    constexpr int16_t GAUGE_LEFT = MARGIN;
    constexpr int16_t GAUGE_WIDTH = RIGHT - MARGIN;
    constexpr double_t GAUGE_MIN_HALF_SPAN = 1.0;

    constexpr const char* DEGREES_C = "\xF8" "C"; // "°C" en CP437

    constexpr const char* RAMP_OWNER_KEY =
        "thermostat.ramp";

    constexpr const char* RAMP_OWNER_NAME =
        "Rampe thermostat";

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
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    temperatureInput.begin(
        "thermostat_input",
        "Sonde thermostat",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0.0f);

    if (!board.addSensor(
            temperatureInput))
    {
        return fail("Sonde thermostat : entrée analogique indisponible");
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
        return fail("Mesures thermostat non enregistrées");
    }

    thermostat.begin(
        "thermostat",
        "Thermostat",
        temperature);

    if (!process.add(thermostat))
        return fail("Thermostat non enregistré");

    relayActuator.begin(
        "thermostat_relay_actuator",
        "Commande thermostat",
        thermostat);

    if (!process.add(relayActuator))
        return fail("Commande thermostat non enregistrée");

    relayOutput.begin(
        "thermostat_relay",
        "Output 1",
        Board::Rp2040::OUTPUT_1,
        true,
        false);

    if (!process.connect(
            relayActuator,
            relayOutput))
    {
        return fail("Relais thermostat non relié à la commande");
    }

    // Alarme de température haute, à activer dans le menu Alarmes. Le
    // thermostat sert de référence aux types relatifs à la consigne.
    alarm.begin("thermostat_alarm", "Temp. haute", temperature);
    alarm.settings.limit = 80.0;
    alarm.setReference(thermostat);

    if (!process.add(alarm))
        return fail("Alarme thermostat non enregistrée");

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (!thermostat.setpointRamp.registerParameters(
            parameterList,
            RAMP_OWNER_KEY,
            RAMP_OWNER_NAME,
            "°C/min"))
    {
        return fail("Paramètres rampe thermostat");
    }

    if (parameterList.hasError())
        return fail("Paramètres thermostat invalides");

    // Consigne réglable à l'encodeur depuis l'écran d'accueil.
    if (!setHomeSetpoint("thermostat", "setpoint"))
        return fail("Consigne d'accueil introuvable");

    return true;
}

void ThermostatInstallation::captureHomeScreenState()
{
    homeState.mode = thermostat.settings.mode;
    homeState.hysteresis = thermostat.settings.hysteresis;
    homeState.commandValid = thermostat.isCommandValid();
    homeState.fallback = thermostat.isInFallback();

    const bool rampActive =
        thermostat.setpointRamp.hasActiveSetpoint();

    homeState.setpoint =
        rampActive
            ? thermostat.setpointRamp.activeSetpoint()
            : thermostat.settings.setpoint;

    homeState.ramping =
        rampActive &&
        std::fabs(
            homeState.setpoint -
            thermostat.settings.setpoint) > 0.05;
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
        display.drawFastHLine(
            MARGIN,
            SEPARATOR_Y,
            RIGHT - MARGIN,
            COLOR_GREY);

        TextField::print(display, MARGIN, RELAY_Y, 2, COLOR_WHITE, "Relais", LABEL_CHARS);

        measurementDrawn = false;
        gauge.drawn = false;
    }

    const bool cooling =
        homeState.mode == Thermostat::Mode::Cooling;

    // ----- En-tête : sens d'action et heure -----

    TextField::print(
        display,
        MARGIN,
        HEADER_Y,
        2,
        cooling ? COLOR_CYAN : COLOR_ORANGE,
        cooling ? "FROID" : "CHAUD",
        5);

    char text[24];

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

    // ----- Mesure -----

    const MeasurementSample* sample =
        context.snapshot.find(temperature);

    printMeasurement(display, sample);

    // ----- Consigne active ("Rampe" pendant une rampe) -----

    TextField::print(
        display,
        MARGIN,
        SETPOINT_Y,
        2,
        COLOR_WHITE,
        !context.editingSetpoint && homeState.ramping ? "Rampe" : "Consigne",
        LABEL_CHARS);

    // En réglage à l'encodeur : la valeur réglée, en jaune.
    const double_t setpoint =
        context.editingSetpoint
            ? context.editedSetpoint
            : homeState.setpoint;

    if (std::isfinite(setpoint))
    {
        snprintf(
            text,
            sizeof(text),
            "%.1f %s",
            setpoint,
            DEGREES_C);
    }
    else
    {
        snprintf(text, sizeof(text), "--");
    }

    printValue(
        display,
        SETPOINT_Y,
        context.editingSetpoint ? COLOR_YELLOW : COLOR_GREEN,
        text);

    // ----- Jauge des seuils -----

    printGauge(display, sample);

    // ----- Relais -----

    const OutputSample* relaySample =
        context.snapshot.find(relayOutput);

    if (relaySample == nullptr || !relaySample->healthy)
        printValue(display, RELAY_Y, COLOR_GREY, "--");
    else if (relaySample->appliedCommand >= 0.5)
        printValue(display, RELAY_Y, COLOR_GREEN, "ON");
    else if (relaySample->waitingToStart())
        printValue(display, RELAY_Y, COLOR_ORANGE, "ATTENTE");
    else
        printValue(display, RELAY_Y, COLOR_GREY, "OFF");

    // ----- Alarmes -----

    AlarmDisplay::printBanner(display, context.snapshot, ALARM_Y);

    // ----- Relais forcé en état sûr, ou maintenu sur défaut capteur -----

    TextField::print(
        display,
        (SCREEN_WIDTH - TextField::pixelWidth(SAFETY_CHARS, 2)) / 2,
        SAFETY_Y,
        2,
        COLOR_ORANGE,
        homeState.fallback
            ? "REPLI"
            : homeState.commandValid ? "" : "SECURITE",
        SAFETY_CHARS,
        Align::Center);
}

void ThermostatInstallation::printMeasurement(
    Adafruit_GFX& display,
    const MeasurementSample* sample)
{
    const bool asNumber =
        sample != nullptr &&
        sample->valid &&
        std::isfinite(sample->value);

    // Le nombre et le libellé d'état n'ont pas la même taille : on efface
    // la zone quand on passe de l'un à l'autre.
    if (!measurementDrawn || asNumber != measurementAsNumber)
    {
        display.fillRect(
            MARGIN,
            MEASUREMENT_Y,
            RIGHT - MARGIN,
            MEASUREMENT_HEIGHT,
            COLOR_BLACK);
    }

    measurementDrawn = true;
    measurementAsNumber = asNumber;

    char text[24];

    if (!asNumber)
    {
        MeasurementDisplay::format(
            sample,
            text,
            sizeof(text),
            STATUS_LABEL_CHARS);

        TextField::print(
            display,
            (SCREEN_WIDTH -
                TextField::pixelWidth(STATUS_LABEL_CHARS, STATUS_LABEL_SIZE)) / 2,
            MEASUREMENT_Y + 8,
            STATUS_LABEL_SIZE,
            MeasurementDisplay::color(sample, COLOR_WHITE),
            text,
            STATUS_LABEL_CHARS,
            Align::Center);
        return;
    }

    // Une décimale ; aucune si la valeur ne tient pas (-123.4).
    snprintf(text, sizeof(text), "%.1f", sample->value);

    if (std::strlen(text) > NUMBER_CHARS)
        snprintf(text, sizeof(text), "%.0f", sample->value);

    TextField::print(
        display,
        NUMBER_X,
        MEASUREMENT_Y,
        NUMBER_SIZE,
        COLOR_WHITE,
        text,
        NUMBER_CHARS,
        Align::Right);

    TextField::print(
        display,
        NUMBER_X +
            TextField::pixelWidth(NUMBER_CHARS, NUMBER_SIZE) +
            UNIT_GAP,
        MEASUREMENT_Y,
        UNIT_SIZE,
        COLOR_WHITE,
        DEGREES_C,
        2);
}

void ThermostatInstallation::printGauge(
    Adafruit_GFX& display,
    const MeasurementSample* sample)
{
    GaugeState next;

    const double_t hysteresis =
        std::isfinite(homeState.hysteresis)
            ? homeState.hysteresis
            : 0.0;

    const double_t halfSpan =
        std::max(2.0 * hysteresis, GAUGE_MIN_HALF_SPAN);

    const auto xFor = [&](double_t value) -> int16_t
    {
        const double_t ratio =
            (value - homeState.setpoint + halfSpan) / (2.0 * halfSpan);

        const double_t clamped = constrain(ratio, 0.0, 1.0);

        // Intérieur du cadre, repère de 3 px compris.
        return static_cast<int16_t>(
            GAUGE_LEFT + 2 +
            std::lround(clamped * (GAUGE_WIDTH - 5)));
    };

    if (std::isfinite(homeState.setpoint))
    {
        next.setpointX = xFor(homeState.setpoint);
        next.bandStart = xFor(homeState.setpoint - hysteresis / 2.0);
        next.bandEnd = xFor(homeState.setpoint + hysteresis / 2.0);

        if (sample != nullptr &&
            sample->valid &&
            std::isfinite(sample->value))
        {
            next.markerX = xFor(sample->value);
        }
    }

    const bool backgroundChanged =
        !gauge.drawn ||
        next.setpointX != gauge.setpointX ||
        next.bandStart != gauge.bandStart ||
        next.bandEnd != gauge.bandEnd;

    if (backgroundChanged)
    {
        // Fond complet (cadre, bande, consigne), colonne par colonne et sans
        // effacement préalable : pas de scintillement pendant une rampe.
        for (int16_t x = GAUGE_LEFT; x < GAUGE_LEFT + GAUGE_WIDTH; x++)
            drawGaugeColumn(display, next, x);
    }
    else if (next.markerX != gauge.markerX && gauge.markerX >= 0)
    {
        // Seul le repère a bougé : on restaure le fond sous l'ancien.
        for (int16_t x = gauge.markerX - 1; x <= gauge.markerX + 1; x++)
            drawGaugeColumn(display, next, x);
    }

    if (next.markerX >= 0)
    {
        display.fillRect(
            next.markerX - 1,
            GAUGE_Y - MARKER_OVERHANG,
            3,
            GAUGE_HEIGHT + 2 * MARKER_OVERHANG,
            COLOR_WHITE);
    }

    next.drawn = true;
    gauge = next;
}

void ThermostatInstallation::drawGaugeColumn(
    Adafruit_GFX& display,
    const GaugeState& state,
    int16_t x)
{
    // Débords au-dessus et au-dessous du cadre.
    display.drawFastVLine(x, GAUGE_Y - MARKER_OVERHANG, MARKER_OVERHANG, COLOR_BLACK);
    display.drawFastVLine(x, GAUGE_Y + GAUGE_HEIGHT, MARKER_OVERHANG, COLOR_BLACK);

    const bool border =
        x == GAUGE_LEFT ||
        x == GAUGE_LEFT + GAUGE_WIDTH - 1;

    if (border)
    {
        display.drawFastVLine(x, GAUGE_Y, GAUGE_HEIGHT, COLOR_GREY);
        return;
    }

    uint16_t inside = COLOR_BLACK;

    if (state.setpointX >= 0 && x == state.setpointX)
        inside = COLOR_GREEN;
    else if (state.setpointX >= 0 && x >= state.bandStart && x <= state.bandEnd)
        inside = COLOR_DARK_GREEN;

    display.drawPixel(x, GAUGE_Y, COLOR_GREY);
    display.drawFastVLine(x, GAUGE_Y + 1, GAUGE_HEIGHT - 2, inside);
    display.drawPixel(x, GAUGE_Y + GAUGE_HEIGHT - 1, COLOR_GREY);
}
