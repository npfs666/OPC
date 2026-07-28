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
    auto parameters = parameterList.forOwner({
        "interface",
        "Interface",
        "menu",
        "Menu"
    });

    return parameters.addInteger(
        "inactivity_timeout",
        "Timeout",
        menuSettings.inactivityTimeoutSeconds,
        1,
        120,
        1,
        "s");
}
