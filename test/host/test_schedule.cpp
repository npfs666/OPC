#include "TestHarness.h"

#include <Hardware/RTC.h>
#include <Installation.h>
#include <Measurements/Temperature/Temperature.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <Regulator/PID.h>
#include <Regulator/ScheduledSetpoint.h>
#include <Regulator/Thermostat.h>
#include <Regulator/TimeSchedule.h>
#include <hmi/MenuBuilder.h>
#include <hmi/ParameterList.h>

#include <cstring>

namespace
{
    constexpr uint8_t MONDAY = 1;
    constexpr uint8_t FRIDAY = 5;
    constexpr uint8_t SATURDAY = 6;
    constexpr uint8_t SUNDAY = 7;

    constexpr uint16_t minutes(uint8_t hour, uint8_t minute = 0)
    {
        return static_cast<uint16_t>(hour * 60 + minute);
    }

    class FakeTemperature final :
        public Temperature
    {
    public:
        FakeTemperature()
        {
            Temperature::begin("temperature");
        }

        void setReading(
            double_t value,
            bool valid = true)
        {
            setValue(value);
            setValid(valid);
        }

        void update() override
        {
        }
    };

    /* Horloge pilotée par le test : seuls le jour et l'heure comptent. */
    void setClock(
        ClockSample& clock,
        uint8_t dayOfWeek,
        uint8_t hour,
        uint8_t minute = 0,
        bool valid = true)
    {
        clock.dateTime.dayOfWeek = dayOfWeek;
        clock.dateTime.hour = hour;
        clock.dateTime.minute = minute;
        clock.dateTime.second = 0;
        clock.valid = valid;
    }

    bool commandAt(
        TimeSchedule& schedule,
        ClockSample& clock,
        uint8_t dayOfWeek,
        uint8_t hour,
        uint8_t minute = 0)
    {
        setClock(clock, dayOfWeek, hour, minute);
        schedule.update(0);

        CHECK_TRUE(schedule.isCommandValid());

        return schedule.readCommand() >= 0.5;
    }

    class ScheduleMenuInstallation final :
        public Installation
    {
    public:
        const char* name() const override
        {
            return "Programmation";
        }

        const char* configurationKey() const override
        {
            return "schedule_test";
        }

        bool begin(
            SensorBoard&,
            Adafruit_BMP5xx&,
            ProcessControl&) override
        {
            return true;
        }

        void printHomeScreen(
            HomeScreenContext&) override
        {
        }

        bool registerSchedule()
        {
            parameterList.begin(
                parameterStorage,
                MAX_PARAMETERS);

            schedule.begin("prog", "Eclairage", clock);
            schedule.registerParameters(parameterList);

            return !parameterList.hasError();
        }

        bool registerOrphanSlot()
        {
            parameterList.begin(
                parameterStorage,
                MAX_PARAMETERS);

            ParameterOwner owner{
                "schedules",
                "Programmation",
                "orphan.p1",
                "Plage 1"
            };

            owner.parentOwnerKey = "orphan";

            return parameterList.forOwner(owner).addTime(
                "start",
                "Début",
                start);
        }

    private:
        ClockSample clock;
        TimeSchedule schedule;
        uint16_t start = 0;
    };

    void testScheduleSlots()
    {
        ClockSample clock;
        TimeSchedule schedule;
        schedule.begin("prog", "Programme", clock);

        // Par défaut : toutes les plages désactivées.
        CHECK_FALSE(commandAt(schedule, clock, MONDAY, 12));

        schedule.settings.slots[0] = {
            TimeSchedule::Days::Weekdays, minutes(6, 30), minutes(8)
        };

        // Début inclus, fin exclue.
        CHECK_FALSE(commandAt(schedule, clock, MONDAY, 6, 29));
        CHECK_TRUE(commandAt(schedule, clock, MONDAY, 6, 30));
        CHECK_TRUE(commandAt(schedule, clock, FRIDAY, 7, 59));
        CHECK_FALSE(commandAt(schedule, clock, FRIDAY, 8, 0));
        CHECK_FALSE(commandAt(schedule, clock, SATURDAY, 7, 0));

        // Passage de minuit : les jours sont ceux du début de la plage.
        schedule.settings.slots[1] = {
            TimeSchedule::Days::Weekdays, minutes(22), minutes(6)
        };

        CHECK_TRUE(commandAt(schedule, clock, FRIDAY, 23));
        CHECK_TRUE(commandAt(schedule, clock, SATURDAY, 5, 59));
        CHECK_FALSE(commandAt(schedule, clock, SATURDAY, 6, 0));
        CHECK_FALSE(commandAt(schedule, clock, SATURDAY, 23));
        CHECK_FALSE(commandAt(schedule, clock, MONDAY, 3));
        CHECK_TRUE(commandAt(schedule, clock, 2, 3));

        // Dimanche soir → lundi matin.
        schedule.settings.slots[1] = {
            TimeSchedule::Days::Sunday, minutes(23), minutes(1)
        };

        CHECK_TRUE(commandAt(schedule, clock, SUNDAY, 23, 30));
        CHECK_TRUE(commandAt(schedule, clock, MONDAY, 0, 30));
        CHECK_FALSE(commandAt(schedule, clock, SUNDAY, 0, 30));

        // Début égal à fin : journée entière.
        schedule.settings.slots[2] = {
            TimeSchedule::Days::Saturday, minutes(0), minutes(0)
        };

        CHECK_TRUE(commandAt(schedule, clock, SATURDAY, 0, 0));
        CHECK_TRUE(commandAt(schedule, clock, SATURDAY, 23, 59));
        CHECK_FALSE(commandAt(schedule, clock, SUNDAY, 12));

        // Week-end et jour précis.
        schedule.settings.slots[3] = {
            TimeSchedule::Days::Weekend, minutes(9), minutes(12)
        };
        schedule.settings.slots[4] = {
            TimeSchedule::Days::Wednesday, minutes(14), minutes(15)
        };

        CHECK_TRUE(commandAt(schedule, clock, SUNDAY, 10));
        CHECK_FALSE(commandAt(schedule, clock, FRIDAY, 10));
        CHECK_TRUE(commandAt(schedule, clock, 3, 14, 30));
        CHECK_FALSE(commandAt(schedule, clock, 4, 14, 30));

        // Une plage désactivée garde ses heures sans effet.
        schedule.settings.slots[4].days = TimeSchedule::Days::Off;
        CHECK_FALSE(commandAt(schedule, clock, 3, 14, 30));
    }

    void testScheduleModesAndClock()
    {
        ClockSample clock;
        TimeSchedule schedule;
        schedule.begin("prog", "Programme", clock);

        schedule.settings.slots[0] = {
            TimeSchedule::Days::Everyday, minutes(8), minutes(18)
        };

        bool active = true;

        // Auto sans heure valide : commande invalide, alerte requise.
        setClock(clock, MONDAY, 12, 0, false);
        schedule.update(0);
        CHECK_FALSE(schedule.isCommandValid());
        CHECK_FALSE(schedule.isActive(active));
        CHECK_TRUE(schedule.requiresClock());

        // Les modes forcés fonctionnent sans horloge.
        schedule.settings.mode = TimeSchedule::Mode::ForcedOn;
        schedule.update(0);
        CHECK_TRUE(schedule.isCommandValid());
        CHECK_NEAR(schedule.readCommand(), 1.0, 0.0);
        CHECK_TRUE(schedule.isActive(active));
        CHECK_TRUE(active);
        CHECK_FALSE(schedule.requiresClock());

        schedule.settings.mode = TimeSchedule::Mode::ForcedOff;
        setClock(clock, MONDAY, 12);
        schedule.update(0);
        CHECK_TRUE(schedule.isCommandValid());
        CHECK_NEAR(schedule.readCommand(), 0.0, 0.0);
        CHECK_FALSE(schedule.requiresClock());

        // Retour en Auto : l'heure courante s'applique directement.
        schedule.settings.mode = TimeSchedule::Mode::Auto;
        schedule.update(0);
        CHECK_NEAR(schedule.readCommand(), 1.0, 0.0);

        // Un begin() remet les réglages par défaut.
        schedule.begin("prog", "Programme", clock);
        CHECK_TRUE(schedule.settings.mode == TimeSchedule::Mode::Auto);
        CHECK_TRUE(
            schedule.settings.slots[0].days ==
                TimeSchedule::Days::Off);
    }

    void testScheduleParameters()
    {
        ClockSample clock;
        TimeSchedule schedule;
        schedule.begin("prog", "Eclairage", clock);

        Parameter storage[TimeSchedule::SLOT_COUNT * 3 + 1];
        ParameterList list;
        list.begin(storage, sizeof(storage) / sizeof(storage[0]));

        schedule.registerParameters(list);

        CHECK_FALSE(list.hasError());
        CHECK_TRUE(list.count() == TimeSchedule::SLOT_COUNT * 3 + 1);

        const Parameter* mode = list.find("prog", "mode");
        CHECK_TRUE(mode != nullptr);
        CHECK_TRUE(mode->parentOwnerKey == nullptr);
        CHECK_TRUE(std::strcmp(mode->categoryKey, "schedules") == 0);

        const Parameter* start = list.find("prog.p1", "start");
        CHECK_TRUE(start != nullptr);
        CHECK_TRUE(std::strcmp(start->parentOwnerKey, "prog") == 0);
        CHECK_TRUE(std::strcmp(start->ownerName, "Plage 1") == 0);
        CHECK_TRUE(start->type == Parameter::Type::Integer);
        CHECK_TRUE(
            start->data.integer.format ==
                Parameter::IntegerFormat::TimeOfDay);
        CHECK_TRUE(start->data.integer.minimum == 0);
        CHECK_TRUE(start->data.integer.maximum == 24 * 60 - 1);
        CHECK_TRUE(start->data.integer.step == 5);
        CHECK_TRUE(start->persistent);

        CHECK_TRUE(list.find("prog.p6", "end") != nullptr);
        CHECK_TRUE(list.find("prog.p7", "end") == nullptr);

        const Parameter* days = list.find("prog.p6", "days");
        CHECK_TRUE(days != nullptr);
        CHECK_TRUE(days->data.selection.count == 11);

        // Une heure lue ou écrite via le paramètre reste en minutes.
        schedule.settings.slots[0].start = minutes(6, 30);
        CHECK_TRUE(
            start->discrete.read(start->discrete.target) == minutes(6, 30));
        start->discrete.write(start->discrete.target, minutes(21, 45));
        CHECK_TRUE(schedule.settings.slots[0].start == minutes(21, 45));
    }

    void testTimeParameterValidation()
    {
        Parameter storage[4];
        ParameterList list;
        list.begin(storage, 4);

        auto parameters = list.forOwner({
            "schedules", "Programmation", "owner", "Owner"
        });

        uint16_t valid = minutes(23, 59);
        uint16_t outOfRange = 24 * 60;

        CHECK_TRUE(parameters.addTime("valid", "Valide", valid, 1));
        CHECK_FALSE(list.hasError());

        CHECK_FALSE(parameters.addTime("range", "Hors plage", outOfRange));
        CHECK_TRUE(list.hasError());

        list.begin(storage, 4);
        auto stepParameters = list.forOwner({
            "schedules", "Programmation", "owner", "Owner"
        });

        CHECK_FALSE(stepParameters.addTime("step0", "Pas nul", valid, 0));
        CHECK_FALSE(stepParameters.addTime("step61", "Pas 61", valid, 61));

        // Un propriétaire ne peut pas être son propre parent.
        list.begin(storage, 4);
        ParameterOwner self{
            "schedules", "Programmation", "loop", "Boucle"
        };
        self.parentOwnerKey = "loop";

        CHECK_FALSE(list.forOwner(self).addTime("start", "Début", valid));
    }

    void testScheduleMenuNesting()
    {
        ScheduleMenuInstallation installation;
        MenuBuilder menu;

        CHECK_TRUE(installation.registerSchedule());
        CHECK_TRUE(installation.buildMenu(menu));

        const MenuBuilder::GroupId schedules =
            menu.findSubmenu(menu.root(), "schedules");
        const MenuBuilder::GroupId program =
            menu.findGroupForOwner("prog");
        const MenuBuilder::GroupId slot1 =
            menu.findGroupForOwner("prog.p1");
        const MenuBuilder::GroupId slot6 =
            menu.findGroupForOwner("prog.p6");

        CHECK_TRUE(schedules != MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(program != MenuBuilder::INVALID_GROUP);
        CHECK_TRUE(menu.getGroup(program)->parent == schedules);
        CHECK_TRUE(menu.getGroup(slot1)->parent == program);
        CHECK_TRUE(menu.getGroup(slot6)->parent == program);
        CHECK_TRUE(std::strcmp(menu.getGroup(slot1)->name, "Plage 1") == 0);

        // Ordre de déclaration dans le programme : Mode, puis Plage 1 à 6.
        const char* expected[TimeSchedule::SLOT_COUNT + 1] = {
            "prog", "prog.p1", "prog.p2", "prog.p3",
            "prog.p4", "prog.p5", "prog.p6"
        };
        size_t found = 0;

        for (size_t i = 0; i < menu.entryCount(); i++)
        {
            const MenuBuilder::Entry* entry = menu.getEntry(i);
            CHECK_TRUE(entry != nullptr);

            const char* key = nullptr;

            if (entry->kind == MenuBuilder::Entry::Kind::Owner)
            {
                const MenuBuilder::OwnerBinding* binding =
                    menu.getOwnerBinding(entry->index);

                if (binding->group == program)
                    key = binding->ownerKey;
            }
            else if (menu.getGroup(entry->index)->parent == program)
            {
                key = menu.getGroup(entry->index)->key;
            }

            if (key == nullptr)
                continue;

            CHECK_TRUE(found < TimeSchedule::SLOT_COUNT + 1);

            if (found < TimeSchedule::SLOT_COUNT + 1)
                CHECK_TRUE(std::strcmp(key, expected[found]) == 0);

            found++;
        }

        CHECK_TRUE(found == TimeSchedule::SLOT_COUNT + 1);

        // Un parent inconnu (ou déclaré après) fait échouer le menu.
        ScheduleMenuInstallation orphan;
        MenuBuilder orphanMenu;

        CHECK_TRUE(orphan.registerOrphanSlot());
        CHECK_FALSE(orphan.buildMenu(orphanMenu));
    }

    void testScheduledThermostat()
    {
        FakeTemperature temperature;
        ClockSample clock;
        TimeSchedule schedule;
        Thermostat thermostat;

        schedule.begin("prog", "Chauffe", clock);
        schedule.settings.slots[0] = {
            TimeSchedule::Days::Everyday, minutes(8), minutes(18)
        };

        thermostat.begin("thermostat", "Thermostat", temperature);
        thermostat.settings.setpoint = 20.0;
        thermostat.settings.hysteresis = 2.0;

        // Sans programme : fonctionnement inchangé, pas de réglages ajoutés.
        {
            Parameter storage[8];
            ParameterList list;
            list.begin(storage, 8);
            thermostat.registerParameters(list);
            CHECK_TRUE(list.find("thermostat", "reduced_setpoint") == nullptr);
        }

        temperature.setReading(18.0);
        setClock(clock, MONDAY, 20);
        thermostat.update(0);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);
        CHECK_TRUE(
            thermostat.scheduledSetpoint.state() ==
                ScheduledSetpoint::State::Unscheduled);

        thermostat.setSchedule(schedule, 16.0);

        {
            Parameter storage[8];
            ParameterList list;
            list.begin(storage, 8);
            thermostat.registerParameters(list);
            CHECK_FALSE(list.hasError());
            CHECK_TRUE(list.find("thermostat", "reduced_setpoint") != nullptr);
            CHECK_TRUE(list.find("thermostat", "outside_schedule") != nullptr);
        }

        // Hors plage : consigne réduite 16 °C, 18 °C suffit.
        thermostat.update(1000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);
        CHECK_TRUE(
            thermostat.scheduledSetpoint.state() ==
                ScheduledSetpoint::State::Reduced);

        // Dans la plage : consigne normale 20 °C.
        setClock(clock, MONDAY, 12);
        thermostat.update(2000);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);
        CHECK_TRUE(
            thermostat.scheduledSetpoint.state() ==
                ScheduledSetpoint::State::Comfort);

        // Hors plage réglé sur Arrêt : arrêt programmé, pas un défaut.
        // Commande 0 valide, même sonde en défaut.
        thermostat.scheduledSetpoint.settings.outside =
            ScheduledSetpoint::Outside::Off;
        setClock(clock, MONDAY, 20);
        thermostat.update(3000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);
        CHECK_TRUE(
            thermostat.scheduledSetpoint.state() ==
                ScheduledSetpoint::State::Off);

        temperature.setReading(NAN, false);
        thermostat.update(3500);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);
        temperature.setReading(18.0);

        // Dérogation : marche forcée du programme.
        schedule.settings.mode = TimeSchedule::Mode::ForcedOn;
        thermostat.update(4000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        // Heure inconnue en Auto : régulation arrêtée.
        schedule.settings.mode = TimeSchedule::Mode::Auto;
        setClock(clock, MONDAY, 12, 0, false);
        thermostat.update(5000);
        CHECK_FALSE(thermostat.isCommandValid());
        CHECK_TRUE(
            thermostat.scheduledSetpoint.state() ==
                ScheduledSetpoint::State::ClockInvalid);

        // Un nouveau begin() détache le programme.
        thermostat.begin("thermostat", "Thermostat", temperature);
        CHECK_FALSE(thermostat.scheduledSetpoint.isAttached());
    }

    void testScheduledPID()
    {
        FakeTemperature measurement;
        ClockSample clock;
        TimeSchedule schedule;
        PID pid;

        schedule.begin("prog", "Chauffe", clock);
        schedule.settings.slots[0] = {
            TimeSchedule::Days::Everyday, minutes(8), minutes(18)
        };

        pid.begin("pid", measurement);
        pid.settings.setpoint = 10.0;
        pid.settings.kp = 0.1;
        pid.settings.ti = 0.0;
        pid.settings.td = 0.0;
        pid.setSchedule(schedule, 5.0);

        {
            Parameter storage[16];
            ParameterList list;
            list.begin(storage, 16);
            pid.registerParameters(list);
            CHECK_FALSE(list.hasError());
            CHECK_TRUE(list.find("pid", "reduced_setpoint") != nullptr);
        }

        measurement.setReading(5.0);

        // Plage : consigne 10, écart 5 → 0,5 après la première mesure.
        setClock(clock, MONDAY, 12);
        pid.update(0);
        pid.update(1000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.5, 0.0001);

        // Hors plage : consigne réduite 5, écart nul.
        setClock(clock, MONDAY, 20);
        pid.update(2000);
        CHECK_NEAR(pid.readCommand(), 0.0, 0.0001);

        // Arrêt hors plage : arrêt programmé (commande 0 valide, pas un
        // défaut), puis reprise sans mémoire.
        pid.scheduledSetpoint.settings.outside =
            ScheduledSetpoint::Outside::Off;
        pid.update(3000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.0, 0.0);

        setClock(clock, MONDAY, 12);
        pid.update(4000);
        CHECK_FALSE(pid.isCommandValid());
        pid.update(5000);
        CHECK_NEAR(pid.readCommand(), 0.5, 0.0001);

        // Heure inconnue : régulation arrêtée.
        setClock(clock, MONDAY, 12, 0, false);
        pid.update(6000);
        CHECK_FALSE(pid.isCommandValid());
        CHECK_TRUE(
            pid.scheduledSetpoint.state() ==
                ScheduledSetpoint::State::ClockInvalid);
    }

    void testPIDAutotuneIgnoresSchedule()
    {
        FakeTemperature measurement;
        ClockSample clock;
        TimeSchedule schedule;
        PID pid;

        schedule.begin("prog", "Chauffe", clock);

        pid.begin("pid", measurement);
        pid.settings.setpoint = 10.0;
        pid.autoTuneSettings.outputLow = 0.0;
        pid.autoTuneSettings.outputHigh = 1.0;
        pid.autoTuneSettings.noiseBand = 0.5;
        pid.autoTuneSettings.inputMin = 0.0;
        pid.autoTuneSettings.inputMax = 20.0;
        pid.autoTuneSettings.timeoutSeconds = 300;
        pid.autoTuneSettings.cycles = 2;

        // Programme sans plage, arrêt hors plage et heure inconnue :
        // la régulation normale serait arrêtée.
        pid.setSchedule(schedule, 5.0);
        pid.scheduledSetpoint.settings.outside =
            ScheduledSetpoint::Outside::Off;
        setClock(clock, MONDAY, 12, 0, false);

        CHECK_TRUE(pid.startAutoTune(0));

        measurement.setReading(10.0);
        pid.update(0);

        CHECK_TRUE(
            pid.getAutoTuneStatus() ==
                PID::AutoTuneStatus::Running);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 1.0, 0.0);

        measurement.setReading(10.6);
        pid.update(1000);
        CHECK_TRUE(pid.isAutoTuneActive());
        CHECK_TRUE(pid.isCommandValid());
    }

    void testProcessClockRequirement()
    {
        ProcessControl process;
        TimeSchedule schedule;
        ProcessSnapshot snapshot;

        schedule.begin("prog", "Programme", process.clock());
        schedule.settings.slots[0] = {
            TimeSchedule::Days::Everyday, minutes(8), minutes(18)
        };

        CHECK_TRUE(process.add(schedule));

        // Heure transmise par ProcessControl au programme.
        ClockSample sample;
        setClock(sample, MONDAY, 12);
        process.updateClock(sample);
        schedule.update(0);
        CHECK_NEAR(schedule.readCommand(), 1.0, 0.0);

        process.captureInputSnapshot(snapshot);
        CHECK_TRUE(snapshot.clock().valid);
        CHECK_TRUE(snapshot.clock().dateTime.hour == 12);
        CHECK_TRUE(snapshot.clockRequired());

        // Heure perdue : l'écran d'alerte doit s'afficher.
        sample.valid = false;
        process.updateClock(sample);
        process.captureInputSnapshot(snapshot);
        CHECK_FALSE(snapshot.clock().valid);
        CHECK_TRUE(snapshot.clockRequired());

        // En mode forcé, l'heure n'est plus nécessaire.
        schedule.settings.mode = TimeSchedule::Mode::ForcedOn;
        process.captureInputSnapshot(snapshot);
        CHECK_FALSE(snapshot.clockRequired());

        // Sans programme, jamais d'alerte.
        ProcessControl withoutSchedule;
        ProcessSnapshot otherSnapshot;
        withoutSchedule.captureInputSnapshot(otherSnapshot);
        CHECK_FALSE(otherSnapshot.clockRequired());
    }
}

void runScheduleTests()
{
    TestHarness::run(
        "programme horaire : plages, jours et minuit",
        testScheduleSlots);

    TestHarness::run(
        "programme horaire : modes forcés et heure inconnue",
        testScheduleModesAndClock);

    TestHarness::run(
        "programme horaire : paramètres",
        testScheduleParameters);

    TestHarness::run(
        "paramètre heure HH:MM et parent de menu",
        testTimeParameterValidation);

    TestHarness::run(
        "programme horaire : sous-menus imbriqués et ordre",
        testScheduleMenuNesting);

    TestHarness::run(
        "thermostat programmé",
        testScheduledThermostat);

    TestHarness::run(
        "PID programmé",
        testScheduledPID);

    TestHarness::run(
        "autotune PID ignore le programme",
        testPIDAutotuneIgnoresSchedule);

    TestHarness::run(
        "alerte horloge : heure requise",
        testProcessClockRequirement);
}
