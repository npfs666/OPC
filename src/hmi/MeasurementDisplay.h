#ifndef MEASUREMENT_DISPLAY_H
#define MEASUREMENT_DISPLAY_H

#include <cstddef>
#include <cstdint>

class Adafruit_GFX;
struct MeasurementSample;

/**
 * Affichage d'une mesure du snapshot : la valeur suivie de son unité si elle
 * est exploitable, sinon le libellé de son état ("RUPTURE", "TROP HAUT"...),
 * sans unité et dans la couleur de l'état.
 */
namespace MeasurementDisplay
{
    constexpr uint16_t COLOR_NOT_READY = 0x8410;    // gris
    constexpr uint16_t COLOR_OUT_OF_RANGE = 0xFD20; // orange
    constexpr uint16_t COLOR_FAULT = 0xF800;        // rouge

    /**
     * Texte CP437 prêt à afficher. maxLabelLength est la place disponible
     * pour un libellé d'état : au-delà, sa forme courte (4 caractères au
     * plus) est utilisée. Une mesure absente du snapshot (aucun cycle de
     * mesure depuis le démarrage ou le menu) est en attente : "...".
     *
     * @return Longueur du texte écrit
     */
    size_t format(
        const MeasurementSample* sample,
        char* buffer,
        size_t size,
        size_t maxLabelLength = SIZE_MAX);

    /** valueColor si la mesure est exploitable, sinon la couleur de l'état. */
    uint16_t color(
        const MeasurementSample* sample,
        uint16_t valueColor);

    /** Écrit au curseur courant ; la taille du texte est déjà réglée. */
    void print(
        Adafruit_GFX& display,
        const MeasurementSample* sample,
        uint16_t valueColor,
        uint16_t backgroundColor,
        size_t maxLabelLength = SIZE_MAX);
}

#endif
