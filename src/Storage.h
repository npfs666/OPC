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

    void poll();

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

    static constexpr const char* CONFIG_PATH =
        "/config.json";
    static constexpr const char* TEMP_PATH =
        "/config.tmp";

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
    volatile bool usbDriveMounted = false;
    volatile bool usbExportPending = false;

    static Storage* usbOwner;

    static void handleUsbPlug(uint32_t data);
    static void handleUsbUnplug(uint32_t data);

    bool startUsbExport();
    bool refreshUsbExport();

    bool readConfiguration(
        const char* installationName,
        ParameterList& parameters,
        ParameterEditor& editor);

    bool validateWrittenFile() const;

    static const char* typeName(
        Parameter::Type type);
};

#endif
