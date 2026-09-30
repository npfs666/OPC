#ifndef SCHEDULE_INSTALLATION_H
#define SCHEDULE_INSTALLATION_H

#include <Installation.h>

#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>
#include <Regulator/TimeSchedule.h>

/**
 * Programmation horaire : relais 1 et relais 2, chacun avec son programme
 * hebdomadaire (6 plages). Aucune entrée analogique n'est nécessaire.
 *
 * Menu : Programmation > Programme 1 > Mode, Plage 1...6
 */
class ScheduleInstallation final : public Installation
{
public:
    const char* name() const override;
    const char* configurationKey() const override;

    bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) override;

    void captureHomeScreenState() override;

    void printHomeScreen(
        HomeScreenContext& context) override;

private:
    static constexpr uint8_t CHANNEL_COUNT = 2;

    // Copie pour le cœur UI, faite par captureHomeScreenState().
    TimeSchedule::Mode homeModes[CHANNEL_COUNT] = {};

    TimeSchedule schedules[CHANNEL_COUNT];
    ActuatorOnOff actuators[CHANNEL_COUNT];
    RelayOutput relays[CHANNEL_COUNT];
};

#endif
