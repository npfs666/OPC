#include <Installation.h>

#include <hmi/MenuBuilder.h>

#include <cstring>

namespace
{
    bool sameText(
        const char* first,
        const char* second)
    {
        return first != nullptr &&
               second != nullptr &&
               std::strcmp(first, second) == 0;
    }
}

bool Installation::buildMenu(
    MenuBuilder& menu) const
{
    /*
     * Input
     *   Entrées physiques
     *   Timeout mesures
     * Autres catégories
     * Divers
     *   Calibration
     *   Menu
     */
    if (parameterList.hasError() ||
        !menu.begin("Parametres"))
    {
        return false;
    }

    const MenuBuilder::GroupId root =
        menu.root();

    const MenuBuilder::GroupId inputs =
        menu.addSubmenu(
            root,
            "inputs",
            "Input");

    if (inputs == MenuBuilder::INVALID_GROUP)
    {
        return false;
    }

    auto addOwner = [&menu](
        MenuBuilder::GroupId parent,
        const Parameter& parameter,
        bool createSubmenu)
    {
        MenuBuilder::GroupId group = parent;

        if (createSubmenu)
        {
            group = menu.addSubmenu(
                parent,
                parameter.ownerKey,
                parameter.ownerName);
        }

        return group != MenuBuilder::INVALID_GROUP &&
               menu.addParameters(
                   group,
                   parameter.ownerKey);
    };

    for (size_t i = 0;
         i < parameterList.count();
         i++)
    {
        const Parameter* parameter =
            parameterList.get(i);

        if (parameter == nullptr)
            return false;

        if (menu.findGroupForOwner(
                parameter->ownerKey) !=
            MenuBuilder::INVALID_GROUP)
        {
            continue;
        }

        if (sameText(
                parameter->categoryKey,
                "calibration") ||
            sameText(
                parameter->ownerKey,
                "menu"))
        {
            continue;
        }

        if (sameText(
                parameter->ownerKey,
                "measurement_watchdog"))
        {
            if (!addOwner(
                    inputs,
                    *parameter,
                    false))
            {
                return false;
            }

            continue;
        }

        if (sameText(
                parameter->categoryKey,
                "inputs"))
        {
            if (!addOwner(
                    inputs,
                    *parameter,
                    true))
            {
                return false;
            }

            continue;
        }

        MenuBuilder::GroupId category =
            menu.findSubmenu(
                root,
                parameter->categoryKey);

        if (category == MenuBuilder::INVALID_GROUP)
        {
            category = menu.addSubmenu(
                root,
                parameter->categoryKey,
                parameter->categoryName);
        }

        if (category == MenuBuilder::INVALID_GROUP ||
            !addOwner(
                category,
                *parameter,
                true))
        {
            return false;
        }
    }

    const MenuBuilder::GroupId miscellaneous =
        menu.addSubmenu(
            root,
            "miscellaneous",
            "Divers");

    if (miscellaneous ==
        MenuBuilder::INVALID_GROUP)
    {
        return false;
    }

    const MenuBuilder::GroupId calibration =
        menu.addSubmenu(
            miscellaneous,
            "calibration",
            "Calibration");

    const MenuBuilder::GroupId menuSettings =
        menu.addSubmenu(
            miscellaneous,
            "menu",
            "Menu");

    if (calibration ==
            MenuBuilder::INVALID_GROUP ||
        menuSettings ==
            MenuBuilder::INVALID_GROUP)
    {
        return false;
    }

    for (size_t i = 0;
         i < parameterList.count();
         i++)
    {
        const Parameter* parameter =
            parameterList.get(i);

        if (parameter == nullptr)
            return false;

        if (menu.findGroupForOwner(
                parameter->ownerKey) !=
            MenuBuilder::INVALID_GROUP)
        {
            continue;
        }

        if (sameText(
                parameter->categoryKey,
                "calibration"))
        {
            if (!addOwner(
                    calibration,
                    *parameter,
                    true))
            {
                return false;
            }
        }
        else if (sameText(
                     parameter->ownerKey,
                     "menu"))
        {
            if (!addOwner(
                    menuSettings,
                    *parameter,
                    false))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    return addMenuActions(menu);
}

bool Installation::prepareParameterRegistration()
{
    parameterList.begin(
        parameterStorage,
        MAX_PARAMETERS);

    parameterList.clear();

    return parameterList.isInitialized();
}

bool Installation::completeParameterRegistration()
{
    auto menuParameters = parameterList.forOwner({
        "interface",
        "Interface",
        "menu",
        "Menu"
    });

    if (!menuParameters.addInteger(
            "inactivity_timeout",
            "Timeout",
            menuSettings.inactivityTimeoutSeconds,
            1,
            120,
            1,
            "s"))
    {
        return false;
    }

    auto safetyParameters = parameterList.forOwner({
        "safety",
        "Sécurité",
        "measurement_watchdog",
        "Surveillance mesures"
    });

    return safetyParameters.addInteger(
        "measurement_timeout",
        "Timeout mesures",
        safetySettings.measurementTimeoutSeconds,
        10,
        300,
        1,
        "s");
}
