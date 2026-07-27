#include <ParameterList.h>

#include <cstring>

void ParameterList::begin(
    Parameter* storage,
    size_t capacity)
{
    parameters = storage;

    if (storage == nullptr)
        parameterCapacity = 0;
    else
        parameterCapacity = capacity;

    parameterCount = 0;
}

void ParameterList::clear()
{
    parameterCount = 0;
}

size_t ParameterList::count() const
{
    return parameterCount;
}

size_t ParameterList::capacity() const
{
    return parameterCapacity;
}

bool ParameterList::isEmpty() const
{
    return parameterCount == 0;
}

bool ParameterList::isFull() const
{
    return parameterCount >= parameterCapacity;
}

bool ParameterList::isInitialized() const
{
    return parameters != nullptr &&
           parameterCapacity > 0;
}

const Parameter* ParameterList::get(size_t index) const
{
    if (!isInitialized())
        return nullptr;

    if (index >= parameterCount)
        return nullptr;

    return &parameters[index];
}

const Parameter* ParameterList::find(
    const char* ownerKey,
    const char* key) const
{
    if (!isInitialized())
        return nullptr;

    if (!isValidText(ownerKey) ||
        !isValidText(key))
    {
        return nullptr;
    }

    for (size_t i = 0; i < parameterCount; i++)
    {
        const Parameter& parameter = parameters[i];

        if (parameter.ownerKey == nullptr ||
            parameter.key == nullptr)
        {
            continue;
        }

        if (strcmp(parameter.ownerKey, ownerKey) != 0)
            continue;

        if (strcmp(parameter.key, key) != 0)
            continue;

        return &parameter;
    }

    return nullptr;
}

bool ParameterList::addBool(
    const char* ownerKey,
    const char* key,
    const char* name,
    bool& value)
{
    Parameter* parameter = create(
        ownerKey,
        key,
        name,
        Parameter::Type::Bool);

    if (parameter == nullptr)
        return false;

    parameter->value.boolean = &value;

    return true;
}

bool ParameterList::addInteger(
    const char* ownerKey,
    const char* key,
    const char* name,
    int32_t& value,
    int32_t minimum,
    int32_t maximum,
    int32_t step,
    const char* unit)
{
    if (minimum > maximum)
        return false;

    if (step <= 0)
        return false;

    Parameter* parameter = create(
        ownerKey,
        key,
        name,
        Parameter::Type::Integer);

    if (parameter == nullptr)
        return false;

    parameter->value.integer = &value;

    parameter->data.integer.minimum = minimum;
    parameter->data.integer.maximum = maximum;
    parameter->data.integer.step = step;
    parameter->data.integer.unit = unit;

    return true;
}

bool ParameterList::addDouble(
    const char* ownerKey,
    const char* key,
    const char* name,
    double_t& value,
    double_t minimum,
    double_t maximum,
    double_t step,
    uint8_t decimals,
    const char* unit)
{
    if (minimum > maximum)
        return false;

    if (step <= 0.0)
        return false;

    Parameter* parameter = create(
        ownerKey,
        key,
        name,
        Parameter::Type::Double);

    if (parameter == nullptr)
        return false;

    parameter->value.number = &value;

    parameter->data.number.minimum = minimum;
    parameter->data.number.maximum = maximum;
    parameter->data.number.step = step;
    parameter->data.number.decimals = decimals;
    parameter->data.number.unit = unit;

    return true;
}

Parameter* ParameterList::create(
    const char* ownerKey,
    const char* key,
    const char* name,
    Parameter::Type type)
{
    if (!isInitialized())
        return nullptr;

    if (isFull())
        return nullptr;

    if (!isValidText(ownerKey) ||
        !isValidText(key) ||
        !isValidText(name))
    {
        return nullptr;
    }

    /*
     * Seule la paire ownerKey/key doit être unique.
     *
     * Deux thermostats peuvent donc avoir chacun
     * un paramètre nommé "setpoint".
     */
    if (find(ownerKey, key) != nullptr)
        return nullptr;

    Parameter& parameter =
        parameters[parameterCount];

    /*
     * Efface les anciennes informations de cette case,
     * notamment après un clear() suivi d'un nouvel ajout.
     */
    parameter = Parameter{};

    parameter.ownerKey = ownerKey;
    parameter.key = key;
    parameter.name = name;
    parameter.type = type;

    parameterCount++;

    return &parameter;
}

bool ParameterList::isValidText(const char* text)
{
    return text != nullptr &&
           text[0] != '\0';
}