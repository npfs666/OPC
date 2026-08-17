#ifndef RELAYOUTPUT_H
#define RELAYOUTPUT_H

#include "Outputs/Output.h"

class RelayOutput : public Output
{
public:

    struct Settings
    {
        uint8_t pin = 0;
        bool activeHigh = true;
        bool safeState = false;
    };

    Settings settings;

    RelayOutput();

    void begin(
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        bool safeState = false);

    void begin(
        const char* key,
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        bool safeState = false);

    bool begin() override;

    void poll(uint32_t now) override;

    void forceSafe() override;

    bool applySettings() override;

    bool isHealthy() const override;

    /**
     * Impose l'état logique de sécurité et interdit sa restauration/édition.
     * Utile lorsqu'un état ON ne peut jamais être considéré comme sûr.
     */
    void lockSafeState(bool safeState = false);

    void registerParameters(
        ParameterList& list) override;

    bool validateParameters(
        const ParameterEditor& editor)
        const override;

private:
    void applyLogicalState(bool state);

    static void writePhysicalState(
        uint8_t pin,
        bool logicalState,
        bool activeHigh);

    bool initialized = false;
    uint8_t configuredPin = 0;
    bool configuredActiveHigh = true;
    bool configuredSafeState = false;

    bool safeStateLocked = false;
    bool lockedSafeState = false;
};

#endif
