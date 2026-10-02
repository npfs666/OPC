#include <Templates/SolarInstallation.h>

#include <Hardware/SensorBoard.h>
#include <Hardware/pinout.h>
#include <ProcessControl.h>

#include <Adafruit_GFX.h>

#include <hmi/HomeScreen.h>
#include <hmi/MeasurementDisplay.h>
#include <hmi/TextField.h>
#include <ProcessSnapshot.h>

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

    // Écran d'accueil 240 × 240, champs à largeur fixe (voir TextField) ;
    // toutes les lignes en taille 2, tous les 24 px.
    constexpr int16_t MARGIN = 8;
    constexpr int16_t RIGHT = 232;          // bord droit des valeurs

    constexpr int16_t HEADER_Y = 8;
    constexpr int16_t SEPARATOR_1_Y = 30;
    constexpr int16_t COLLECTOR_Y = 40;
    constexpr int16_t TANK_TOP_Y = 64;
    constexpr int16_t TANK_BOTTOM_Y = 88;
    constexpr int16_t DELTA_Y = 112;
    constexpr int16_t SEPARATOR_2_Y = 136;
    constexpr int16_t PUMP_Y = 146;
    constexpr int16_t HEATER_Y = 170;

    constexpr size_t LABEL_CHARS = 8;       // "Ballon H"
    constexpr size_t VALUE_CHARS = 9;       // "-123.4 °C", "C-CIRCUIT"
    constexpr size_t TITLE_CHARS = 8;       // "VACANCES"
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

    bool isUsable(const MeasurementSample* sample)
    {
        return
            sample != nullptr &&
            sample->valid &&
            std::isfinite(sample->value);
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

bool SolarInstallation::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    collectorInput.begin(
        "solar_collector_input",
        "Capteur solaire",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0.0f);

    tankTopInput.begin(
        "tank_top_input",
        "Haut ballon",
        Sensor::Type::Pt100,
        Sensor::Wiring::FourWire,
        16,
        0.0f);

    tankBottomInput.begin(
        "tank_bottom_input",
        "Bas ballon",
        Sensor::Type::Pt100,
        Sensor::Wiring::ThreeWire,
        16,
        0.0f);

    if (!board.addSensor(collectorInput) ||
        !board.addSensor(tankTopInput) ||
        !board.addSensor(tankBottomInput))
    {
        return fail("Sondes solaires : entrées analogiques indisponibles");
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
        return fail("Mesures solaires non enregistrées");
    }

    solarRegulator.begin(
        "solar_regulator",
        "Regulateur solaire",
        collectorTemperature,
        tankTopTemperature,
        tankBottomTemperature);

    if (!process.add(solarRegulator))
        return fail("Régulateur solaire non enregistré");

    pump.begin(
        "solar_pump",
        "Pompe solaire",
        solarRegulator);

    if (!process.add(pump))
        return fail("Pompe solaire non enregistrée");

    pumpRelay.begin(
        "solar_pump_relay",
        "Relais pompe",
        Board::Rp2040::OUTPUT_1,
        true,
        false);

    if (!process.connect(
            pump,
            pumpRelay))
    {
        return fail("Relais pompe non relié à la pompe");
    }

    // ----- Mode vacances : décharge du ballon la nuit -----

    holidaySchedule.begin(
        "holiday_night",
        "Décharge nuit",
        process.clock());

    holidaySchedule.settings.slots[0] = {
        TimeSchedule::Days::Everyday, 23 * 60, 6 * 60
    };

    solarRegulator.setHolidaySchedule(holidaySchedule);

    // Le programme ne pilote pas de sortie : il sert au régulateur.
    if (!process.add(holidaySchedule))
        return fail("Programme vacances non enregistré");

    // ----- Appoint électrique en heures creuses -----

    offPeakSchedule.begin(
        "off_peak",
        "Heures creuses",
        process.clock());

    offPeakSchedule.settings.slots[0] = {
        TimeSchedule::Days::Everyday, 22 * 60, 6 * 60
    };

    backupHeater.begin(
        "backup_heater",
        "Appoint",
        tankTopTemperature);

    backupHeater.settings.setpoint = 55.0;
    backupHeater.settings.hysteresis = 5.0;

    // Hors heures creuses, l'appoint est coupé.
    backupHeater.setSchedule(offPeakSchedule, 45.0);

    // Sans mesure, un appoint électrique ne chauffe jamais.
    backupHeater.lockFaultAction(Regulator::FaultAction::SafeState);
    backupHeater.scheduledSetpoint.settings.outside =
        ScheduledSetpoint::Outside::Off;

    heater.begin(
        "backup_heater_cmd",
        "Appoint",
        backupHeater);

    heaterRelay.begin(
        "backup_heater_relay",
        "Relais appoint",
        Board::Rp2040::OUTPUT_2,
        true,
        false);

    // Une résistance ne doit jamais chauffer en état de repli.
    heaterRelay.lockSafeState(false);

    if (!process.add(offPeakSchedule) ||
        !process.add(backupHeater) ||
        !process.add(heater) ||
        !process.connect(heater, heaterRelay))
    {
        return fail("Appoint électrique non relié");
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres solaires invalides");

    return true;
}

void SolarInstallation::captureHomeScreenState()
{
    homeState.discharging = solarRegulator.isDischarging();
    homeState.holidayMode = solarRegulator.settings.holidayMode;
    homeState.startDelta = solarRegulator.settings.startDelta;

    bool offPeak = false;
    homeState.offPeakKnown = offPeakSchedule.isActive(offPeak);
    homeState.offPeak = offPeak;
}

void SolarInstallation::printHomeScreen(
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
        display.drawFastHLine(MARGIN, SEPARATOR_1_Y, RIGHT - MARGIN, COLOR_GREY);
        display.drawFastHLine(MARGIN, SEPARATOR_2_Y, RIGHT - MARGIN, COLOR_GREY);

        const struct
        {
            int16_t y;
            const char* label;
        } labels[] = {
            {COLLECTOR_Y, "Capteur"},
            {TANK_TOP_Y, "Ballon H"},
            {TANK_BOTTOM_Y, "Ballon B"},
            {DELTA_Y, "Ecart"},
            {PUMP_Y, "Pompe"},
            {HEATER_Y, "Appoint"}
        };

        for (const auto& row : labels)
        {
            TextField::print(
                display, MARGIN, row.y, 2, COLOR_WHITE, row.label, LABEL_CHARS);
        }
    }

    char text[24];

    // ----- En-tête : mode et heure -----

    TextField::print(
        display,
        MARGIN,
        HEADER_Y,
        2,
        homeState.holidayMode ? COLOR_CYAN : COLOR_WHITE,
        homeState.holidayMode ? "VACANCES" : "SOLAIRE",
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

    // ----- Températures et écart capteur - bas du ballon -----

    const MeasurementSample* collector =
        context.snapshot.find(collectorTemperature);
    const MeasurementSample* tankBottom =
        context.snapshot.find(tankBottomTemperature);

    printTemperature(display, COLLECTOR_Y, collector);
    printTemperature(
        display,
        TANK_TOP_Y,
        context.snapshot.find(tankTopTemperature));
    printTemperature(display, TANK_BOTTOM_Y, tankBottom);

    if (isUsable(collector) && isUsable(tankBottom))
    {
        const double_t delta = collector->value - tankBottom->value;

        snprintf(text, sizeof(text), "%+.1f K", delta);

        // Vert : écart suffisant pour démarrer la charge solaire.
        printValue(
            display,
            DELTA_Y,
            delta >= homeState.startDelta ? COLOR_GREEN : COLOR_WHITE,
            text);
    }
    else
    {
        printValue(display, DELTA_Y, COLOR_GREY, "--");
    }

    // ----- Pompe -----

    const OutputSample* pumpSample =
        context.snapshot.find(pumpRelay);

    if (pumpSample == nullptr || !pumpSample->healthy)
        printValue(display, PUMP_Y, COLOR_GREY, "--");
    else if (pumpSample->appliedCommand >= 0.5)
    {
        printValue(
            display,
            PUMP_Y,
            homeState.discharging ? COLOR_CYAN : COLOR_GREEN,
            homeState.discharging ? "DECHARGE" : "ON");
    }
    else if (pumpSample->waitingToStart())
        printValue(display, PUMP_Y, COLOR_ORANGE, "ATTENTE");
    else
        printValue(display, PUMP_Y, COLOR_GREY, "OFF");

    // ----- Appoint électrique -----

    const OutputSample* heaterSample =
        context.snapshot.find(heaterRelay);

    if (heaterSample == nullptr || !heaterSample->healthy)
        printValue(display, HEATER_Y, COLOR_GREY, "--");
    else if (heaterSample->appliedCommand >= 0.5)
        printValue(display, HEATER_Y, COLOR_ORANGE, "ON");
    else if (heaterSample->waitingToStart())
        printValue(display, HEATER_Y, COLOR_ORANGE, "ATTENTE");
    else if (homeState.offPeakKnown && !homeState.offPeak)
        printValue(display, HEATER_Y, COLOR_GREY, "HORS HC");
    else
        printValue(display, HEATER_Y, COLOR_GREY, "OFF");
}
