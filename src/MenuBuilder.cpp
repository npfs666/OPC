#include <MenuBuilder.h>

#include <cstring>

bool MenuBuilder::build(
    const ParameterList& parameters,
    const char* rootName)
{
    if (parameters.hasError())
        return false;

    if (!begin(rootName))
        return false;

    for (size_t i = 0; i < parameters.count(); i++)
    {
        const Parameter* parameter =
            parameters.get(i);

        if (parameter == nullptr ||
            !isValidText(parameter->categoryKey) ||
            !isValidText(parameter->categoryName) ||
            !isValidText(parameter->ownerKey) ||
            !isValidText(parameter->ownerName))
        {
            reset();
            return false;
        }

        GroupId category = findSubmenu(
            root(),
            parameter->categoryKey);

        if (category == INVALID_GROUP)
        {
            category = addSubmenu(
                root(),
                parameter->categoryKey,
                parameter->categoryName);

            if (category == INVALID_GROUP)
            {
                reset();
                return false;
            }
        }
        else
        {
            const Group* categoryGroup =
                getGroup(category);

            if (categoryGroup == nullptr ||
                !haveSameText(
                    categoryGroup->name,
                    parameter->categoryName))
            {
                reset();
                return false;
            }
        }

        GroupId owner = findSubmenu(
            category,
            parameter->ownerKey);

        if (owner == INVALID_GROUP)
        {
            /*
             * Un ownerKey doit identifier un seul objet,
             * même si deux catégories utilisent des noms similaires.
             */
            if (findGroupForOwner(
                    parameter->ownerKey) != INVALID_GROUP)
            {
                reset();
                return false;
            }

            owner = addSubmenu(
                category,
                parameter->ownerKey,
                parameter->ownerName);

            if (owner == INVALID_GROUP ||
                !addParameters(
                    owner,
                    parameter->ownerKey))
            {
                reset();
                return false;
            }
        }
        else
        {
            const Group* ownerGroup =
                getGroup(owner);

            if (ownerGroup == nullptr ||
                !haveSameText(
                    ownerGroup->name,
                    parameter->ownerName) ||
                findGroupForOwner(
                    parameter->ownerKey) != owner)
            {
                reset();
                return false;
            }
        }
    }

    if (ownerBindingsUsed == 0)
    {
        reset();
        return false;
    }

    return true;
}

bool MenuBuilder::begin(const char* rootName)
{
    reset();

    if (!isValidText(rootName))
        return false;

    groups[0] = Group{
        rootName,
        rootName,
        INVALID_GROUP
    };

    groupsUsed = 1;

    return true;
}

MenuBuilder::GroupId MenuBuilder::root() const
{
    if (!isInitialized())
        return INVALID_GROUP;

    return 0;
}

MenuBuilder::GroupId MenuBuilder::addSubmenu(
    GroupId parent,
    const char* name)
{
    return addSubmenu(
        parent,
        name,
        name);
}

MenuBuilder::GroupId MenuBuilder::addSubmenu(
    GroupId parent,
    const char* key,
    const char* name)
{
    if (!isInitialized())
        return INVALID_GROUP;

    if (getGroup(parent) == nullptr)
        return INVALID_GROUP;

    if (!isValidText(key) ||
        !isValidText(name))
        return INVALID_GROUP;

    if (groupsUsed >= MAX_GROUPS)
        return INVALID_GROUP;

    if (findSubmenu(parent, key) != INVALID_GROUP)
        return INVALID_GROUP;

    const GroupId id =
        static_cast<GroupId>(groupsUsed);

    groups[id] = Group{
        key,
        name,
        parent
    };

    groupsUsed++;

    return id;
}

bool MenuBuilder::addParameters(
    GroupId group,
    const char* ownerKey)
{
    if (!isInitialized())
        return false;

    if (getGroup(group) == nullptr)
        return false;

    if (!isValidText(ownerKey))
        return false;

    if (ownerBindingsUsed >= MAX_OWNER_BINDINGS)
        return false;

    if (findGroupForOwner(ownerKey) != INVALID_GROUP)
        return false;

    ownerBindings[ownerBindingsUsed] = OwnerBinding{
        ownerKey,
        group
    };

    ownerBindingsUsed++;

    return true;
}

bool MenuBuilder::isInitialized() const
{
    return groupsUsed > 0;
}

size_t MenuBuilder::groupCount() const
{
    return groupsUsed;
}

size_t MenuBuilder::ownerBindingCount() const
{
    return ownerBindingsUsed;
}

const MenuBuilder::Group* MenuBuilder::getGroup(
    GroupId id) const
{
    if (id == INVALID_GROUP)
        return nullptr;

    if (id >= groupsUsed)
        return nullptr;

    return &groups[id];
}

const MenuBuilder::OwnerBinding*
MenuBuilder::getOwnerBinding(size_t index) const
{
    if (index >= ownerBindingsUsed)
        return nullptr;

    return &ownerBindings[index];
}

MenuBuilder::GroupId MenuBuilder::findGroupForOwner(
    const char* ownerKey) const
{
    if (!isValidText(ownerKey))
        return INVALID_GROUP;

    for (size_t i = 0; i < ownerBindingsUsed; i++)
    {
        const OwnerBinding& binding =
            ownerBindings[i];

        if (binding.ownerKey == nullptr)
            continue;

        if (std::strcmp(
                binding.ownerKey,
                ownerKey) == 0)
        {
            return binding.group;
        }
    }

    return INVALID_GROUP;
}

MenuBuilder::GroupId MenuBuilder::findSubmenu(
    GroupId parent,
    const char* key) const
{
    if (getGroup(parent) == nullptr ||
        !isValidText(key))
    {
        return INVALID_GROUP;
    }

    for (size_t i = 1; i < groupsUsed; i++)
    {
        const Group& group = groups[i];

        if (group.parent != parent ||
            group.key == nullptr)
        {
            continue;
        }

        if (std::strcmp(group.key, key) == 0)
            return static_cast<GroupId>(i);
    }

    return INVALID_GROUP;
}

void MenuBuilder::reset()
{
    groupsUsed = 0;
    ownerBindingsUsed = 0;
}

bool MenuBuilder::isValidText(const char* text)
{
    return text != nullptr &&
           text[0] != '\0';
}

bool MenuBuilder::haveSameText(
    const char* first,
    const char* second)
{
    return isValidText(first) &&
           isValidText(second) &&
           std::strcmp(first, second) == 0;
}
