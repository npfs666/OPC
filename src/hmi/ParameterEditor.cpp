#include <hmi/ParameterEditor.h>

#include <cmath>
#include <cstring>

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

void ParameterEditor::capture()
{
    for (size_t i = 0; i < draftCount; i++)
    {
        if (drafts[i].parameter == nullptr)
            continue;

        const Parameter& parameter = *drafts[i].parameter;

        switch (parameter.type)
        {
        case Parameter::Type::Bool:
            if (parameter.value.boolean != nullptr)
                drafts[i].booleanValue = *parameter.value.boolean;
            break;

        case Parameter::Type::Integer:
            if (parameter.discrete.target != nullptr &&
                parameter.discrete.read != nullptr)
            {
                drafts[i].integerValue =
                    parameter.discrete.read(
                        parameter.discrete.target);
            }
            break;

        case Parameter::Type::Double:
            if (parameter.value.number != nullptr)
                drafts[i].numberValue = *parameter.value.number;
            break;

        case Parameter::Type::Selection:
            if (parameter.discrete.target != nullptr &&
                parameter.discrete.read != nullptr)
            {
                drafts[i].selectionValue =
                    parameter.discrete.read(
                        parameter.discrete.target);
            }
            break;
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
