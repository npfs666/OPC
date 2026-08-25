#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <Configurable.h>
#include <Drivers/DS3231.h>
#include <hmi/MenuBuilder.h>

class ParameterEditor;
class ParameterList;

class RTC : public Configurable
{
public:
    /**
     * L'heure est toujours exposée de 0 à 23 et toujours écrite en 24 h.
     * dayOfWeek est calculé à la lecture : lundi = 1, dimanche = 7.
     */
    struct DateTime
    {
        /* L'interface accepte volontairement les années 2000 à 2099. */
        uint16_t year = 2000;
        uint8_t month = 1;
        uint8_t day = 1;
        uint8_t dayOfWeek = 6;
        uint8_t hour = 0;
        uint8_t minute = 0;
        uint8_t second = 0;
    };

    enum class AlarmMode : uint8_t
    {
        Daily,
        DayOfMonth,
        DayOfWeek
    };

    struct Alarm
    {
        AlarmMode mode = AlarmMode::Daily;
        uint8_t day = 1;
        uint8_t hour = 0;
        uint8_t minute = 0;
        uint8_t second = 0;
    };

    /**
     * Configure puis démarre le bus I2C avant d'initialiser le DS3231.
     * Cette surcharge doit être appelée avant tout autre begin() sur ce bus.
     */
    bool begin(
        uint8_t sdaPin,
        uint8_t sclPin,
        TwoWire& wire = Wire);

    /**
     * Initialise le DS3231 sur un bus I2C déjà démarré à 400 kHz maximum.
     * Les interruptions et flags d'alarme hérités sont remis à zéro.
     */
    bool begin(TwoWire& wire);

    bool isInitialized() const;

    bool readDateTime(DateTime& dateTime) const;
    bool setDateTime(const DateTime& dateTime);

    bool setTime(
        uint8_t hour,
        uint8_t minute,
        uint8_t second);

    bool setDate(
        uint16_t year,
        uint8_t month,
        uint8_t day);

    /**
     * Retourne false si le registre d'état n'a pas pu être lu.
     * valid reflète le témoin matériel OSF ; readDateTime() valide ensuite
     * séparément le contenu BCD et le calendrier.
     * Seul setDateTime() acquitte l'indicateur de perte d'heure OSF.
     */
    bool isTimeValid(bool& valid) const;

    bool readTemperature(float& temperature) const;

    /**
     * Configure l'alarme 1. Une alarme par date se répète chaque mois.
     */
    bool setAlarm(const Alarm& alarm);

    bool setDailyAlarm(
        uint8_t hour,
        uint8_t minute,
        uint8_t second);

    /* Toutes les fonctions d'alarme utilisent l'alarme 1 du DS3231. */
    bool disableAlarm();
    bool clearAlarm();
    bool isAlarmTriggered(bool& triggered) const;

    void registerParameters(ParameterList& list) override;

    bool validateParameters(
        const ParameterEditor& editor) const override;

    /** Recharge les champs du menu depuis le DS3231. */
    bool onMenuOpened();

    bool addMenuActions(MenuBuilder& menu) const;

    bool handlesMenuAction(
        MenuBuilder::ActionId actionId) const;

    bool executeMenuAction(
        MenuBuilder::ActionId actionId);

    static bool isValidDateTime(
        const DateTime& dateTime);

    static bool isValidDate(
        uint16_t year,
        uint8_t month,
        uint8_t day);

    static uint8_t calculateDayOfWeek(
        uint16_t year,
        uint8_t month,
        uint8_t day);

private:
    static constexpr const char* MENU_OWNER_KEY =
        "rtc.clock";

    static constexpr MenuBuilder::ActionId
        SET_DATE_TIME_ACTION = 48;

    static uint8_t toBcd(uint8_t value);

    static bool fromBcd(
        uint8_t value,
        uint8_t maximum,
        uint8_t& decoded);

    static bool decodeHour(
        uint8_t value,
        uint8_t& hour);

    static bool isLeapYear(uint16_t year);

    bool clearOscillatorStopFlag();
    bool clearStatusFlags(uint8_t flags);
    bool isValidAlarm(const Alarm& alarm) const;

    DS3231 device;
    DateTime menuDateTime;
    bool initialized = false;
};

#endif
