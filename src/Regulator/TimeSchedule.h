// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIME_SCHEDULE_H
#define TIME_SCHEDULE_H

#include <Regulator/Regulator.h>

struct ClockSample;

/**
 * Programmation horaire hebdomadaire, cadencée par le DS3231.
 *
 * Commande 1 pendant une plage active, 0 sinon. Elle pilote soit directement
 * un actionneur, soit la consigne d'un Thermostat ou d'un PID (setSchedule()).
 *
 * L'état est recalculé à chaque cycle depuis l'heure courante : après une
 * coupure ou un changement d'heure, la sortie reprend directement le bon état,
 * aucune commutation n'est ratée.
 *
 * Sans heure valide (DS3231 absent ou heure perdue), le mode Auto invalide la
 * commande et les sorties passent en état sûr. Les modes forcés n'ont pas
 * besoin de l'horloge.
 */
class TimeSchedule : public Regulator
{
public:
    static constexpr uint8_t SLOT_COUNT = 6;

    enum class Mode : uint8_t
    {
        Auto,
        ForcedOn,
        ForcedOff
    };

    enum class Days : uint8_t
    {
        Off,
        Everyday,
        Weekdays,
        Weekend,
        Monday,
        Tuesday,
        Wednesday,
        Thursday,
        Friday,
        Saturday,
        Sunday
    };

    /**
     * Heures en minutes depuis minuit : début inclus, fin exclue.
     *
     * - fin avant début : la plage passe minuit, les jours choisis sont ceux
     *   du début (Lun-Ven 22:00 → 06:00 finit le samedi à 06:00) ;
     * - début égal à fin : journée entière.
     */
    struct Slot
    {
        Days days = Days::Off;
        uint16_t start = 8 * 60;
        uint16_t end = 18 * 60;

        /* dayOfWeek : lundi = 1, dimanche = 7 (RTC::DateTime). */
        bool contains(
            uint8_t dayOfWeek,
            uint16_t minute) const;
    };

    struct Settings
    {
        Mode mode = Mode::Auto;
        Slot slots[SLOT_COUNT];
    };

    Settings settings;

    TimeSchedule();

    void begin(
        const char* name,
        const ClockSample& clock);

    /**
     * key sert aussi de préfixe aux plages ("<key>.p1"...) :
     * la garder courte (moins de 36 caractères).
     */
    void begin(
        const char* key,
        const char* name,
        const ClockSample& clock);

    void update(uint32_t now) override;

    /**
     * État du programme à l'instant présent.
     * Retourne false en mode Auto si l'heure est inconnue.
     */
    bool isActive(bool& active) const;

    /** L'heure n'est indispensable qu'en mode Auto. */
    bool requiresClock() const override;

    /** Marche ou Arrêt forcés (dérogation) : prioritaires sur l'inhibition. */
    bool isManual() const override;

    void print(Stream& stream) const override;

    void registerParameters(ParameterList& list) override;

private:
    static constexpr size_t SLOT_KEY_LENGTH = 40;

    static bool matches(
        Days days,
        uint8_t dayOfWeek);

    const ClockSample* clock = nullptr;
    char slotKeys[SLOT_COUNT][SLOT_KEY_LENGTH] = {};
};

#endif
