// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HOME_SETPOINT_EDITOR_H
#define HOME_SETPOINT_EDITOR_H

#include <cmath>
#include <cstdint>

/**
 * Édition de la consigne à l'encodeur depuis l'écran d'accueil (cœur UI),
 * sur le modèle des touches ▲/▼ des régulateurs compacts :
 *  - chaque cran change la valeur d'un pas ; des crans rapprochés (rotation
 *    rapide) changent de dix pas ;
 *  - la valeur reste dans les limites du paramètre, sur la grille du pas ;
 *  - seul un clic valide : sans action, le réglage est abandonné.
 */
class HomeSetpointEditor
{
public:
    // Crans plus rapprochés que ce délai : accélération.
    static constexpr uint32_t FAST_DETENT_MS = 60;
    static constexpr int32_t FAST_FACTOR = 10;

    void begin(
        double_t value,
        double_t minimum,
        double_t maximum,
        double_t step,
        uint32_t now);

    /** Applique detents crans (signe = sens de rotation). */
    void rotate(int32_t detents, uint32_t now);

    bool isActive() const
    {
        return active;
    }

    /** Aucune rotation depuis timeoutMs : le réglage est à abandonner. */
    bool inactiveFor(uint32_t now, uint32_t timeoutMs) const
    {
        return active && now - lastActivity >= timeoutMs;
    }

    bool hasChanged() const
    {
        return active && editedValue != initialValue;
    }

    double_t value() const
    {
        return editedValue;
    }

    void end()
    {
        active = false;
    }

private:
    double_t initialValue = 0.0;
    double_t editedValue = 0.0;
    double_t minimum = 0.0;
    double_t maximum = 0.0;
    double_t step = 0.1;

    uint32_t lastActivity = 0;
    uint32_t lastDetent = 0;
    bool hasDetent = false;
    bool active = false;
};

#endif
