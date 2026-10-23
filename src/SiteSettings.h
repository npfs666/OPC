// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SITESETTINGS_H
#define SITESETTINGS_H

#include <cstdint>

/**
 * Réglages du lieu d'installation (Divers > Site), propres à la carte :
 * sauvegardés dans /board.json, conservés quand l'installation change.
 */
struct SiteSettings
{
    static constexpr int32_t MIN_ALTITUDE = -500;
    static constexpr int32_t MAX_ALTITUDE = 5000;

    // Altitude du lieu (m), pour la pression atmosphérique (PressureAltitude).
    int32_t altitude = 0;
};

#endif
