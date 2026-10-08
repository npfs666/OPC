// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Storage.h>

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SingleFileDrive.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterJson.h>

#include <cmath>
#include <cstring>

namespace
{
    class InterruptGuard final
    {
    public:
        InterruptGuard()
        {
            noInterrupts();
        }

        ~InterruptGuard()
        {
            interrupts();
        }
    };

    // Compare la sérialisation d'un document au contenu d'un fichier.
    class FileComparator final
    {
    public:
        explicit FileComparator(File& file)
            : file(file)
        {
        }

        size_t write(uint8_t character)
        {
            if (same && file.read() != character)
                same = false;

            return 1;
        }

        size_t write(const uint8_t* buffer, size_t length)
        {
            for (size_t i = 0; i < length; i++)
                write(buffer[i]);

            return length;
        }

        bool matches()
        {
            return same && file.available() == 0;
        }

    private:
        File& file;
        bool same = true;
    };
}

Storage* Storage::usbOwner = nullptr;

bool Storage::begin()
{
    if (!mounted)
        mounted = LittleFS.begin();

    if (mounted &&
        !usbExportStarted)
    {
        startUsbExport();
    }

    return mounted;
}

void Storage::poll()
{
    if (!mounted ||
        !usbExportStarted ||
        !usbExportPending)
    {
        return;
    }

    InterruptGuard interruptGuard;

    if (usbDriveMounted)
        return;

    usbExportPending =
        !refreshUsbExport();
}

Storage::RestoreResult Storage::restore(
    const char* installationId,
    ParameterList& parameters,
    ParameterEditor& editor,
    const ParameterRestoreValidator& validator)
{
    InterruptGuard interruptGuard;

    resetOwnerTotal = 0;
    boardResult = BoardRestoreResult::NoFile;

    editor.begin(parameters);
    editor.capture();

    if (!mounted)
        return RestoreResult::StorageUnavailable;

    // Hors de la pile : appelée au démarrage seulement.
    static ParameterJson::ReadState state;
    state = ParameterJson::ReadState{};

    RestoreResult result = RestoreResult::NoFile;
    const bool hasBoardFile = LittleFS.exists(BOARD_PATH);

    if (LittleFS.exists(CONFIG_PATH))
    {
        JsonDocument document;
        const bool readable = readDocument(CONFIG_PATH, document);

        const bool sameInstallation =
            readable &&
            installationId != nullptr &&
            installationId[0] != '\0' &&
            document["schema"].as<uint32_t>() == SCHEMA_VERSION &&
            document["installation_id"].is<const char*>() &&
            strcmp(
                installationId,
                document["installation_id"].as<const char*>()) == 0;

        result =
            sameInstallation &&
            ParameterJson::read(
                document["parameters"],
                parameters,
                editor,
                ParameterJson::Scope::Installation,
                state)
            ? RestoreResult::Restored
            : RestoreResult::InvalidFile;

        /*
         * Une version précédente enregistrait les réglages de la carte
         * dans /config.json : ils sont repris même si l'installation a
         * changé depuis, puis /board.json est créé.
         */
        if (!hasBoardFile &&
            readable &&
            ParameterJson::read(
                document["parameters"],
                parameters,
                editor,
                ParameterJson::Scope::Board,
                state))
        {
            for (size_t i = 0; i < parameters.count(); i++)
            {
                const Parameter* parameter = parameters.get(i);

                if (state.seen[i] &&
                    parameter != nullptr &&
                    parameter->board)
                {
                    boardResult = BoardRestoreResult::Migrated;
                }
            }
        }
    }

    if (hasBoardFile)
    {
        JsonDocument document;

        boardResult =
            readDocument(BOARD_PATH, document) &&
            document["schema"].as<uint32_t>() == BOARD_SCHEMA_VERSION &&
            ParameterJson::read(
                document["parameters"],
                parameters,
                editor,
                ParameterJson::Scope::Board,
                state)
            ? BoardRestoreResult::Restored
            : BoardRestoreResult::InvalidFile;
    }

    // Un réglage refusé ne fait pas perdre tout le fichier : seul son
    // propriétaire revient par défaut (voir ParameterEditor).
    resetOwnerTotal = editor.keepValidDrafts(
        validator,
        resetOwners,
        MAX_RESET_OWNERS,
        state.rejected);

    if (!editor.apply())
    {
        /*
         * editor.apply() ne peut normalement plus échouer après
         * validation. En cas d'incohérence interne, le démarrage
         * doit néanmoins être refusé comme restauration valide.
         */
        editor.capture();
        resetOwnerTotal = 0;
        boardResult = BoardRestoreResult::InvalidFile;
        return RestoreResult::InvalidFile;
    }

    if (boardResult == BoardRestoreResult::Migrated &&
        !saveBoard(parameters))
    {
        Serial.println("Board settings migration save failed");
    }

    return result == RestoreResult::Restored && resetOwnerTotal > 0
        ? RestoreResult::PartiallyRestored
        : result;
}

Storage::BoardRestoreResult Storage::boardRestoreResult() const
{
    return boardResult;
}

size_t Storage::resetOwnerCount() const
{
    return resetOwnerTotal;
}

const Parameter* Storage::resetOwner(size_t index) const
{
    return index < resetOwnerTotal && index < MAX_RESET_OWNERS
        ? resetOwners[index]
        : nullptr;
}

bool Storage::save(
    const char* installationId,
    const ParameterList& parameters)
{
    if (!mounted ||
        installationId == nullptr ||
        installationId[0] == '\0')
    {
        return false;
    }

    /*
     * /board.json d'abord : tant qu'il n'est pas écrit, /config.json garde
     * les réglages de la carte d'une version précédente (migration).
     */
    if (!saveBoard(parameters))
        return false;

    JsonDocument document;
    document["schema"] = SCHEMA_VERSION;
    document["installation_id"] = installationId;

    if (!ParameterJson::write(
            document["parameters"].to<JsonArray>(),
            parameters,
            ParameterJson::Scope::Installation))
    {
        return false;
    }

    if (!writeDocument(document, TEMP_PATH, CONFIG_PATH))
        return false;

    usbExportPending = true;

    InterruptGuard interruptGuard;

    if (usbExportStarted &&
        !usbDriveMounted)
    {
        usbExportPending =
            !refreshUsbExport();
    }

    return true;
}

bool Storage::saveBoard(const ParameterList& parameters)
{
    JsonDocument document;
    document["schema"] = BOARD_SCHEMA_VERSION;

    if (!ParameterJson::write(
            document["parameters"].to<JsonArray>(),
            parameters,
            ParameterJson::Scope::Board))
    {
        return false;
    }

    // Réglages rarement modifiés : la flash n'est réécrite qu'au besoin.
    if (matchesFile(document, BOARD_PATH))
        return true;

    return writeDocument(document, BOARD_TEMP_PATH, BOARD_PATH);
}

bool Storage::readDocument(
    const char* path,
    JsonDocument& document)
{
    File file = LittleFS.open(path, "r");

    if (!file)
        return false;

    const DeserializationError error =
        deserializeJson(document, file);

    file.close();
    return !error;
}

bool Storage::writeDocument(
    const JsonDocument& document,
    const char* temporaryPath,
    const char* path)
{
    InterruptGuard interruptGuard;

    File temporary =
        LittleFS.open(temporaryPath, "w");

    if (!temporary)
        return false;

    const size_t expectedSize =
        measureJson(document);
    const size_t writtenSize =
        serializeJson(document, temporary);

    temporary.flush();
    temporary.close();

    JsonDocument written;

    const bool valid =
        writtenSize == expectedSize &&
        readDocument(temporaryPath, written) &&
        written["schema"] == document["schema"] &&
        written["parameters"].is<JsonArrayConst>();

    /*
     * littlefs remplace atomiquement la destination lors du rename :
     * une coupure laisse donc soit l'ancien fichier, soit le nouveau.
     */
    if (!valid ||
        !LittleFS.rename(temporaryPath, path))
    {
        LittleFS.remove(temporaryPath);
        return false;
    }

    return true;
}

bool Storage::matchesFile(
    const JsonDocument& document,
    const char* path)
{
    InterruptGuard interruptGuard;

    if (!LittleFS.exists(path))
        return false;

    File file = LittleFS.open(path, "r");

    if (!file)
        return false;

    FileComparator comparator(file);
    serializeJson(document, comparator);

    const bool same = comparator.matches();
    file.close();
    return same;
}

bool Storage::saveCounters(const JsonDocument& document)
{
    if (!mounted)
        return false;

    InterruptGuard interruptGuard;

    File temporary =
        LittleFS.open(COUNTERS_TEMP_PATH, "w");

    if (!temporary)
        return false;

    const size_t expectedSize =
        measureJson(document);
    const size_t writtenSize =
        serializeJson(document, temporary);

    temporary.flush();
    temporary.close();

    // Une coupure laisse soit l'ancien fichier, soit le nouveau.
    if (writtenSize != expectedSize ||
        !LittleFS.rename(COUNTERS_TEMP_PATH, COUNTERS_PATH))
    {
        LittleFS.remove(COUNTERS_TEMP_PATH);
        return false;
    }

    return true;
}

bool Storage::loadCounters(JsonDocument& document)
{
    if (!mounted || !LittleFS.exists(COUNTERS_PATH))
        return false;

    File file = LittleFS.open(COUNTERS_PATH, "r");

    if (!file)
        return false;

    const DeserializationError error =
        deserializeJson(document, file);

    file.close();
    return !error;
}

bool Storage::saveEvents(const EventEntry* entries, size_t count)
{
    if (!mounted || (entries == nullptr && count > 0))
        return false;

    InterruptGuard interruptGuard;

    File temporary =
        LittleFS.open(EVENTS_TEMP_PATH, "w");

    if (!temporary)
        return false;

    char line[EventLog::LINE_SIZE];
    bool written = true;

    for (size_t i = 0; i < count && written; i++)
    {
        const size_t length =
            EventLog::formatCsv(entries[i], line, sizeof(line));

        written =
            temporary.write(
                reinterpret_cast<const uint8_t*>(line), length) == length &&
            temporary.write('\n') == 1;
    }

    temporary.flush();
    temporary.close();

    // Une coupure laisse soit l'ancien fichier, soit le nouveau.
    if (!written ||
        !LittleFS.rename(EVENTS_TEMP_PATH, EVENTS_PATH))
    {
        LittleFS.remove(EVENTS_TEMP_PATH);
        return false;
    }

    return true;
}

bool Storage::loadEvents(EventLog& log)
{
    if (!mounted || !LittleFS.exists(EVENTS_PATH))
        return false;

    File file = LittleFS.open(EVENTS_PATH, "r");

    if (!file)
        return false;

    char line[EventLog::LINE_SIZE];
    size_t length = 0;

    while (file.available())
    {
        const int character = file.read();

        if (character < 0)
            break;

        if (character != '\n' && length + 1 < sizeof(line))
        {
            line[length++] = static_cast<char>(character);
            continue;
        }

        if (character == '\n')
        {
            line[length] = '\0';

            EventEntry entry;

            if (EventLog::parseCsv(line, entry))
                log.restore(entry);

            length = 0;
        }
    }

    file.close();
    return true;
}

bool Storage::erase()
{
    if (!mounted)
        return false;

    InterruptGuard interruptGuard;

    bool success = true;

    if (LittleFS.exists(TEMP_PATH))
        success = LittleFS.remove(TEMP_PATH);

    if (LittleFS.exists(CONFIG_PATH))
    {
        success =
            LittleFS.remove(CONFIG_PATH) &&
            success;
    }

    usbExportPending = true;

    if (usbExportStarted &&
        !usbDriveMounted)
    {
        usbExportPending =
            !refreshUsbExport();
    }

    return success;
}

void Storage::handleUsbPlug(uint32_t data)
{
    (void)data;

    if (usbOwner != nullptr)
        usbOwner->usbDriveMounted = true;
}

void Storage::handleUsbUnplug(uint32_t data)
{
    (void)data;

    if (usbOwner == nullptr)
        return;

    usbOwner->usbDriveMounted = false;
    usbOwner->usbExportPending = true;
}

bool Storage::startUsbExport()
{
    if (!mounted)
        return false;

    usbExportPending =
        !refreshUsbExport();

    usbOwner = this;

    singleFileDrive.onPlug(
        handleUsbPlug);

    singleFileDrive.onUnplug(
        handleUsbUnplug);

    usbExportStarted =
        singleFileDrive.begin(
            USB_EXPORT_PATH,
            USB_VISIBLE_NAME);

    if (!usbExportStarted)
    {
        usbOwner = nullptr;
        return false;
    }

    /*
     * La pile USB se déconnecte puis se reconnecte pour ajouter
     * l'interface MSC à l'interface série CDC déjà présente.
     */
    delay(2000);

    return true;
}

bool Storage::refreshUsbExport()
{
    if (!mounted ||
        usbDriveMounted)
    {
        return false;
    }

    File destination =
        LittleFS.open(
            USB_EXPORT_TEMP_PATH,
            "w");

    if (!destination)
        return false;

    bool success = true;

    if (LittleFS.exists(CONFIG_PATH))
    {
        File source =
            LittleFS.open(CONFIG_PATH, "r");

        if (!source)
        {
            destination.close();
            LittleFS.remove(
                USB_EXPORT_TEMP_PATH);
            return false;
        }

        uint8_t buffer[256];

        while (source.available())
        {
            const size_t bytesRead =
                source.read(
                    buffer,
                    sizeof(buffer));

            if (bytesRead == 0 ||
                destination.write(
                    buffer,
                    bytesRead) != bytesRead)
            {
                success = false;
                break;
            }
        }

        source.close();
    }
    else
    {
        static constexpr char NO_CONFIGURATION[] =
            "{\n"
            "  \"status\": \"no_saved_configuration\"\n"
            "}\n";

        success =
            destination.write(
                reinterpret_cast<
                    const uint8_t*>(
                    NO_CONFIGURATION),
                sizeof(NO_CONFIGURATION) - 1) ==
            sizeof(NO_CONFIGURATION) - 1;
    }

    destination.flush();
    destination.close();

    if (!success)
    {
        LittleFS.remove(
            USB_EXPORT_TEMP_PATH);
        return false;
    }

    if (!LittleFS.rename(
            USB_EXPORT_TEMP_PATH,
            USB_EXPORT_PATH))
    {
        LittleFS.remove(
            USB_EXPORT_TEMP_PATH);
        return false;
    }

    return true;
}
