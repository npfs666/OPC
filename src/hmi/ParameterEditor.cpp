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
        if (drafts[i].parameter == nullptr)
            return false;

        const ParameterDraft& draft = drafts[i];
        const Parameter& parameter =
            *draft.parameter;

        if (parameter.readOnly && !parameter.persistent)
            continue;

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
    }

    return true;
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
