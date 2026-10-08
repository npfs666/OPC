#ifndef STORAGE_H
#define STORAGE_H

#include <Hardware/pinout.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <ArduinoJson.h>
#include <EventLog.h>

class Storage
{
public:
    enum class RestoreResult : uint8_t
    {
        Restored,
        // Réglages refusés (hors plage, validation croisée) remis par
        // défaut, propriétaire par propriétaire ; les autres sont restaurés.
        PartiallyRestored,
        NoFile,
        InvalidFile,
        StorageUnavailable
    };

    // Réglages de la carte (/board.json), voir ParameterOwner::board.
    enum class BoardRestoreResult : uint8_t
    {
        Restored,
        // Pas encore de /board.json : réglages repris de /config.json
        // (version précédente), quelle que soit l'installation.
        Migrated,
        NoFile,
        InvalidFile
    };

    // Propriétaires remis par défaut gardés pour le journal.
    static constexpr size_t MAX_RESET_OWNERS = 4;

    bool begin();

    void poll();

    /**
     * Restaure /config.json (réglages de l'installation) et /board.json
     * (réglages de la carte). Une entrée illisible ou refusée ne remet par
     * défaut que son propriétaire. /config.json est ignoré entièrement s'il
     * vient d'une autre installation ou d'un autre schéma ; /board.json ne
     * dépend pas de l'installation.
     *
     * @return résultat pour /config.json ; voir boardRestoreResult()
     */
    RestoreResult restore(
        const char* installationId,
        ParameterList& parameters,
        ParameterEditor& editor,
        const ParameterRestoreValidator& validator);

    BoardRestoreResult boardRestoreResult() const;

    /**
     * Après restore() : nombre de propriétaires remis par défaut, dans
     * l'un ou l'autre fichier, et le premier réglage de chacun
     * (MAX_RESET_OWNERS au plus).
     */
    size_t resetOwnerCount() const;
    const Parameter* resetOwner(size_t index) const;

    /**
     * Écrit /board.json s'il a changé, puis /config.json. Si /board.json
     * ne peut être écrit, /config.json n'est pas modifié.
     */
    bool save(
        const char* installationId,
        const ParameterList& parameters);

    // Efface /config.json ; /board.json est conservé.
    bool erase();

    /**
     * Compteurs d'entretien : fichier séparé de la configuration, écrit
     * périodiquement pour ménager la flash. Écriture atomique (fichier
     * temporaire puis renommage).
     */
    bool saveCounters(const JsonDocument& document);
    bool loadCounters(JsonDocument& document);

    /**
     * Journal : événements importants, du plus ancien au plus récent, une
     * ligne CSV chacun (EventLog::formatCsv). Écriture atomique ; les
     * écritures sont espacées par OPC pour ménager la flash.
     */
    bool saveEvents(const EventEntry* entries, size_t count);
    bool loadEvents(EventLog& log);

private:
    static constexpr uint32_t SCHEMA_VERSION = 2;
    static constexpr uint32_t BOARD_SCHEMA_VERSION = 1;

    static constexpr const char* CONFIG_PATH =
        "/config.json";
    static constexpr const char* TEMP_PATH =
        "/config.tmp";

    static constexpr const char* BOARD_PATH =
        "/board.json";
    static constexpr const char* BOARD_TEMP_PATH =
        "/board.tmp";

    static constexpr const char* COUNTERS_PATH =
        "/counters.json";
    static constexpr const char* COUNTERS_TEMP_PATH =
        "/counters.tmp";

    static constexpr const char* EVENTS_PATH =
        "/events.csv";
    static constexpr const char* EVENTS_TEMP_PATH =
        "/events.tmp";

    /*
     * Le PC lit une copie stable de la configuration. CONFIG_PATH
     * reste ainsi modifiable par save() même lorsque le volume USB
     * est monté.
     */
    static constexpr const char* USB_EXPORT_PATH =
        "/config.usb.json";
    static constexpr const char* USB_EXPORT_TEMP_PATH =
        "/config.usb.tmp";
    static constexpr const char* USB_VISIBLE_NAME =
        "config.json";

    bool mounted = false;
    bool usbExportStarted = false;

    const Parameter* resetOwners[MAX_RESET_OWNERS] = {};
    size_t resetOwnerTotal = 0;
    BoardRestoreResult boardResult = BoardRestoreResult::NoFile;
    volatile bool usbDriveMounted = false;
    volatile bool usbExportPending = false;

    static Storage* usbOwner;

    static void handleUsbPlug(uint32_t data);
    static void handleUsbUnplug(uint32_t data);

    bool startUsbExport();
    bool refreshUsbExport();

    bool saveBoard(const ParameterList& parameters);

    static bool readDocument(
        const char* path,
        JsonDocument& document);

    /**
     * Écriture atomique : fichier temporaire relu et vérifié, puis
     * renommé. Une coupure laisse soit l'ancien fichier, soit le nouveau.
     */
    static bool writeDocument(
        const JsonDocument& document,
        const char* temporaryPath,
        const char* path);

    static bool matchesFile(
        const JsonDocument& document,
        const char* path);
};

#endif
