// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PRESSUREALTITUDE_H
#define PRESSUREALTITUDE_H

#include <Measurements/Pressure/Pressure.h>

struct SiteSettings;

/**
 * Pression atmosphérique fixe, calculée depuis l'altitude du lieu
 * (Divers > Site > Altitude) avec l'atmosphère normalisée, sans capteur.
 *
 * La météo (±30 hPa) n'est pas suivie : pour le psychromètre, l'erreur reste
 * sous 0,5 point d'HR, bien moins que celle du bulbe humide.
 */
class PressureAltitude : public Pressure
{
public:
    PressureAltitude() = default;

    /**
     * @param[in] name Nom et clé de la mesure (const char*).
     * @param[in] site Réglages du lieu, en général process.site().
     */
    void begin(
        const char* name,
        const SiteSettings& site);

    void update() override;

    uint8_t printDecimals() const override;

    /**
     * @param[in] altitude Altitude (m).
     * @return Pression de l'atmosphère normalisée à cette altitude (Pa).
     */
    static double_t standardPressure(double_t altitude);

private:
    const SiteSettings* site = nullptr;
};

#endif
