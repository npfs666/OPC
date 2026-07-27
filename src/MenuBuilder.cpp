#include <MenuBuilder.h>

#include <cstring>

bool MenuBuilder::begin(const char* rootName)
{
    groupsUsed = 0;
    ownerBindingsUsed = 0;

    if (!isValidText(rootName))
        return false;

    groups[0] = Group{
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
    if (!isInitialized())
        return INVALID_GROUP;

    if (getGroup(parent) == nullptr)
        return INVALID_GROUP;

    if (!isValidText(name))
        return INVALID_GROUP;

    if (groupsUsed >= MAX_GROUPS)
        return INVALID_GROUP;

    const GroupId id =
        static_cast<GroupId>(groupsUsed);

    groups[id] = Group{
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

bool MenuBuilder::isValidText(const char* text)
{
    return text != nullptr &&
           text[0] != '\0';
}
