#ifndef PHYSICS_THERMOCOUPLE_H
#define PHYSICS_THERMOCOUPLE_H

#include <cmath>
#include <cstdint>

namespace Physics
{
namespace Thermocouple
{
    // Valeurs explicites pour les futurs paramètres persistants.
    enum class Type : uint8_t
    {
        B = 0,
        E = 1,
        J = 2,
        K = 3,
        N = 4,
        R = 5,
        S = 6,
        T = 7
    };

    struct Range
    {
        double minimum;
        double maximum;
    };

    const char* typeName(Type type);

    // Domaine des fonctions directes NIST, utilisé aussi pour la jonction froide.
    // Les températures sont en °C. Un type inconnu renvoie {NAN, NAN}.
    Range referenceRange(Type type);

    // Domaine accepté pour la température de la pointe.
    // B : 250 à 1820 °C (pas d'inversion ambiguë près de l'ambiante).
    // Autres types : domaine complet de la fonction directe NIST.
    Range measurementRange(Type type);

    // Tension référencée à une jonction froide à 0 °C.
    // NAN sur type inconnu, valeur non finie ou hors domaine ; pas d'extrapolation.
    double temperatureToMillivolts(Type type, double temperatureC);
    double millivoltsToTemperature(Type type, double voltageMv);

    // E(pointe) = tension mesurée + E(jonction froide).
    // coldJunctionC doit déjà contenir l'éventuelle correction de jonction froide.
    double compensatedTemperature(
        Type type,
        double voltageMv,
        double coldJunctionC);
}
}

#endif
