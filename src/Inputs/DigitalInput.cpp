#include <Inputs/DigitalInput.h>

#include <hmi/ParameterList.h>

void DigitalInput::begin(
    const char* name,
    uint8_t pin,
    bool activeHigh,
    uint32_t debounceMs)
{
    begin(name, name, pin, activeHigh, debounceMs);
}

void DigitalInput::begin(
    const char* key,
    const char* name,
    uint8_t pin,
    bool activeHigh,
    uint32_t debounceMs)
{
    beginConfiguration(key);
    Displayable::begin(name);
    this->pin = pin;
    settings.activeHigh = activeHigh;
    settings.debounceMs = debounceMs;
    appliedSettings = settings;

    pinMode(pin, INPUT);
    initialized = true;
    hasSample = false;
    active = false;
    valid = false;
    sampleTime = 0;
}

void DigitalInput::poll(uint32_t now)
{
    if (!initialized)
        return;

    // Les réglages restaurés ou appliqués par le menu prennent effet ici.
    if (settings.activeHigh != appliedSettings.activeHigh ||
        settings.debounceMs != appliedSettings.debounceMs)
    {
        appliedSettings = settings;
        hasSample = false;
        active = false;
        valid = false;
    }

    const bool reading =
        (digitalRead(pin) == HIGH) == appliedSettings.activeHigh;

    sampleTime = now;

    if (!hasSample || reading != candidate)
    {
        candidate = reading;
        candidateSince = now;
        hasSample = true;
    }

    // Soustraction non signée : reste correcte au débordement de millis().
    if (uint32_t(now - candidateSince) >= appliedSettings.debounceMs)
    {
        active = candidate;
        valid = true;
    }
}

bool DigitalInput::isActive() const
{
    return isValid() && active;
}

bool DigitalInput::isValid() const
{
    return valid &&
        settings.activeHigh == appliedSettings.activeHigh &&
        settings.debounceMs == appliedSettings.debounceMs;
}

uint32_t DigitalInput::sampledAt() const
{
    return sampleTime;
}

double_t DigitalInput::printValue() const
{
    return isValid() ? (active ? 1.0 : 0.0) : NAN;
}

const char* DigitalInput::getUnit() const
{
    return "";
}

uint8_t DigitalInput::printDecimals() const
{
    return 0;
}

void DigitalInput::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "inputs", "Input", getConfigurationKey(), getName()
    });

    parameters.addBool(
        "active_high", "Actif à HIGH", settings.activeHigh);

    parameters.addInteger(
        "debounce_ms", "Filtrage", settings.debounceMs,
        0, 10000, 1, "ms");
}
