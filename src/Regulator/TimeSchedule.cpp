// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Regulator/TimeSchedule.h>

#include <Arduino.h>
#include <Hardware/RTC.h>
#include <hmi/ParameterList.h>

#include <cstdio>
#include <cstring>

namespace
{
    constexpr const char* CATEGORY_KEY = "schedules";
    constexpr const char* CATEGORY_NAME = "Programmation";

    constexpr uint8_t SUNDAY = 7;

    constexpr ParameterOption MODE_OPTIONS[] = {
        {
            static_cast<int32_t>(TimeSchedule::Mode::Auto),
            "Auto"
        },
        {
            static_cast<int32_t>(TimeSchedule::Mode::ForcedOn),
            "Marche forcée"
        },
        {
            static_cast<int32_t>(TimeSchedule::Mode::ForcedOff),
            "Arrêt forcé"
        }
    };

    constexpr ParameterOption DAYS_OPTIONS[] = {
        {static_cast<int32_t>(TimeSchedule::Days::Off), "Désactivée"},
        {static_cast<int32_t>(TimeSchedule::Days::Everyday), "Chaque jour"},
        {static_cast<int32_t>(TimeSchedule::Days::Weekdays), "Lun-Ven"},
        {static_cast<int32_t>(TimeSchedule::Days::Weekend), "Sam-Dim"},
        {static_cast<int32_t>(TimeSchedule::Days::Monday), "Lundi"},
        {static_cast<int32_t>(TimeSchedule::Days::Tuesday), "Mardi"},
        {static_cast<int32_t>(TimeSchedule::Days::Wednesday), "Mercredi"},
        {static_cast<int32_t>(TimeSchedule::Days::Thursday), "Jeudi"},
        {static_cast<int32_t>(TimeSchedule::Days::Friday), "Vendredi"},
        {static_cast<int32_t>(TimeSchedule::Days::Saturday), "Samedi"},
        {static_cast<int32_t>(TimeSchedule::Days::Sunday), "Dimanche"}
    };

    // Pointés par ParameterList : doivent rester valides.
    constexpr const char* SLOT_NAMES[] = {
        "Plage 1",
        "Plage 2",
        "Plage 3",
        "Plage 4",
        "Plage 5",
        "Plage 6"
    };

    static_assert(
        sizeof(SLOT_NAMES) / sizeof(SLOT_NAMES[0]) ==
            TimeSchedule::SLOT_COUNT,
        "SLOT_NAMES doit contenir un nom par plage");
}

TimeSchedule::TimeSchedule()
{
}

void TimeSchedule::begin(
    const char* name,
    const ClockSample& clock)
{
    begin(name, name, clock);
}

void TimeSchedule::begin(
    const char* key,
    const char* name,
    const ClockSample& clock)
{
    Regulator::begin(key, name);

    this->clock = &clock;

    settings = Settings{};

    for (uint8_t i = 0; i < SLOT_COUNT; i++)
    {
        snprintf(
            slotKeys[i],
            SLOT_KEY_LENGTH,
            "%s.p%u",
            getConfigurationKey(),
            static_cast<unsigned>(i + 1));
    }
}

bool TimeSchedule::matches(
    Days days,
    uint8_t dayOfWeek)
{
    switch (days)
    {
    case Days::Off:
        return false;

    case Days::Everyday:
        return true;

    case Days::Weekdays:
        return dayOfWeek >= 1 && dayOfWeek <= 5;

    case Days::Weekend:
        return dayOfWeek == 6 || dayOfWeek == SUNDAY;

    default:
        return dayOfWeek ==
            static_cast<uint8_t>(days) -
            static_cast<uint8_t>(Days::Monday) + 1;
    }
}

bool TimeSchedule::Slot::contains(
    uint8_t dayOfWeek,
    uint16_t minute) const
{
    if (start == end)
        return matches(days, dayOfWeek);

    if (start < end)
    {
        return matches(days, dayOfWeek) &&
               minute >= start &&
               minute < end;
    }

    // La partie après minuit appartient à la veille.
    const uint8_t previousDay =
        dayOfWeek == 1 ? SUNDAY : dayOfWeek - 1;

    return (matches(days, dayOfWeek) && minute >= start) ||
           (matches(days, previousDay) && minute < end);
}

void TimeSchedule::update(uint32_t now)
{
    (void)now;

    bool active = false;

    if (isActive(active))
        writeCommand(active ? 1.0 : 0.0);
    else
        invalidateCommand();
}

bool TimeSchedule::isManual() const
{
    return settings.mode != Mode::Auto;
}

bool TimeSchedule::isActive(bool& active) const
{
    switch (settings.mode)
    {
    case Mode::ForcedOn:
        active = true;
        return true;

    case Mode::ForcedOff:
        active = false;
        return true;

    case Mode::Auto:
        break;

    default:
        return false;
    }

    if (clock == nullptr || !clock->valid)
        return false;

    const uint16_t minute =
        clock->dateTime.hour * 60 +
        clock->dateTime.minute;

    active = false;

    for (const Slot& slot : settings.slots)
    {
        if (slot.contains(
                clock->dateTime.dayOfWeek,
                minute))
        {
            active = true;
            break;
        }
    }

    return true;
}

bool TimeSchedule::requiresClock() const
{
    return settings.mode == Mode::Auto;
}

void TimeSchedule::print(Stream& stream) const
{
    stream.print(getName());

    uint8_t len = strlen(getName());
    while (len++ < 16)
        stream.print(' ');

    stream.print(": ");

    if (!commandValid)
        stream.print("--");
    else
        stream.print(command >= 0.5 ? "On" : "Off");

    stream.print(" | ");
    stream.print(
        MODE_OPTIONS[static_cast<uint8_t>(settings.mode) % 3].name);

    if (settings.mode == Mode::Auto &&
        (clock == nullptr || !clock->valid))
    {
        stream.print(" | Heure invalide");
    }

    stream.println(' ');
}

void TimeSchedule::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        CATEGORY_KEY,
        CATEGORY_NAME,
        getConfigurationKey(),
        getName()
    });

    parameters.addSelection(
        "mode",
        "Mode",
        settings.mode,
        MODE_OPTIONS);

    for (uint8_t i = 0; i < SLOT_COUNT; i++)
    {
        ParameterOwner owner{
            CATEGORY_KEY,
            CATEGORY_NAME,
            slotKeys[i],
            SLOT_NAMES[i]
        };

        owner.parentOwnerKey = getConfigurationKey();

        auto slotParameters = list.forOwner(owner);
        Slot& slot = settings.slots[i];

        slotParameters.addSelection(
            "days",
            "Jours",
            slot.days,
            DAYS_OPTIONS);

        slotParameters.addTime(
            "start",
            "Début",
            slot.start);

        slotParameters.addTime(
            "end",
            "Fin",
            slot.end);
    }
}
