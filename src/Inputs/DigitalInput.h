#pragma once

#include <Configurable.h>
#include <hmi/Displayable.h>

class DigitalInput : public Displayable, public Configurable
{
public:
    struct Settings
    {
        bool activeHigh = true;
        uint32_t debounceMs = 0;
    };

    Settings settings;

    void begin(
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        uint32_t debounceMs = 0);

    void begin(
        const char* key,
        const char* name,
        uint8_t pin,
        bool activeHigh = true,
        uint32_t debounceMs = 0);

    void poll(uint32_t now);

    bool isActive() const;
    bool isValid() const;
    uint32_t sampledAt() const;

    double_t printValue() const override;
    const char* getUnit() const override;
    uint8_t printDecimals() const override;

    void registerParameters(ParameterList& list) override;

private:
    uint8_t pin = 0;
    bool initialized = false;
    bool hasSample = false;
    bool active = false;
    bool valid = false;
    bool candidate = false;
    uint32_t candidateSince = 0;
    uint32_t sampleTime = 0;
    Settings appliedSettings;
};
