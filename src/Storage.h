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

    // Propriétaires remis par défaut gardés pour le journal.
    static constexpr size_t MAX_RESET_OWNERS = 4;

    bool begin();

    void poll();

    RestoreResult restore(
        const char* installationId,
        ParameterList& parameters,
        ParameterEditor& editor,
        const ParameterRestoreValidator& validator);

    /**
     * Après PartiallyRestored : nombre de propriétaires remis par défaut,
     * et le premier réglage de chacun (MAX_RESET_OWNERS au plus).
     */
    size_t resetOwnerCount() const;
    const Parameter* resetOwner(size_t index) const;

    bool save(
        const char* installationId,
        const ParameterList& parameters);

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

    static constexpr const char* CONFIG_PATH =
        "/config.json";
    static constexpr const char* TEMP_PATH =
        "/config.tmp";

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
    volatile bool usbDriveMounted = false;
    volatile bool usbExportPending = false;

    static Storage* usbOwner;

    static void handleUsbPlug(uint32_t data);
    static void handleUsbUnplug(uint32_t data);

    bool startUsbExport();
    bool refreshUsbExport();

    bool readConfiguration(
        const char* installationId,
        ParameterList& parameters,
        ParameterEditor& editor);

    bool validateWrittenFile() const;

    static const char* typeName(
        Parameter::Type type);
};

#endif
