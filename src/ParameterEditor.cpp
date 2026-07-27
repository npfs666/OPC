#include <ParameterEditor.h>

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
            if (parameter.value.integer != nullptr)
                drafts[i].integerValue = *parameter.value.integer;
            break;

        case Parameter::Type::Double:
            if (parameter.value.number != nullptr)
                drafts[i].numberValue = *parameter.value.number;
            break;
        }
    }
}

ParameterDraft& ParameterEditor::get(size_t index)
{
    return drafts[index];
}

const ParameterDraft& ParameterEditor::get(size_t index) const
{
    return drafts[index];
}
