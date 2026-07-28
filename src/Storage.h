#ifndef STORAGE_H
#define STORAGE_H

#include <Hardware/pinout.h>
#include <hmi/ParameterList.h>

class ParameterEditor;

class ParameterRestoreValidator
{
public:
    virtual ~ParameterRestoreValidator() = default;

    virtual bool validateRestoredParameters(
        const ParameterEditor& editor) const = 0;
};

class Storage
{
public:
    enum class RestoreResult : uint8_t
    {
        Restored,
        NoFile,
        InvalidFile,
        StorageUnavailable
    };

    bool begin();

    RestoreResult restore(
        const char* installationName,
        ParameterList& parameters,
        ParameterEditor& editor,
        const ParameterRestoreValidator& validator);

    bool save(
        const char* installationName,
        const ParameterList& parameters);

    bool erase();

private:
    static constexpr uint32_t SCHEMA_VERSION = 1;
    static constexpr size_t MAX_LABEL_LENGTH = 64;
    static constexpr size_t MAX_UNIT_LENGTH = 24;

    static constexpr const char* CONFIG_PATH =
        "/config.json";
    static constexpr const char* TEMP_PATH =
        "/config.tmp";

    struct RestoredParameterText
    {
        bool present = false;
        char categoryName[MAX_LABEL_LENGTH] = {};
        char ownerName[MAX_LABEL_LENGTH] = {};
        char parameterName[MAX_LABEL_LENGTH] = {};
        char unit[MAX_UNIT_LENGTH] = {};
        size_t optionStart = 0;
        uint8_t optionCount = 0;
    };

    RestoredParameterText restoredText[MAX_PARAMETERS];
    ParameterOption restoredOptions[
        ParameterList::MAX_SELECTION_OPTIONS];
    char restoredOptionNames[
        ParameterList::MAX_SELECTION_OPTIONS]
        [MAX_LABEL_LENGTH] = {};

    size_t restoredOptionCount = 0;
    bool mounted = false;

    void clearRestoredText();

    bool readConfiguration(
        const char* installationName,
        ParameterList& parameters,
        ParameterEditor& editor);

    bool applyRestoredText(
        ParameterList& parameters);

    bool validateWrittenFile() const;

    static const char* typeName(
        Parameter::Type type);

    static bool copyRequiredText(
        const char* source,
        char* destination,
        size_t capacity);

    static bool copyOptionalText(
        const char* source,
        char* destination,
        size_t capacity);
};

#endif
