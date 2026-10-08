// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <hmi/ParameterJson.h>

#include <cmath>
#include <cstring>

namespace
{
    bool isValidRequiredText(JsonVariantConst value)
    {
        return value.is<const char*>() &&
               value.as<const char*>()[0] != '\0';
    }

    bool writeEntry(
        JsonObject stored,
        const Parameter& parameter)
    {
        stored["category_key"] = parameter.categoryKey;
        stored["category_name"] = parameter.categoryName;
        stored["owner_key"] = parameter.ownerKey;
        stored["owner_name"] = parameter.ownerName;
        stored["key"] = parameter.key;
        stored["name"] = parameter.name;
        stored["type"] = ParameterJson::typeName(parameter.type);

        switch (parameter.type)
        {
        case Parameter::Type::Bool:
            if (parameter.value.boolean == nullptr)
                return false;

            stored["value"] = *parameter.value.boolean;
            return true;

        case Parameter::Type::Integer:
            if (parameter.discrete.target == nullptr ||
                parameter.discrete.read == nullptr)
            {
                return false;
            }

            stored["value"] =
                parameter.discrete.read(parameter.discrete.target);
            stored["unit"] =
                parameter.data.integer.unit != nullptr
                    ? parameter.data.integer.unit
                    : "";
            return true;

        case Parameter::Type::Double:
            if (parameter.value.number == nullptr ||
                !std::isfinite(*parameter.value.number))
            {
                return false;
            }

            stored["value"] = *parameter.value.number;
            stored["unit"] =
                parameter.data.number.unit != nullptr
                    ? parameter.data.number.unit
                    : "";
            return true;

        case Parameter::Type::Selection:
        {
            if (parameter.discrete.target == nullptr ||
                parameter.discrete.read == nullptr ||
                parameter.data.selection.options == nullptr ||
                parameter.data.selection.count == 0)
            {
                return false;
            }

            stored["value"] =
                parameter.discrete.read(parameter.discrete.target);

            JsonArray options = stored["options"].to<JsonArray>();

            for (uint8_t option = 0;
                 option < parameter.data.selection.count;
                 option++)
            {
                const ParameterOption& source =
                    parameter.data.selection.options[option];

                if (source.name == nullptr || source.name[0] == '\0')
                    return false;

                JsonObject destination = options.add<JsonObject>();
                destination["value"] = source.value;
                destination["name"] = source.name;
            }

            return true;
        }

        default:
            return false;
        }
    }

    /*
     * Valeur de l'entrée dans le brouillon, sans contrôle des bornes ni des
     * options : une liste d'options complétée par une nouvelle version garde
     * ainsi la valeur, et une valeur devenue invalide est remise par défaut
     * par ParameterEditor::keepValidDrafts().
     */
    bool readValue(
        JsonVariantConst value,
        ParameterDraft& draft)
    {
        switch (draft.parameter->type)
        {
        case Parameter::Type::Bool:
            if (!value.is<bool>())
                return false;

            draft.booleanValue = value.as<bool>();
            return true;

        case Parameter::Type::Integer:
            if (!value.is<int32_t>())
                return false;

            draft.integerValue = value.as<int32_t>();
            return true;

        case Parameter::Type::Double:
            if (!value.is<double>())
                return false;

            draft.numberValue = value.as<double_t>();
            return true;

        case Parameter::Type::Selection:
            if (!value.is<int32_t>())
                return false;

            draft.selectionValue = value.as<int32_t>();
            return true;

        default:
            return false;
        }
    }
}

bool ParameterJson::inScope(
    const Parameter& parameter,
    Scope scope)
{
    return parameter.board == (scope == Scope::Board);
}

bool ParameterJson::write(
    JsonArray destination,
    const ParameterList& parameters,
    Scope scope)
{
    for (size_t i = 0; i < parameters.count(); i++)
    {
        const Parameter* parameter = parameters.get(i);

        if (parameter == nullptr)
            return false;

        if (!parameter->persistent ||
            !inScope(*parameter, scope))
        {
            continue;
        }

        if (parameter->categoryKey == nullptr ||
            parameter->categoryName == nullptr ||
            parameter->ownerKey == nullptr ||
            parameter->ownerName == nullptr ||
            parameter->key == nullptr ||
            parameter->name == nullptr)
        {
            return false;
        }

        if (!writeEntry(destination.add<JsonObject>(), *parameter))
            return false;
    }

    return true;
}

bool ParameterJson::read(
    JsonVariantConst source,
    const ParameterList& parameters,
    ParameterEditor& editor,
    Scope scope,
    ReadState& state)
{
    if (!source.is<JsonArrayConst>())
        return false;

    for (JsonVariantConst entry : source.as<JsonArrayConst>())
    {
        if (!isValidRequiredText(entry["owner_key"]) ||
            !isValidRequiredText(entry["key"]))
        {
            continue;
        }

        const char* ownerKey = entry["owner_key"].as<const char*>();
        const char* key = entry["key"].as<const char*>();

        size_t index = parameters.count();

        for (size_t i = 0; i < parameters.count(); i++)
        {
            const Parameter* candidate = parameters.get(i);

            if (candidate != nullptr &&
                std::strcmp(candidate->ownerKey, ownerKey) == 0 &&
                std::strcmp(candidate->key, key) == 0)
            {
                index = i;
                break;
            }
        }

        /*
         * Une version plus récente peut avoir supprimé un paramètre, ou
         * l'avoir rendu transitoire : l'entrée est ignorée sans invalider
         * les autres.
         */
        if (index >= parameters.count() ||
            index >= MAX_PARAMETERS ||
            index >= editor.count())
        {
            continue;
        }

        const Parameter* parameter = parameters.get(index);

        if (!parameter->persistent ||
            !inScope(*parameter, scope) ||
            state.seen[index])
        {
            continue;
        }

        state.seen[index] = true;

        ParameterDraft& draft = editor.get(index);

        if (draft.parameter != parameter ||
            !entry["type"].is<const char*>() ||
            std::strcmp(
                typeName(parameter->type),
                entry["type"].as<const char*>()) != 0 ||
            !readValue(entry["value"], draft))
        {
            state.rejected[index] = true;
        }
    }

    return true;
}

const char* ParameterJson::typeName(Parameter::Type type)
{
    switch (type)
    {
    case Parameter::Type::Bool:
        return "bool";
    case Parameter::Type::Integer:
        return "integer";
    case Parameter::Type::Double:
        return "double";
    case Parameter::Type::Selection:
        return "selection";
    default:
        return "invalid";
    }
}
