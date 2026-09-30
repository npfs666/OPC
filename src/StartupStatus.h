#ifndef STARTUP_STATUS_H
#define STARTUP_STATUS_H

#include <cstdint>

/**
 * Cause d'un démarrage refusé. Affichée à l'écran par le cœur UI
 * et répétée sur le port série.
 */
enum class StartupError : uint8_t
{
    None,
    SensorBoard,
    BMP580,
    ParameterStorage,
    Installation,
    ParameterRegistration,
    Outputs
};

struct StartupErrorText
{
    const char* title;
    const char* detail;
};

inline StartupErrorText startupErrorText(StartupError error)
{
    switch (error)
    {
    case StartupError::None:
        return {"Aucune erreur", ""};

    case StartupError::SensorBoard:
        return {"Carte de mesure", "MCP23017 ne répond pas"};

    case StartupError::BMP580:
        return {"Capteur pression", "BMP580 ne répond pas"};

    case StartupError::ParameterStorage:
        return {"Paramètres", "Liste non initialisée"};

    case StartupError::Installation:
        return {"Installation", "Voir le port série"};

    case StartupError::ParameterRegistration:
        return {"Paramètres", "Trop de paramètres ou clé en double"};

    case StartupError::Outputs:
        return {"Sorties", "Initialisation impossible"};
    }

    return {"Erreur inconnue", ""};
}

#endif
