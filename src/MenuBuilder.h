#ifndef MENU_BUILDER_H
#define MENU_BUILDER_H

#include <Arduino.h>

#include <Hardware/pinout.h>
#include <ParameterList.h>

class MenuBuilder
{
public:
    using GroupId = uint8_t;

    static constexpr GroupId INVALID_GROUP = UINT8_MAX;
    static constexpr size_t MAX_GROUPS =
        1 + (2 * MAX_PARAMETERS);
    static constexpr size_t MAX_OWNER_BINDINGS = MAX_PARAMETERS;

    static_assert(
        MAX_GROUPS < INVALID_GROUP,
        "MenuBuilder::GroupId is too small for MAX_GROUPS");

    struct Group
    {
        const char* key = nullptr;
        const char* name = nullptr;
        GroupId parent = INVALID_GROUP;
    };

    struct OwnerBinding
    {
        const char* ownerKey = nullptr;
        GroupId group = INVALID_GROUP;
    };

    bool build(
        const ParameterList& parameters,
        const char* rootName = "Parametres");

    bool begin(const char* rootName);

    GroupId root() const;

    GroupId addSubmenu(
        GroupId parent,
        const char* name);

    GroupId addSubmenu(
        GroupId parent,
        const char* key,
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

    GroupId findSubmenu(
        GroupId parent,
        const char* key) const;

private:
    void reset();

    static bool isValidText(const char* text);
    static bool haveSameText(
        const char* first,
        const char* second);

    Group groups[MAX_GROUPS] = {};
    OwnerBinding ownerBindings[MAX_OWNER_BINDINGS] = {};

    size_t groupsUsed = 0;
    size_t ownerBindingsUsed = 0;
};

#endif
