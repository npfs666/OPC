#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <Configurable.h>
#include <Drivers/DS3231.h>
#include <hmi/MenuBuilder.h>

class ParameterEditor;
class ParameterList;

/**
 * Horloge DS3231.
 *
 * Le DS3231 contient l'heure UTC. readDateTime() et setDateTime() travaillent
 * en heure locale : le fuseau et l'heure d'été (timeZone) sont appliqués à
 * chaque accès, sans jamais réécrire le DS3231 au changement d'heure.
 */
class RTC : public Configurable
{
public:
    static constexpr const char* MENU_OWNER_KEY = "rtc.clock";
    static constexpr const char* TIME_ZONE_OWNER_KEY = "rtc.timezone";

    enum class DstRule : uint8_t
    {
        None,
        // Dernier dimanche de mars 01:00 UTC -> dernier dimanche d'octobre 01:00 UTC.
        Europe
    };

    struct TimeZoneSettings
    {
        /* Décalage de l'heure d'hiver sur UTC. France : +1 h. */
        double_t utcOffsetHours = 1.0;
        DstRule dst = DstRule::Europe;
    };

    TimeZoneSettings timeZone;

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

        /* Renseigné par readDateTime() : heure d'été en cours. */
        bool summerTime = false;
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

    /** Heure locale (fuseau et heure d'été appliqués). */
    bool readDateTime(DateTime& dateTime) const;

    /**
     * Écrit une heure locale. Pendant l'heure répétée d'octobre, l'heure
     * d'été est retenue ; une heure sautée en mars est décalée d'une heure.
     */
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
     * Les alarmes utilisent l'heure interne du DS3231, donc UTC.
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

    /** Valide et écrit uniquement les champs de l'horloge modifiés. */
    bool applyMenuParameters(ParameterEditor& editor);

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
    bool readDeviceDateTime(DateTime& utc) const;
    bool writeDeviceDateTime(const DateTime& utc);

    bool utcToLocal(
        const DateTime& utc,
        DateTime& local) const;

    bool localToUtc(
        const DateTime& local,
        DateTime& utc) const;

    int32_t standardOffsetSeconds() const;

    bool isSummerTime(int64_t utcSeconds) const;

    bool readMenuDateTime(
        const ParameterEditor& editor,
        DateTime& dateTime) const;

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

/** Dernière heure lue sur le DS3231, utilisée par la régulation. */
struct ClockSample
{
    RTC::DateTime dateTime;

    /* false si la lecture a échoué ou si l'oscillateur s'est arrêté (OSF). */
    bool valid = false;
};

#endif
