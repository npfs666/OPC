// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HOMESCREEN_H
#define HOMESCREEN_H

#include <cstdint>

class Adafruit_GFX;
class ProcessSnapshot;

struct HomeScreenContext
{
    Adafruit_GFX& display;
    const ProcessSnapshot& snapshot;
    uint32_t now;
    bool fullRefresh;

    /*
     * Consigne en cours de réglage à l'encodeur (Installation::homeSetpoint) :
     * l'afficher à la place de la consigne, en couleur d'édition.
     */
    bool editingSetpoint = false;
    double editedSetpoint = 0.0;
};

#endif
