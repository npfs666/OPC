#include <Regulator/Comparator.h>

#include <Arduino.h>
#include <Measurements/Measurement.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

void Comparator::begin(
    const char* key,
    const char* name,
    const Measurement& input,
    Direction direction,
    double_t onThreshold,
    double_t offThreshold)
{
    beginComparator(key, name, direction, onThreshold, offThreshold);
    first = &input;
    second = nullptr;
}

void Comparator::begin(
    const char* key,
    const char* name,
    const Measurement& first,
    const Measurement& second,
    Direction direction,
    double_t onThreshold,
    double_t offThreshold)
{
    beginComparator(key, name, direction, onThreshold, offThreshold);
    this->first = &first;
    this->second = &second;
}

void Comparator::beginComparator(
    const char* key,
    const char* name,
    Direction direction,
    double_t onThreshold,
    double_t offThreshold)
{
    Regulator::begin(key, name);

    this->direction = direction;
    settings.onThreshold = onThreshold;
    settings.offThreshold = offThreshold;

    singleThreshold = false;
    minimum = -50.0;
    maximum = 250.0;
    step = 0.5;
    decimals = 1;
    menuParent = nullptr;
    onLabel = "Seuil marche";
    offLabel = "Seuil arrêt";

    on = false;
    valueValid = false;
    lastValue = 0.0;
}

void Comparator::setRange(
    double_t minimum,
    double_t maximum,
    double_t step,
    uint8_t decimals)
{
    this->minimum = minimum;
    this->maximum = maximum;
    this->step = step;
    this->decimals = decimals;
}

void Comparator::setLabels(
    const char* onLabel,
    const char* offLabel)
{
    this->onLabel = onLabel;
    this->offLabel = offLabel;
}

void Comparator::useSingleThreshold()
{
    singleThreshold = true;
    settings.offThreshold = settings.onThreshold;
}

void Comparator::setMenuParent(const char* ownerKey)
{
    menuParent = ownerKey;
}

bool Comparator::isOn() const
{
    return isCommandValid() && on;
}

bool Comparator::readValue(double_t& value) const
{
    if (!valueValid)
        return false;

    value = lastValue;
    return true;
}

bool Comparator::computeValue(double_t& value) const
{
    if (first == nullptr ||
        first->getStatus() != MeasurementStatus::Ok)
    {
        return false;
    }

    value = first->getValue();

    if (second != nullptr)
    {
        if (second->getStatus() != MeasurementStatus::Ok)
            return false;

        value -= second->getValue();
    }

    return std::isfinite(value);
}

void Comparator::update(uint32_t now)
{
    (void)now;

    double_t value = 0.0;
    valueValid = computeValue(value);

    if (!valueValid)
    {
        // Au retour de la mesure, le comparateur repart de l'arrêt.
        on = false;
        invalidateCommand();
        return;
    }

    lastValue = value;

    // Inhibé : arrêt commandé ; à la levée, le comparateur repart de l'arrêt.
    if (isInhibited())
    {
        on = false;
        invalidateCommand();
        return;
    }

    if (singleThreshold)
        settings.offThreshold = settings.onThreshold;

    const double_t onThreshold = settings.onThreshold;
    const double_t offThreshold = settings.offThreshold;

    const bool above = direction == Direction::Above;

    const bool reachesOn =
        above ? value >= onThreshold : value <= onThreshold;

    if (singleThreshold || onThreshold == offThreshold)
    {
        // Sans hystérésis : simple comparaison, seuil compris.
        on = reachesOn;
    }
    else if (!on)
    {
        on = reachesOn;
    }
    else
    {
        const bool reachesOff =
            above ? value <= offThreshold : value >= offThreshold;

        on = !reachesOff;
    }

    writeCommand(on ? 1.0 : 0.0);
}

void Comparator::resume(uint32_t now)
{
    on = false;
    valueValid = false;
    Regulator::resume(now);
}

const char* Comparator::thresholdUnit() const
{
    const char* unit =
        first != nullptr ? first->getUnit() : nullptr;

    if (unit == nullptr)
        return nullptr;

    // Un écart de températures s'exprime en kelvins.
    if (second != nullptr && std::strcmp(unit, "°C") == 0)
        return "K";

    return unit;
}

void Comparator::registerParameters(ParameterList& list)
{
    ParameterOwner owner{
        "regulators",
        "Regulateur",
        getConfigurationKey(),
        getName()
    };

    owner.parentOwnerKey = menuParent;

    auto parameters = list.forOwner(owner);

    parameters.addDouble(
        "on_threshold",
        onLabel,
        settings.onThreshold,
        minimum,
        maximum,
        step,
        decimals,
        thresholdUnit());

    if (singleThreshold)
        return;

    parameters.addDouble(
        "off_threshold",
        offLabel,
        settings.offThreshold,
        minimum,
        maximum,
        step,
        decimals,
        thresholdUnit());
}

bool Comparator::validateParameters(
    const ParameterEditor& editor) const
{
    if (singleThreshold)
        return true;

    const ParameterDraft* onDraft =
        editor.find(getConfigurationKey(), "on_threshold");

    const ParameterDraft* offDraft =
        editor.find(getConfigurationKey(), "off_threshold");

    if (onDraft == nullptr ||
        offDraft == nullptr)
    {
        return false;
    }

    // Le seuil d'arrêt est du côté où le comparateur retombe.
    return direction == Direction::Above
        ? offDraft->numberValue <= onDraft->numberValue
        : offDraft->numberValue >= onDraft->numberValue;
}
