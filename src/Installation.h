#ifndef INSTALLATION_H
#define INSTALLATION_H

#include <Hardware/pinout.h>
#include <ParameterList.h>
#include <hmi/HomeScreen.h>

class SensorBoard;
class Adafruit_BME280;
class ProcessControl;
class Storage;
class ParameterEditor;
class OPC;

class Installation
{
public:

    struct MenuSettings
    {
        uint32_t inactivityTimeoutSeconds = 10;
    };

    struct SafetySettings
    {
        uint32_t measurementTimeoutSeconds = 30;
    };

    MenuSettings menuSettings;
    SafetySettings safetySettings;

    virtual ~Installation() = default;

    virtual const char* name() const = 0;

    virtual bool begin(
        SensorBoard& board,
        Adafruit_BME280& bme,
        ProcessControl& process) = 0;

    virtual void printHomeScreen(
        HomeScreenContext& context) = 0;

    virtual bool validateParameters(
        const ParameterEditor& editor) const
    {
        (void)editor;
        return true;
    }

    virtual void onParametersApplied()
    {
    }

    virtual void load(Storage& storage) = 0;

    virtual void save(Storage& storage) = 0;

    virtual void factoryReset() = 0;

    ParameterList& getParameters()
    {
        return parameterList;
    }

    uint32_t menuTimeoutMs() const
    {
        return
            menuSettings.inactivityTimeoutSeconds *
            1000UL;
    }

    uint32_t measurementTimeoutMs() const
    {
        return
            safetySettings.measurementTimeoutSeconds *
            1000UL;
    }

protected:

    Parameter parameterStorage[MAX_PARAMETERS];
    ParameterList parameterList;

private:
    friend class OPC;

    bool prepareParameterRegistration();
    bool completeParameterRegistration();
};

#endif
