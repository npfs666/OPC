#include <Storage.h>

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SingleFileDrive.h>
#include <hmi/ParameterEditor.h>

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

    bool isValidRequiredText(JsonVariantConst value)
    {
        return value.is<const char*>() &&
               value.as<const char*>()[0] != '\0';
    }
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
    const char* installationName,
    ParameterList& parameters,
    ParameterEditor& editor,
    const ParameterRestoreValidator& validator)
{
    InterruptGuard interruptGuard;

    editor.begin(parameters);
    editor.capture();

    if (!mounted)
        return RestoreResult::StorageUnavailable;

    if (!LittleFS.exists(CONFIG_PATH))
        return RestoreResult::NoFile;

    if (!readConfiguration(
            installationName,
            parameters,
            editor) ||
        !editor.validate() ||
        !validator.validateRestoredParameters(editor))
    {
        editor.capture();
        return RestoreResult::InvalidFile;
    }

    if (!editor.apply())
    {
        /*
         * editor.apply() ne peut normalement plus échouer après
         * validation. En cas d'incohérence interne, le démarrage
         * doit néanmoins être refusé comme restauration valide.
         */
        return RestoreResult::InvalidFile;
    }

    return RestoreResult::Restored;
}

bool Storage::save(
    const char* installationName,
    const ParameterList& parameters)
{
    if (!mounted ||
        installationName == nullptr ||
        installationName[0] == '\0')
    {
        return false;
    }

    JsonDocument document;
    document["schema"] = SCHEMA_VERSION;
    document["installation_name"] = installationName;

    JsonArray storedParameters =
        document["parameters"].to<JsonArray>();

    for (size_t i = 0; i < parameters.count(); i++)
    {
        const Parameter* parameter =
            parameters.get(i);

        if (parameter == nullptr ||
            parameter->categoryKey == nullptr ||
            parameter->categoryName == nullptr ||
            parameter->ownerKey == nullptr ||
            parameter->ownerName == nullptr ||
            parameter->key == nullptr ||
            parameter->name == nullptr)
        {
            return false;
        }

        JsonObject stored =
            storedParameters.add<JsonObject>();

        stored["category_key"] =
            parameter->categoryKey;
        stored["category_name"] =
            parameter->categoryName;
        stored["owner_key"] =
            parameter->ownerKey;
        stored["owner_name"] =
            parameter->ownerName;
        stored["key"] = parameter->key;
        stored["name"] = parameter->name;
        stored["type"] = typeName(parameter->type);

        switch (parameter->type)
        {
        case Parameter::Type::Bool:
            if (parameter->value.boolean == nullptr)
                return false;

            stored["value"] =
                *parameter->value.boolean;
            break;

        case Parameter::Type::Integer:
            if (parameter->discrete.target == nullptr ||
                parameter->discrete.read == nullptr)
            {
                return false;
            }

            stored["value"] =
                parameter->discrete.read(
                    parameter->discrete.target);
            stored["unit"] =
                parameter->data.integer.unit != nullptr
                    ? parameter->data.integer.unit
                    : "";
            break;

        case Parameter::Type::Double:
            if (parameter->value.number == nullptr ||
                !std::isfinite(*parameter->value.number))
            {
                return false;
            }

            stored["value"] =
                *parameter->value.number;
            stored["unit"] =
                parameter->data.number.unit != nullptr
                    ? parameter->data.number.unit
                    : "";
            break;

        case Parameter::Type::Selection:
        {
            if (parameter->discrete.target == nullptr ||
                parameter->discrete.read == nullptr ||
                parameter->data.selection.options == nullptr ||
                parameter->data.selection.count == 0)
            {
                return false;
            }

            stored["value"] =
                parameter->discrete.read(
                    parameter->discrete.target);

            JsonArray options =
                stored["options"].to<JsonArray>();

            for (uint8_t option = 0;
                 option < parameter->data.selection.count;
                 option++)
            {
                const ParameterOption& source =
                    parameter->data.selection.options[option];

                if (source.name == nullptr ||
                    source.name[0] == '\0')
                {
                    return false;
                }

                JsonObject destination =
                    options.add<JsonObject>();

                destination["value"] = source.value;
                destination["name"] = source.name;
            }

            break;
        }

        default:
            return false;
        }
    }

    InterruptGuard interruptGuard;

    File temporary =
        LittleFS.open(TEMP_PATH, "w");

    if (!temporary)
        return false;

    const size_t expectedSize =
        measureJson(document);
    const size_t writtenSize =
        serializeJson(document, temporary);

    temporary.flush();
    temporary.close();

    if (writtenSize != expectedSize ||
        !validateWrittenFile())
    {
        LittleFS.remove(TEMP_PATH);
        return false;
    }

    /*
     * littlefs remplace atomiquement la destination lors du rename :
     * une coupure laisse donc soit l'ancien fichier, soit le nouveau.
     */
    if (!LittleFS.rename(
            TEMP_PATH,
            CONFIG_PATH))
    {
        LittleFS.remove(TEMP_PATH);
        return false;
    }

    usbExportPending = true;

    if (usbExportStarted &&
        !usbDriveMounted)
    {
        usbExportPending =
            !refreshUsbExport();
    }

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

bool Storage::readConfiguration(
    const char* installationName,
    ParameterList& parameters,
    ParameterEditor& editor)
{
    File file =
        LittleFS.open(CONFIG_PATH, "r");

    if (!file)
        return false;

    JsonDocument document;
    const DeserializationError error =
        deserializeJson(document, file);

    file.close();

    if (error ||
        installationName == nullptr ||
        installationName[0] == '\0' ||
        document["schema"].as<uint32_t>() !=
            SCHEMA_VERSION ||
        !isValidRequiredText(
            document["installation_name"]) ||
        strcmp(
            installationName,
            document["installation_name"]
                .as<const char*>()) != 0 ||
        !document["parameters"].is<JsonArrayConst>())
    {
        return false;
    }

    bool seen[MAX_PARAMETERS] = {};

    for (JsonObjectConst stored :
         document["parameters"].as<JsonArrayConst>())
    {
        if (!isValidRequiredText(stored["category_key"]) ||
            !isValidRequiredText(stored["category_name"]) ||
            !isValidRequiredText(stored["owner_key"]) ||
            !isValidRequiredText(stored["owner_name"]) ||
            !isValidRequiredText(stored["key"]) ||
            !isValidRequiredText(stored["name"]) ||
            !isValidRequiredText(stored["type"]) ||
            stored["value"].isNull())
        {
            return false;
        }

        const char* ownerKey =
            stored["owner_key"].as<const char*>();
        const char* key =
            stored["key"].as<const char*>();

        size_t parameterIndex = parameters.count();

        for (size_t i = 0; i < parameters.count(); i++)
        {
            const Parameter* candidate =
                parameters.get(i);

            if (candidate != nullptr &&
                strcmp(candidate->ownerKey, ownerKey) == 0 &&
                strcmp(candidate->key, key) == 0)
            {
                parameterIndex = i;
                break;
            }
        }

        /*
         * Une version plus récente peut avoir supprimé un paramètre.
         * Une entrée inconnue est donc ignorée, sans invalider les
         * paramètres encore reconnus.
         */
        if (parameterIndex >= parameters.count())
            continue;

        if (seen[parameterIndex])
            return false;

        seen[parameterIndex] = true;

        Parameter* parameter =
            parameters.get(parameterIndex);
        ParameterDraft& draft =
            editor.get(parameterIndex);

        if (parameter == nullptr ||
            draft.parameter != parameter ||
            strcmp(
                parameter->categoryKey,
                stored["category_key"].as<const char*>()) != 0 ||
            strcmp(
                typeName(parameter->type),
                stored["type"].as<const char*>()) != 0)
        {
            return false;
        }

        switch (parameter->type)
        {
        case Parameter::Type::Bool:
            if (!stored["value"].is<bool>())
                return false;

            draft.booleanValue =
                stored["value"].as<bool>();
            break;

        case Parameter::Type::Integer:
        {
            if (!stored["value"].is<int32_t>() ||
                !stored["unit"].is<const char*>())
            {
                return false;
            }

            draft.integerValue =
                stored["value"].as<int32_t>();
            break;
        }

        case Parameter::Type::Double:
        {
            if (!stored["value"].is<double>() ||
                !stored["unit"].is<const char*>())
            {
                return false;
            }

            draft.numberValue =
                stored["value"].as<double_t>();
            break;
        }

        case Parameter::Type::Selection:
        {
            if (!stored["value"].is<int32_t>() ||
                !stored["options"].is<JsonArrayConst>())
            {
                return false;
            }

            JsonArrayConst options =
                stored["options"].as<JsonArrayConst>();

            if (options.size() !=
                    parameter->data.selection.count)
            {
                return false;
            }

            draft.selectionValue =
                stored["value"].as<int32_t>();

            size_t optionIndex = 0;

            for (JsonObjectConst storedOption : options)
            {
                if (!storedOption["value"].is<int32_t>() ||
                    !isValidRequiredText(
                        storedOption["name"]))
                {
                    return false;
                }

                const ParameterOption& currentOption =
                    parameter->data.selection
                        .options[optionIndex];

                if (storedOption["value"].as<int32_t>() !=
                    currentOption.value)
                {
                    return false;
                }

                optionIndex++;
            }

            break;
        }

        default:
            return false;
        }

    }

    return true;
}

bool Storage::validateWrittenFile() const
{
    File file =
        LittleFS.open(TEMP_PATH, "r");

    if (!file)
        return false;

    JsonDocument document;
    const DeserializationError error =
        deserializeJson(document, file);

    file.close();

    return
        !error &&
        document["schema"].as<uint32_t>() ==
            SCHEMA_VERSION &&
        isValidRequiredText(
            document["installation_name"]) &&
        document["parameters"].is<JsonArrayConst>();
}

const char* Storage::typeName(
    Parameter::Type type)
{
    switch (type)
    {
    case Parameter::Type::Bool:
        return "bool";
    case Parameter::Type::Integer:
        return "integer";
    case Parameter::Type::Double:
        return "double";
    case Parameter::Type::Selection:
        return "selection";
    default:
        return "invalid";
    }
}
