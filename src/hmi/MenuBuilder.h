#ifndef MENU_BUILDER_H
#define MENU_BUILDER_H

#include <Hardware/pinout.h>

#include <cstddef>
#include <cstdint>

class MenuBuilder
{
public:
    using GroupId = uint8_t;
    using ActionId = uint8_t;

    static constexpr GroupId INVALID_GROUP = UINT8_MAX;
    static constexpr ActionId NO_ACTION = 0;
    static constexpr size_t MAX_GROUPS =
        1 + (2 * MAX_PARAMETERS);
    static constexpr size_t MAX_OWNER_BINDINGS = MAX_PARAMETERS;
    static constexpr size_t MAX_ACTIONS = 16;

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

    struct Action
    {
        ActionId id = NO_ACTION;
        const char* key = nullptr;
        const char* name = nullptr;
        GroupId group = INVALID_GROUP;
    };

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

    bool addAction(
        GroupId group,
        ActionId id,
        const char* key,
        const char* name);

    bool isInitialized() const;

    size_t groupCount() const;
    size_t ownerBindingCount() const;
    size_t actionCount() const;

    const Group* getGroup(GroupId id) const;

    const OwnerBinding* getOwnerBinding(
        size_t index) const;

    const Action* getAction(size_t index) const;

    const Action* findAction(ActionId id) const;

    const Action* findAction(const char* key) const;

    GroupId findGroupForOwner(
        const char* ownerKey) const;

    GroupId findSubmenu(
        GroupId parent,
        const char* key) const;

private:
    void reset();

    static bool isValidText(const char* text);
    Group groups[MAX_GROUPS] = {};
    OwnerBinding ownerBindings[MAX_OWNER_BINDINGS] = {};
    Action actions[MAX_ACTIONS] = {};

    size_t groupsUsed = 0;
    size_t ownerBindingsUsed = 0;
    size_t actionsUsed = 0;
};

#endif
