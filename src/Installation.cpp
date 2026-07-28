#include <Installation.h>

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
        1,
        300,
        1,
        "s");
}
