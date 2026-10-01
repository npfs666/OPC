#include <Templates/ScheduleInstallation.h>

#include <Adafruit_GFX.h>
#include <Hardware/SensorBoard.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <hmi/HomeScreen.h>

#include <cstdio>

namespace
{
    constexpr uint16_t BLACK = 0x0000;
    constexpr uint16_t WHITE = 0xFFFF;
    constexpr uint16_t CYAN = 0x07FF;
    constexpr uint16_t GREEN = 0x07E0;
    constexpr uint16_t GREY = 0x8410;
    constexpr uint16_t ORANGE = 0xFD20;

    constexpr int16_t MARGIN = 12;
    constexpr int16_t DATE_Y = 44;
    constexpr int16_t TIME_Y = 68;
    constexpr int16_t CHANNEL_Y = 136;
    constexpr int16_t CHANNEL_HEIGHT = 40;

    constexpr const char* DAY_NAMES[] = {
        "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"
    };
}

const char* ScheduleInstallation::name() const
{
    return "Programmation horaire";
}

const char* ScheduleInstallation::configurationKey() const
{
    return "schedule_installation";
}

bool ScheduleInstallation::begin(
    SensorBoard& board,
    Adafruit_BMP5xx& bmp580,
    ProcessControl& process)
{
    (void)bmp580;

    const char* scheduleKeys[] = {"prog1", "prog2"};
    const char* scheduleNames[] = {"Programme 1", "Programme 2"};
    const char* actuatorKeys[] = {"prog1_cmd", "prog2_cmd"};
    const char* relayKeys[] = {"prog1_relay", "prog2_relay"};
    const char* relayNames[] = {"Relais 1", "Relais 2"};
    const uint8_t relayPins[] = {
        Board::Rp2040::OUTPUT_1, Board::Rp2040::OUTPUT_2
    };

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        schedules[i].begin(
            scheduleKeys[i], scheduleNames[i], process.clock());
        actuators[i].begin(
            actuatorKeys[i], scheduleNames[i], schedules[i]);

        // Actif à HIGH, état sûr OFF (heure inconnue, défaut...).
        relays[i].begin(
            relayKeys[i], relayNames[i], relayPins[i], true, false);

        if (!process.add(schedules[i]) ||
            !process.add(actuators[i]) ||
            !process.connect(actuators[i], relays[i]))
        {
            return fail("Programmation non reliée");
        }
    }

    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres invalides");

    return true;
}

void ScheduleInstallation::captureHomeScreenState()
{
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        homeModes[i] = schedules[i].settings.mode;
}

void ScheduleInstallation::printHomeScreen(
    HomeScreenContext& context)
{
    Adafruit_GFX& display = context.display;

    display.cp437(true);
    display.setTextWrap(false);
    display.setTextSize(2);

    if (context.fullRefresh)
    {
        display.fillScreen(BLACK);

        display.setTextColor(CYAN, BLACK);
        display.setCursor(MARGIN, 8);
        display.print("Programmation");

        display.drawFastHLine(0, 30, display.width(), GREY);
    }

    // ----- Date et heure -----

    const ClockSample& clock = context.snapshot.clock();
    char text[24];

    if (clock.valid &&
        clock.dateTime.dayOfWeek >= 1 &&
        clock.dateTime.dayOfWeek <= 7)
    {
        snprintf(
            text, sizeof(text), "%s %02u/%02u/%04u ",
            DAY_NAMES[clock.dateTime.dayOfWeek - 1],
            clock.dateTime.day,
            clock.dateTime.month,
            clock.dateTime.year);
    }
    else
    {
        snprintf(text, sizeof(text), "Heure a regler ");
    }

    display.setTextColor(WHITE, BLACK);
    display.setCursor(MARGIN, DATE_Y);
    display.print(text);

    if (clock.valid)
    {
        snprintf(
            text, sizeof(text), "%02u:%02u",
            clock.dateTime.hour,
            clock.dateTime.minute);
    }
    else
    {
        snprintf(text, sizeof(text), "--:--");
    }

    display.setTextSize(4);
    display.setCursor(MARGIN, TIME_Y);
    display.print(text);
    display.setTextSize(2);

    // ----- Sorties -----

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        const int16_t y = CHANNEL_Y + i * CHANNEL_HEIGHT;
        const OutputSample* relay = context.snapshot.find(relays[i]);

        const bool valid = relay != nullptr && relay->healthy;
        const bool active = valid && relay->appliedCommand >= 0.5;
        const bool waiting = valid && relay->waitingToStart();

        display.setCursor(MARGIN, y);
        display.setTextColor(WHITE, BLACK);
        display.print(relays[i].getName());

        display.setCursor(120, y);
        display.setTextColor(
            active ? GREEN : waiting ? ORANGE : GREY,
            BLACK);
        display.print(
            !valid ? "--  " : active ? "ON  " : waiting ? "ATT." : "OFF ");

        // Rappel d'une dérogation manuelle en cours.
        display.setCursor(MARGIN, y + 18);
        display.setTextSize(1);
        display.setTextColor(GREY, BLACK);

        switch (homeModes[i])
        {
        case TimeSchedule::Mode::ForcedOn:
            display.print("Marche forcee");
            break;

        case TimeSchedule::Mode::ForcedOff:
            display.print("Arret force  ");
            break;

        default:
            display.print("Auto         ");
            break;
        }

        display.setTextSize(2);
    }
}
