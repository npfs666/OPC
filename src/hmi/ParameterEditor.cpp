#include <hmi/ParameterEditor.h>

#include <cmath>
#include <cstring>

namespace
{
    double_t draftValue(const ParameterDraft& draft)
    {
        switch (draft.parameter->type)
        {
        case Parameter::Type::Bool: return draft.booleanValue;
        case Parameter::Type::Integer: return draft.integerValue;
        case Parameter::Type::Double: return draft.numberValue;
        case Parameter::Type::Selection: return draft.selectionValue;
        }
        return NAN;
    }

    // Inverse de draftValue() : exact pour chaque type (entiers sur 32 bits).
    void setDraftValue(ParameterDraft& draft, double_t value)
    {
        switch (draft.parameter->type)
        {
        case Parameter::Type::Bool:
            draft.booleanValue = value != 0.0;
            break;
        case Parameter::Type::Integer:
            draft.integerValue = static_cast<int32_t>(value);
            break;
        case Parameter::Type::Double:
            draft.numberValue = value;
            break;
        case Parameter::Type::Selection:
            draft.selectionValue = static_cast<int32_t>(value);
            break;
        }
    }

    bool sameOwner(const ParameterDraft& first, const ParameterDraft& second)
    {
        return std::strcmp(
                   first.parameter->ownerKey,
                   second.parameter->ownerKey) == 0;
    }

    void readCurrentValue(ParameterDraft& draft)
    {
        const Parameter& parameter = *draft.parameter;
        switch (parameter.type)
        {
        case Parameter::Type::Bool:
            if (parameter.value.boolean != nullptr)
                draft.booleanValue = *parameter.value.boolean;
            break;
        case Parameter::Type::Double:
            if (parameter.value.number != nullptr)
                draft.numberValue = *parameter.value.number;
            break;
        case Parameter::Type::Integer:
        case Parameter::Type::Selection:
            if (parameter.discrete.target != nullptr && parameter.discrete.read != nullptr)
            {
                const int32_t value = parameter.discrete.read(parameter.discrete.target);
                if (parameter.type == Parameter::Type::Integer)
                    draft.integerValue = value;
                else
                    draft.selectionValue = value;
            }
            break;
        }
    }
}

void ParameterEditor::begin(const ParameterList& parameters)
{
    draftCount = parameters.count();

    if (draftCount > MAX_PARAMETERS)
        draftCount = MAX_PARAMETERS;

    for (size_t i = 0; i < draftCount; i++)
        drafts[i].parameter = parameters.get(i);
}

size_t ParameterEditor::count() const
{
    return draftCount;
}

void ParameterEditor::capture(const char* ownerKey)
{
    for (size_t i = 0; i < draftCount; i++)
    {
        if (drafts[i].parameter == nullptr)
            continue;

        if (ownerKey != nullptr &&
            std::strcmp(drafts[i].parameter->ownerKey, ownerKey) != 0)
            continue;

        readCurrentValue(drafts[i]);
        capturedValues[i] = draftValue(drafts[i]);
    }
}

bool ParameterEditor::isChanged(size_t index) const
{
    const ParameterDraft& draft = drafts[index];
    if (draft.parameter == nullptr || draft.parameter->readOnly)
        return false;

    const double_t value = draftValue(draft);
    return value != capturedValues[index] &&
        !(std::isnan(value) && std::isnan(capturedValues[index]));
}

bool ParameterEditor::hasChanges(const char* ownerKey) const
{
    for (size_t i = 0; i < draftCount; i++)
        if (isChanged(i) &&
            (ownerKey == nullptr ||
             std::strcmp(drafts[i].parameter->ownerKey, ownerKey) == 0))
            return true;
    return false;
}

bool ParameterEditor::hasOnlyLiveChanges() const
{
    bool changed = false;

    for (size_t i = 0; i < draftCount; i++)
    {
        if (!isChanged(i))
            continue;

        if (!drafts[i].parameter->live)
            return false;

        changed = true;
    }

    return changed;
}

void ParameterEditor::refreshUnchanged()
{
    for (size_t i = 0; i < draftCount; i++)
    {
        if (drafts[i].parameter != nullptr && !isChanged(i))
        {
            readCurrentValue(drafts[i]);
            capturedValues[i] = draftValue(drafts[i]);
        }
    }
}

bool ParameterEditor::validate() const
{
    for (size_t i = 0; i < draftCount; i++)
    {
        if (!isDraftValid(drafts[i]))
            return false;
    }

    return true;
}

bool ParameterEditor::isDraftValid(const ParameterDraft& draft)
{
    if (draft.parameter == nullptr)
        return false;

    const Parameter& parameter =
        *draft.parameter;

    // Lecture de diagnostic : jamais appliquée.
    if (parameter.readOnly && !parameter.persistent)
        return true;

    switch (parameter.type)
    {
    case Parameter::Type::Bool:
        if (parameter.value.boolean == nullptr)
            return false;
        break;

    case Parameter::Type::Integer:
        if (parameter.discrete.target == nullptr ||
            parameter.discrete.read == nullptr ||
            parameter.discrete.write == nullptr ||
            parameter.data.integer.minimum >
                parameter.data.integer.maximum ||
            parameter.data.integer.step <= 0 ||
            draft.integerValue <
                parameter.data.integer.minimum ||
            draft.integerValue >
                parameter.data.integer.maximum)
        {
            return false;
        }
        break;

    case Parameter::Type::Double:
        if (parameter.value.number == nullptr ||
            !std::isfinite(
                parameter.data.number.minimum) ||
            !std::isfinite(
                parameter.data.number.maximum) ||
            !std::isfinite(
                parameter.data.number.step) ||
            !std::isfinite(
                draft.numberValue) ||
            parameter.data.number.minimum >
                parameter.data.number.maximum ||
            parameter.data.number.step <= 0.0 ||
            draft.numberValue <
                parameter.data.number.minimum ||
            draft.numberValue >
                parameter.data.number.maximum)
        {
            return false;
        }
        break;

    case Parameter::Type::Selection:
    {
        if (parameter.discrete.target == nullptr ||
            parameter.discrete.read == nullptr ||
            parameter.discrete.write == nullptr ||
            parameter.data.selection.options == nullptr ||
            parameter.data.selection.count == 0)
        {
            return false;
        }

        bool valueFound = false;

        for (uint8_t option = 0;
             option <
                parameter.data.selection.count;
             option++)
        {
            if (parameter.data.selection
                    .options[option].value ==
                draft.selectionValue)
            {
                valueFound = true;
                break;
            }
        }

        if (!valueFound)
            return false;

        break;
    }

    default:
        return false;
    }

    return true;
}

bool ParameterEditor::isValidSet(
    const ParameterRestoreValidator& validator) const
{
    return validate() && validator.validateRestoredParameters(*this);
}

size_t ParameterEditor::keepValidDrafts(
    const ParameterRestoreValidator& validator,
    const Parameter** resetOwners,
    size_t capacity)
{
    if (isValidSet(validator))
        return 0;

    for (size_t i = 0; i < draftCount; i++)
    {
        if (drafts[i].parameter == nullptr)
            return 0;
    }

    // Valeurs lues et réglages remis par défaut ; les valeurs capturées
    // avant la lecture sont les réglages par défaut. Statiques : hors de la
    // pile, pour une fonction appelée au démarrage seulement.
    static double_t loaded[MAX_PARAMETERS];
    static bool reset[MAX_PARAMETERS];

    // Hors plage (plage réduite par une nouvelle version...) : ce seul
    // réglage revient par défaut.
    for (size_t i = 0; i < draftCount; i++)
    {
        reset[i] = !isDraftValid(drafts[i]);
        loaded[i] = reset[i] ? capturedValues[i] : draftValue(drafts[i]);
    }

    // Un propriétaire est traité une fois, à son premier réglage.
    auto isFirstOfOwner = [this](size_t index)
    {
        for (size_t i = 0; i < index; i++)
        {
            if (sameOwner(drafts[i], drafts[index]))
                return false;
        }

        return true;
    };

    // Valeurs du propriétaire de first : lues, ou par défaut.
    auto setOwner = [this](size_t first, const double_t* values)
    {
        for (size_t j = first; j < draftCount; j++)
        {
            if (sameOwner(drafts[first], drafts[j]))
                setDraftValue(drafts[j], values[j]);
        }
    };

    // Validation croisée : tout par défaut, puis chaque propriétaire repris
    // avec ses valeurs lues, gardé seulement si l'ensemble reste valide.
    for (size_t i = 0; i < draftCount; i++)
        setDraftValue(drafts[i], capturedValues[i]);

    for (size_t i = 0; i < draftCount; i++)
    {
        if (!isFirstOfOwner(i))
            continue;

        setOwner(i, loaded);

        if (isValidSet(validator))
            continue;

        setOwner(i, capturedValues);

        for (size_t j = i; j < draftCount; j++)
        {
            if (sameOwner(drafts[i], drafts[j]))
                reset[j] = true;
        }
    }

    size_t resetCount = 0;

    for (size_t i = 0; i < draftCount; i++)
    {
        if (!isFirstOfOwner(i))
            continue;

        bool ownerReset = false;

        for (size_t j = i; j < draftCount; j++)
        {
            if (reset[j] && sameOwner(drafts[i], drafts[j]))
                ownerReset = true;
        }

        if (!ownerReset)
            continue;

        if (resetOwners != nullptr && resetCount < capacity)
            resetOwners[resetCount] = drafts[i].parameter;

        resetCount++;
    }

    return resetCount;
}

bool ParameterEditor::apply()
{
    if (!validate())
        return false;

    for (size_t i = 0; i < draftCount; i++)
    {
        ParameterDraft& draft = drafts[i];
        const Parameter& parameter =
            *draft.parameter;

        if (parameter.readOnly && !parameter.persistent)
            continue;

        switch (parameter.type)
        {
        case Parameter::Type::Bool:
            *parameter.value.boolean =
                draft.booleanValue;
            break;

        case Parameter::Type::Integer:
            parameter.discrete.write(
                parameter.discrete.target,
                draft.integerValue);
            break;

        case Parameter::Type::Double:
            *parameter.value.number =
                draft.numberValue;
            break;

        case Parameter::Type::Selection:
            parameter.discrete.write(
                parameter.discrete.target,
                draft.selectionValue);
            break;

        default:
            return false;
        }
    }

    capture();

    return true;
}

ParameterDraft& ParameterEditor::get(size_t index)
{
    return drafts[index];
}

const ParameterDraft& ParameterEditor::get(size_t index) const
{
    return drafts[index];
}

const ParameterDraft* ParameterEditor::find(
    const char* ownerKey,
    const char* parameterKey) const
{
    if (ownerKey == nullptr ||
        parameterKey == nullptr)
    {
        return nullptr;
    }

    for (size_t i = 0; i < draftCount; i++)
    {
        const Parameter* parameter =
            drafts[i].parameter;

        if (parameter == nullptr ||
            parameter->ownerKey == nullptr ||
            parameter->key == nullptr)
        {
            continue;
        }

        if (std::strcmp(
                parameter->ownerKey,
                ownerKey) == 0 &&
            std::strcmp(
                parameter->key,
                parameterKey) == 0)
        {
            return &drafts[i];
        }
    }

    return nullptr;
}
