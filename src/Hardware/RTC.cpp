#include <Hardware/RTC.h>

#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

namespace
{
    constexpr uint8_t HOUR_12_MODE = 0x40;
    constexpr uint8_t HOUR_PM = 0x20;

    constexpr uint8_t ALARM_MASK = 0x80;
    constexpr uint8_t ALARM_DAY_OF_WEEK = 0x40;

    constexpr uint8_t CONTROL_EOSC = 0x80;
    constexpr uint8_t CONTROL_INTCN = 0x04;
    constexpr uint8_t CONTROL_A2IE = 0x02;
    constexpr uint8_t CONTROL_A1IE = 0x01;

    constexpr uint8_t STATUS_OSF = 0x80;
    constexpr uint8_t STATUS_EN32KHZ = 0x08;
    constexpr uint8_t STATUS_A2F = 0x02;
    constexpr uint8_t STATUS_A1F = 0x01;
}

bool RTC::begin(
    uint8_t sdaPin,
    uint8_t sclPin,
    TwoWire& wire)
{
    initialized = false;

    if (!wire.setSDA(sdaPin) ||
        !wire.setSCL(sclPin))
    {
        return false;
    }

    wire.begin();

    return begin(wire);
}

bool RTC::begin(TwoWire& wire)
{
    initialized = false;

    if (!device.begin(wire, Board::DS3231::ADDRESS) ||
        !device.updateRegister(
            DS3231::Register::Control,
            CONTROL_EOSC |
                CONTROL_INTCN |
                CONTROL_A2IE |
                CONTROL_A1IE,
            CONTROL_INTCN) ||
        !clearStatusFlags(
            STATUS_A2F |
                STATUS_A1F))
    {
        return false;
    }

    initialized = true;

    onMenuOpened();

    return true;
}

bool RTC::isInitialized() const
{
    return initialized;
}

bool RTC::readDateTime(DateTime& dateTime) const
{
    if (!initialized)
        return false;

    uint8_t registers[7] = {};

    if (!device.readRegisters(
            DS3231::Register::Seconds,
            registers,
            sizeof(registers)))
    {
        return false;
    }

    DateTime decoded;

    if (!fromBcd(
            registers[0] & 0x7F,
            59,
            decoded.second) ||
        !fromBcd(
            registers[1] & 0x7F,
            59,
            decoded.minute) ||
        !decodeHour(
            registers[2],
            decoded.hour) ||
        !fromBcd(
            registers[4] & 0x3F,
            31,
            decoded.day) ||
        (registers[5] & 0x80) != 0 ||
        !fromBcd(
            registers[5] & 0x1F,
            12,
            decoded.month))
    {
        return false;
    }

    uint8_t shortYear = 0;

    if (!fromBcd(
            registers[6],
            99,
            shortYear))
    {
        return false;
    }

    decoded.year = 2000 + shortYear;

    if (!isValidDateTime(decoded))
    {
        return false;
    }

    decoded.dayOfWeek =
        calculateDayOfWeek(
            decoded.year,
            decoded.month,
            decoded.day);

    dateTime = decoded;

    return true;
}

bool RTC::setDateTime(
    const DateTime& dateTime)
{
    if (!initialized ||
        !isValidDateTime(dateTime))
    {
        return false;
    }

    const uint8_t registers[7] = {
        toBcd(dateTime.second),
        toBcd(dateTime.minute),
        toBcd(dateTime.hour),
        calculateDayOfWeek(
            dateTime.year,
            dateTime.month,
            dateTime.day),
        toBcd(dateTime.day),
        toBcd(dateTime.month),
        toBcd(
            static_cast<uint8_t>(
                dateTime.year - 2000))
    };

    return
        device.writeRegisters(
            DS3231::Register::Seconds,
            registers,
            sizeof(registers)) &&
        clearOscillatorStopFlag();
}

bool RTC::setTime(
    uint8_t hour,
    uint8_t minute,
    uint8_t second)
{
    if (!initialized ||
        hour > 23 ||
        minute > 59 ||
        second > 59)
    {
        return false;
    }

    const uint8_t registers[3] = {
        toBcd(second),
        toBcd(minute),
        toBcd(hour)
    };

    return device.writeRegisters(
        DS3231::Register::Seconds,
        registers,
        sizeof(registers));
}

bool RTC::setDate(
    uint16_t year,
    uint8_t month,
    uint8_t day)
{
    if (!initialized ||
        !isValidDate(year, month, day))
    {
        return false;
    }

    const uint8_t registers[4] = {
        calculateDayOfWeek(
            year,
            month,
            day),
        toBcd(day),
        toBcd(month),
        toBcd(
            static_cast<uint8_t>(
                year - 2000))
    };

    return device.writeRegisters(
        DS3231::Register::DayOfWeek,
        registers,
        sizeof(registers));
}

bool RTC::isTimeValid(bool& valid) const
{
    if (!initialized)
        return false;

    uint8_t status = 0;

    if (!device.readRegister(
            DS3231::Register::Status,
            status))
    {
        return false;
    }

    valid = (status & STATUS_OSF) == 0;

    return true;
}

bool RTC::readTemperature(
    float& temperature) const
{
    if (!initialized)
        return false;

    uint8_t registers[2] = {};

    if (!device.readRegisters(
            DS3231::Register::TemperatureMsb,
            registers,
            sizeof(registers)))
    {
        return false;
    }

    int16_t integerPart = registers[0];

    if ((registers[0] & 0x80) != 0)
        integerPart -= 256;

    temperature =
        integerPart +
        static_cast<float>(registers[1] >> 6) *
            0.25f;

    return true;
}

bool RTC::setAlarm(const Alarm& alarm)
{
    if (!initialized ||
        !isValidAlarm(alarm))
    {
        return false;
    }

    if (alarm.mode == AlarmMode::DayOfWeek)
    {
        DateTime currentDateTime;

        if (!readDateTime(currentDateTime) ||
            !device.writeRegister(
                DS3231::Register::DayOfWeek,
                currentDateTime.dayOfWeek))
        {
            return false;
        }
    }

    if (!disableAlarm())
        return false;

    uint8_t dayDate = ALARM_MASK;

    if (alarm.mode == AlarmMode::DayOfMonth)
    {
        dayDate = toBcd(alarm.day);
    }
    else if (alarm.mode == AlarmMode::DayOfWeek)
    {
        dayDate =
            ALARM_DAY_OF_WEEK |
            alarm.day;
    }

    const uint8_t registers[4] = {
        toBcd(alarm.second),
        toBcd(alarm.minute),
        toBcd(alarm.hour),
        dayDate
    };

    if (!device.writeRegisters(
            DS3231::Register::Alarm1Seconds,
            registers,
            sizeof(registers)) ||
        !clearAlarm())
    {
        return false;
    }

    return device.updateRegister(
        DS3231::Register::Control,
        CONTROL_INTCN | CONTROL_A1IE,
        CONTROL_INTCN | CONTROL_A1IE);
}

bool RTC::setDailyAlarm(
    uint8_t hour,
    uint8_t minute,
    uint8_t second)
{
    Alarm alarm;
    alarm.mode = AlarmMode::Daily;
    alarm.hour = hour;
    alarm.minute = minute;
    alarm.second = second;

    return setAlarm(alarm);
}

bool RTC::disableAlarm()
{
    return initialized &&
           device.updateRegister(
               DS3231::Register::Control,
               CONTROL_A1IE,
               0);
}

bool RTC::clearAlarm()
{
    return initialized &&
           clearStatusFlags(STATUS_A1F);
}

bool RTC::isAlarmTriggered(
    bool& triggered) const
{
    if (!initialized)
        return false;

    uint8_t status = 0;

    if (!device.readRegister(
            DS3231::Register::Status,
            status))
    {
        return false;
    }

    triggered = (status & STATUS_A1F) != 0;

    return true;
}

void RTC::registerParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "miscellaneous",
        "Divers",
        MENU_OWNER_KEY,
        "Horloge",
        false
    });

    parameters.addInteger(
        "year",
        "Année",
        menuDateTime.year,
        2000,
        2099,
        1);

    parameters.addInteger(
        "month",
        "Mois",
        menuDateTime.month,
        1,
        12,
        1);

    parameters.addInteger(
        "day",
        "Jour",
        menuDateTime.day,
        1,
        31,
        1);

    parameters.addInteger(
        "hour",
        "Heure",
        menuDateTime.hour,
        0,
        23,
        1,
        "h");

    parameters.addInteger(
        "minute",
        "Minute",
        menuDateTime.minute,
        0,
        59,
        1,
        "min");

    parameters.addInteger(
        "second",
        "Seconde",
        menuDateTime.second,
        0,
        59,
        1,
        "s");
}

bool RTC::validateParameters(
    const ParameterEditor& editor) const
{
    const ParameterDraft* year =
        editor.find(MENU_OWNER_KEY, "year");

    const ParameterDraft* month =
        editor.find(MENU_OWNER_KEY, "month");

    const ParameterDraft* day =
        editor.find(MENU_OWNER_KEY, "day");

    const ParameterDraft* hour =
        editor.find(MENU_OWNER_KEY, "hour");

    const ParameterDraft* minute =
        editor.find(MENU_OWNER_KEY, "minute");

    const ParameterDraft* second =
        editor.find(MENU_OWNER_KEY, "second");

    if (year == nullptr &&
        month == nullptr &&
        day == nullptr &&
        hour == nullptr &&
        minute == nullptr &&
        second == nullptr)
    {
        return true;
    }

    if (year == nullptr ||
        month == nullptr ||
        day == nullptr ||
        hour == nullptr ||
        minute == nullptr ||
        second == nullptr)
    {
        return false;
    }

    const ParameterDraft* drafts[] = {
        year,
        month,
        day,
        hour,
        minute,
        second
    };

    for (const ParameterDraft* draft : drafts)
    {
        if (draft->parameter == nullptr ||
            draft->parameter->type !=
                Parameter::Type::Integer)
        {
            return false;
        }
    }

    if (year->integerValue < 2000 ||
        year->integerValue > 2099 ||
        month->integerValue < 1 ||
        month->integerValue > 12 ||
        day->integerValue < 1 ||
        day->integerValue > 31 ||
        hour->integerValue < 0 ||
        hour->integerValue > 23 ||
        minute->integerValue < 0 ||
        minute->integerValue > 59 ||
        second->integerValue < 0 ||
        second->integerValue > 59)
    {
        return false;
    }

    DateTime dateTime;
    dateTime.year =
        static_cast<uint16_t>(
            year->integerValue);
    dateTime.month =
        static_cast<uint8_t>(
            month->integerValue);
    dateTime.day =
        static_cast<uint8_t>(
            day->integerValue);
    dateTime.hour =
        static_cast<uint8_t>(
            hour->integerValue);
    dateTime.minute =
        static_cast<uint8_t>(
            minute->integerValue);
    dateTime.second =
        static_cast<uint8_t>(
            second->integerValue);

    return isValidDateTime(dateTime);
}

bool RTC::onMenuOpened()
{
    DateTime currentDateTime;

    if (!readDateTime(currentDateTime))
        return false;

    menuDateTime = currentDateTime;

    return true;
}

bool RTC::addMenuActions(
    MenuBuilder& menu) const
{
    const MenuBuilder::GroupId group =
        menu.findGroupForOwner(
            MENU_OWNER_KEY);

    if (group == MenuBuilder::INVALID_GROUP)
        return true;

    return menu.addAction(
        group,
        SET_DATE_TIME_ACTION,
        "rtc_set_date_time",
        "Mettre à jour");
}

bool RTC::handlesMenuAction(
    MenuBuilder::ActionId actionId) const
{
    return actionId == SET_DATE_TIME_ACTION;
}

bool RTC::executeMenuAction(
    MenuBuilder::ActionId actionId)
{
    if (!handlesMenuAction(actionId))
        return false;

    return setDateTime(menuDateTime);
}

bool RTC::isValidDateTime(
    const DateTime& dateTime)
{
    return
        isValidDate(
            dateTime.year,
            dateTime.month,
            dateTime.day) &&
        dateTime.hour <= 23 &&
        dateTime.minute <= 59 &&
        dateTime.second <= 59;
}

bool RTC::isValidDate(
    uint16_t year,
    uint8_t month,
    uint8_t day)
{
    if (year < 2000 ||
        year > 2099 ||
        month < 1 ||
        month > 12 ||
        day < 1)
    {
        return false;
    }

    constexpr uint8_t DAYS_PER_MONTH[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    uint8_t maximumDay =
        DAYS_PER_MONTH[month - 1];

    if (month == 2 &&
        isLeapYear(year))
    {
        maximumDay = 29;
    }

    return day <= maximumDay;
}

uint8_t RTC::calculateDayOfWeek(
    uint16_t year,
    uint8_t month,
    uint8_t day)
{
    if (!isValidDate(year, month, day))
        return 0;

    constexpr uint8_t MONTH_OFFSETS[] = {
        0, 3, 2, 5, 0, 3,
        5, 1, 4, 6, 2, 4
    };

    uint16_t adjustedYear = year;

    if (month < 3)
        adjustedYear--;

    const uint8_t sundayBased =
        (adjustedYear +
         adjustedYear / 4 -
         adjustedYear / 100 +
         adjustedYear / 400 +
         MONTH_OFFSETS[month - 1] +
         day) % 7;

    return sundayBased == 0
        ? 7
        : sundayBased;
}

uint8_t RTC::toBcd(uint8_t value)
{
    return
        static_cast<uint8_t>(
            ((value / 10) << 4) |
            (value % 10));
}

bool RTC::fromBcd(
    uint8_t value,
    uint8_t maximum,
    uint8_t& decoded)
{
    const uint8_t tens = value >> 4;
    const uint8_t units = value & 0x0F;

    if (tens > 9 || units > 9)
        return false;

    decoded = tens * 10 + units;

    return decoded <= maximum;
}

bool RTC::decodeHour(
    uint8_t value,
    uint8_t& hour)
{
    if ((value & HOUR_12_MODE) == 0)
    {
        return fromBcd(
            value & 0x3F,
            23,
            hour);
    }

    uint8_t hour12 = 0;

    if (!fromBcd(
            value & 0x1F,
            12,
            hour12) ||
        hour12 == 0)
    {
        return false;
    }

    hour = hour12 % 12;

    if ((value & HOUR_PM) != 0)
        hour += 12;

    return true;
}

bool RTC::isLeapYear(uint16_t year)
{
    return
        (year % 4 == 0) &&
        ((year % 100 != 0) ||
         (year % 400 == 0));
}

bool RTC::clearOscillatorStopFlag()
{
    return clearStatusFlags(STATUS_OSF);
}

bool RTC::clearStatusFlags(uint8_t flags)
{
    uint8_t status = 0;

    if (!device.readRegister(
            DS3231::Register::Status,
            status))
    {
        return false;
    }

    uint8_t value =
        (status & STATUS_EN32KHZ) |
        STATUS_OSF |
        STATUS_A2F |
        STATUS_A1F;

    value &= ~flags;

    return device.writeRegister(
        DS3231::Register::Status,
        value);
}

bool RTC::isValidAlarm(
    const Alarm& alarm) const
{
    if (alarm.hour > 23 ||
        alarm.minute > 59 ||
        alarm.second > 59)
    {
        return false;
    }

    switch (alarm.mode)
    {
    case AlarmMode::Daily:
        return true;

    case AlarmMode::DayOfMonth:
        return alarm.day >= 1 &&
               alarm.day <= 31;

    case AlarmMode::DayOfWeek:
        return alarm.day >= 1 &&
               alarm.day <= 7;

    default:
        return false;
    }
}
