#ifndef ALARM_DISPLAY_H
#define ALARM_DISPLAY_H

#include <cstddef>
#include <cstdint>

class Adafruit_GFX;
class ProcessSnapshot;

/**
 * Bandeau d'alarme des écrans d'accueil :
 *  - "ALARME <nom>" pour une alarme, "<n> ALARMES" pour plusieurs ;
 *  - rouge si une alarme est en cours, orange si elles sont seulement
 *    mémorisées (cause disparue, acquittement attendu) ;
 *  - vide sans alarme.
 */
namespace AlarmDisplay
{
    constexpr uint16_t COLOR_ACTIVE = 0xF800;   // rouge
    constexpr uint16_t COLOR_LATCHED = 0xFD20;  // orange

    // Largeur du bandeau en taille 2 : 18 caractères (216 px).
    constexpr size_t BANNER_CHARS = 18;

    /**
     * Texte CP437 du bandeau, tronqué à maxChars, et sa couleur.
     * @return Longueur du texte (0 sans alarme)
     */
    size_t format(
        const ProcessSnapshot& snapshot,
        char* buffer,
        size_t size,
        size_t maxChars,
        uint16_t& color);

    /** Bandeau centré en taille 2 à la hauteur y (efface sans alarme). */
    void printBanner(
        Adafruit_GFX& display,
        const ProcessSnapshot& snapshot,
        int16_t y);
}

#endif
