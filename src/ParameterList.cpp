#include <ParameterList.h>

#include <cstring>

ParameterList::Writer::Writer(
    ParameterList& list,
    const ParameterOwner& owner)
    : list(&list),
      owner(owner)
{
}

bool ParameterList::Writer::addBool(
    const char* key,
    const char* name,
    bool& value)
{
    return list != nullptr &&
           list->remember(
               list->addBool(
                   owner,
                   key,
                   name,
                   value));
}

bool ParameterList::Writer::addDouble(
    const char* key,
    const char* name,
    double_t& value,
    double_t minimum,
    double_t maximum,
    double_t step,
    uint8_t decimals,
    const char* unit)
{
    return list != nullptr &&
           list->remember(
               list->addDouble(
                   owner,
                   key,
                   name,
                   value,
                   minimum,
                   maximum,
                   step,
                   decimals,
                   unit));
}

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
    selectionOptionCount = 0;
    registrationError = false;
}

void ParameterList::clear()
{
    parameterCount = 0;
    selectionOptionCount = 0;
    registrationError = false;
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

bool ParameterList::hasError() const
{
    return registrationError;
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

ParameterList::Writer ParameterList::forOwner(
    const ParameterOwner& owner)
{
    return Writer(*this, owner);
}

bool ParameterList::addBool(
    const ParameterOwner& owner,
    const char* key,
    const char* name,
    bool& value)
{
    Parameter* parameter = create(
        owner,
        key,
        name,
        Parameter::Type::Bool);

    if (parameter == nullptr)
        return false;

    parameter->value.boolean = &value;

    return true;
}

bool ParameterList::addInteger(
    const ParameterOwner& owner,
    const char* key,
    const char* name,
    const ParameterDiscreteBinding& binding,
    int32_t minimum,
    int32_t maximum,
    int32_t step,
    const char* unit)
{
    if (binding.target == nullptr ||
        binding.read == nullptr ||
        binding.write == nullptr)
    {
        return false;
    }

    if (minimum > maximum)
        return false;

    if (step <= 0)
        return false;

    Parameter* parameter = create(
        owner,
        key,
        name,
        Parameter::Type::Integer);

    if (parameter == nullptr)
        return false;

    parameter->discrete = binding;

    parameter->data.integer.minimum = minimum;
    parameter->data.integer.maximum = maximum;
    parameter->data.integer.step = step;
    parameter->data.integer.unit = unit;

    return true;
}

bool ParameterList::addDouble(
    const ParameterOwner& owner,
    const char* key,
    const char* name,
    double_t& value,
    double_t minimum,
    double_t maximum,
    double_t step,
    uint8_t decimals,
    const char* unit)
{
    if (!(minimum <= maximum))
        return false;

    if (!(step > 0.0))
        return false;

    if (decimals > 3)
        return false;

    if (!(value >= minimum &&
          value <= maximum))
    {
        return false;
    }

    Parameter* parameter = create(
        owner,
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

bool ParameterList::addSelection(
    const ParameterOwner& owner,
    const char* key,
    const char* name,
    const ParameterDiscreteBinding& binding,
    const ParameterOption* options,
    uint8_t optionCount)
{
    if (binding.target == nullptr ||
        binding.read == nullptr ||
        binding.write == nullptr)
    {
        return false;
    }

    if (options == nullptr ||
        optionCount == 0)
    {
        return false;
    }

    if (selectionOptionCount +
            optionCount >
        MAX_SELECTION_OPTIONS)
    {
        return false;
    }

    const int32_t currentValue =
        binding.read(binding.target);

    bool currentValueFound = false;

    for (uint8_t i = 0; i < optionCount; i++)
    {
        if (!isValidText(options[i].name))
            return false;

        if (options[i].value == currentValue)
            currentValueFound = true;

        for (uint8_t j = 0; j < i; j++)
        {
            if (options[j].value ==
                options[i].value)
            {
                return false;
            }
        }
    }

    if (!currentValueFound)
        return false;

    Parameter* parameter = create(
        owner,
        key,
        name,
        Parameter::Type::Selection);

    if (parameter == nullptr)
        return false;

    parameter->discrete = binding;
    parameter->data.selection.options = options;
    parameter->data.selection.count =
        optionCount;

    selectionOptionCount += optionCount;

    return true;
}

Parameter* ParameterList::create(
    const ParameterOwner& owner,
    const char* key,
    const char* name,
    Parameter::Type type)
{
    if (!isInitialized())
        return nullptr;

    if (isFull())
        return nullptr;

    if (!isValidOwner(owner) ||
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
    if (find(owner.ownerKey, key) != nullptr)
        return nullptr;

    Parameter& parameter =
        parameters[parameterCount];

    /*
     * Efface les anciennes informations de cette case,
     * notamment après un clear() suivi d'un nouvel ajout.
     */
    parameter = Parameter{};

    parameter.categoryKey = owner.categoryKey;
    parameter.categoryName = owner.categoryName;
    parameter.ownerKey = owner.ownerKey;
    parameter.ownerName = owner.ownerName;
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

bool ParameterList::isValidOwner(
    const ParameterOwner& owner)
{
    return isValidText(owner.categoryKey) &&
           isValidText(owner.categoryName) &&
           isValidText(owner.ownerKey) &&
           isValidText(owner.ownerName);
}

bool ParameterList::remember(bool result)
{
    if (!result)
        registrationError = true;

    return result;
}
