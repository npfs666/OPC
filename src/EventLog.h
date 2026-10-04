#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <Arduino.h>
#include <Hardware/RTC.h>

#include <cstddef>
#include <cstdint>

enum class EventKind : uint8_t
{
    Info,
    Fault,
    Alarm,
    Restart
};

struct EventEntry
{
    uint32_t uptimeMs = 0;          // millis() du dernier événement
    RTC::DateTime time;             // heure locale de la première occurrence
    bool timeValid = false;
    EventKind kind = EventKind::Info;

    // Conservé dans /events.csv (défauts, alarmes, redémarrages...).
    bool important = false;

    // Occurrences regroupées (rafale d'un même événement).
    uint16_t repeats = 1;

    char text[44] = {};
};

/**
 * Journal des événements, en RAM (cœur contrôle, sous processDataMutex).
 *
 * Anneau des CAPACITY derniers événements, horodatés à l'heure locale. Un
 * événement identique à l'un des COALESCE_DEPTH derniers, moins de
 * COALESCE_WINDOW_MS après lui, est regroupé (compteur de répétitions) au
 * lieu d'ajouter une ligne : une sonde qui bagote ne remplit pas le journal.
 *
 * Seuls les événements importants sont écrits en flash, par lots (voir OPC).
 */
class EventLog
{
public:
    static constexpr size_t CAPACITY = 64;
    static constexpr size_t TEXT_SIZE = sizeof(EventEntry::text);
    static constexpr uint32_t COALESCE_WINDOW_MS = 60000;
    static constexpr size_t COALESCE_DEPTH = 4;

    // Ligne CSV ou affichée : date, champs et texte.
    static constexpr size_t LINE_SIZE = 96;

    void add(
        uint32_t now,
        const ClockSample& clock,
        EventKind kind,
        bool important,
        const char* text);

    size_t count() const;

    /** Événement d'indice index, 0 étant le plus récent ; nullptr au-delà. */
    const EventEntry* at(size_t index) const;

    /** Écrit les événements ajoutés depuis le dernier appel (port série). */
    void printNew(Stream& stream);

    /** Événement important ajouté ou répété depuis la dernière sauvegarde. */
    bool hasUnsavedImportant() const;
    void markSaved();
    void markUnsaved();

    /**
     * Copie les événements importants, du plus ancien au plus récent.
     * @return Nombre d'événements copiés
     */
    size_t copyImportant(EventEntry* destination, size_t capacity) const;

    /** Recharge un événement sauvegardé (au démarrage, du plus ancien). */
    void restore(const EventEntry& entry);

    void clear();

    /** "04/10 14:32:05", ou "+125 s" si l'heure était inconnue. */
    static size_t formatTimestamp(
        const EventEntry& entry,
        char* buffer,
        size_t size);

    /** "04/10 14:32:05 Temp. : RUPTURE (x3)" */
    static size_t formatLine(
        const EventEntry& entry,
        char* buffer,
        size_t size);

    /** "2026-10-04 14:32:05;125000;1;3;Temp. : RUPTURE" */
    static size_t formatCsv(
        const EventEntry& entry,
        char* buffer,
        size_t size);

    static bool parseCsv(const char* line, EventEntry& entry);

private:
    EventEntry entries[CAPACITY];
    size_t head = 0;        // prochaine case écrite
    size_t used = 0;
    size_t unprinted = 0;   // événements les plus récents pas encore écrits
    bool unsavedImportant = false;

    EventEntry& slot(size_t index);    // 0 = le plus récent
    void append(const EventEntry& entry);
};

#endif
