#ifndef MENU_BUILDER_H
#define MENU_BUILDER_H

#include <Arduino.h>

#include <Hardware/pinout.h>

class MenuBuilder
{
public:
    using GroupId = uint8_t;

    static constexpr GroupId INVALID_GROUP = UINT8_MAX;
    static constexpr size_t MAX_GROUPS = MAX_PARAMETERS;
    static constexpr size_t MAX_OWNER_BINDINGS = MAX_PARAMETERS;

    static_assert(
        MAX_GROUPS < INVALID_GROUP,
        "MenuBuilder::GroupId is too small for MAX_GROUPS");

    struct Group
    {
        const char* name = nullptr;
        GroupId parent = INVALID_GROUP;
    };

    struct OwnerBinding
    {
        const char* ownerKey = nullptr;
        GroupId group = INVALID_GROUP;
    };

    bool begin(const char* rootName);

    GroupId root() const;

    GroupId addSubmenu(
        GroupId parent,
        const char* name);

    bool addParameters(
        GroupId group,
        const char* ownerKey);

    bool isInitialized() const;

    size_t groupCount() const;
    size_t ownerBindingCount() const;

    const Group* getGroup(GroupId id) const;

    const OwnerBinding* getOwnerBinding(
        size_t index) const;

    GroupId findGroupForOwner(
        const char* ownerKey) const;

private:
    static bool isValidText(const char* text);

    Group groups[MAX_GROUPS] = {};
    OwnerBinding ownerBindings[MAX_OWNER_BINDINGS] = {};

    size_t groupsUsed = 0;
    size_t ownerBindingsUsed = 0;
};

#endif
