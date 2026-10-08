// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEXT_FIELD_H
#define TEXT_FIELD_H

#include <cstddef>
#include <cstdint>

class Adafruit_GFX;

/**
 * Champ de texte à largeur fixe pour les écrans d'accueil (police 6 × 8 px
 * multipliée par la taille du texte).
 *
 * Le texte est complété par des espaces et écrit avec un fond : l'ancien
 * contenu est remplacé sans effacement préalable, donc sans scintillement.
 */
namespace TextField
{
    enum class Align : uint8_t
    {
        Left,
        Center,
        Right
    };

    /** Largeur en pixels d'un champ de width caractères. */
    constexpr int16_t pixelWidth(size_t width, uint8_t size)
    {
        return static_cast<int16_t>(width * 6 * size);
    }

    /**
     * Écrit text sur width caractères (47 au plus) à partir de (x, y).
     * Un texte trop long est tronqué.
     */
    void print(
        Adafruit_GFX& display,
        int16_t x,
        int16_t y,
        uint8_t size,
        uint16_t color,
        const char* text,
        size_t width,
        Align align = Align::Left,
        uint16_t background = 0x0000);
}

#endif
