// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MEASUREMENT_STATUS_H
#define MEASUREMENT_STATUS_H

#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * État d'une mesure. Seul Ok porte une valeur exploitable.
 */
enum class MeasurementStatus : uint8_t
{
    NotReady,   // aucune mesure depuis le démarrage
    Ok,
    Open,       // rupture du capteur ou de la ligne
    Short,      // court-circuit
    UnderRange, // sous l'étendue de mesure
    OverRange,  // au-dessus de l'étendue de mesure
    Invalid     // autre défaut : câblage, calcul impossible, entrée amont...
};

/**
 * Libellé ASCII de l'état : le libellé long (10 caractères au plus) s'il
 * tient dans maxLength, sinon le libellé court (4 caractères au plus).
 */
inline const char* measurementStatusLabel(
    MeasurementStatus status,
    size_t maxLength = SIZE_MAX)
{
    const char* longLabel = "ERREUR";
    const char* shortLabel = "ERR";

    switch (status)
    {
    case MeasurementStatus::NotReady:
        longLabel = "...";
        shortLabel = "...";
        break;

    case MeasurementStatus::Ok:
        longLabel = "OK";
        shortLabel = "OK";
        break;

    case MeasurementStatus::Open:
        longLabel = "RUPTURE";
        shortLabel = "RUPT";
        break;

    case MeasurementStatus::Short:
        longLabel = "C-CIRCUIT";
        shortLabel = "C-C";
        break;

    case MeasurementStatus::UnderRange:
        longLabel = "TROP BAS";
        shortLabel = "BAS";
        break;

    case MeasurementStatus::OverRange:
        longLabel = "TROP HAUT";
        shortLabel = "HAUT";
        break;

    case MeasurementStatus::Invalid:
        break;
    }

    return std::strlen(longLabel) <= maxLength
        ? longLabel
        : shortLabel;
}

#endif
