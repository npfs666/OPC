// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DELAY_TIMER_H
#define DELAY_TIMER_H

#include <Regulator/Regulator.h>

/**
 * Temporisation réglable au menu. Sa commande vaut 1 quand la sortie est
 * active, 0 sinon.
 *
 * - Retard à la montée (OnDelay) : sortie active quand l'entrée est vraie
 *   sans interruption depuis le délai. Entrée fausse : sortie inactive et
 *   délai remis à zéro.
 * - Retard à la descente (OffDelay) : sortie active dès que l'entrée est
 *   vraie, et encore pendant le délai après sa retombée (post-circulation).
 *
 * Deux usages :
 * - piloté par la glue : run(entrée, now) à chaque cycle, puis isOn() ;
 * - relié dans begin() à un régulateur (setSource()) : entrée = commande du
 *   régulateur (>= 0,5). Une commande invalide rend la temporisation
 *   invalide et la remet à zéro. On peut alors la relier à un actionneur.
 *
 * Une pause (menu, timeout) ne remet pas le délai à zéro : le temps continue
 * de compter, un réglage au menu ne repousse pas une échéance de plusieurs
 * heures. La commande reste invalide jusqu'au prochain calcul.
 */
class DelayTimer : public Regulator
{
public:
    enum class Mode : uint8_t
    {
        OnDelay,
        OffDelay
    };

    /** Unité du délai dans le menu. */
    enum class Unit : uint8_t
    {
        Seconds,
        Minutes,
        Hours
    };

    struct Settings
    {
        /* Délai, dans l'unité choisie. */
        uint32_t delay = 0;
    };

    Settings settings;

    void begin(
        const char* key,
        const char* name,
        Mode mode,
        uint32_t delay,
        Unit unit = Unit::Seconds);

    /**
     * Entrée prise sur la commande d'un régulateur (comparateur, thermostat,
     * alarme...), enregistré avant la temporisation.
     */
    void setSource(const Regulator& source);

    /**
     * Délai maximal dans le menu. Par défaut : 3600 s, 1440 min ou 48 h.
     * Borné pour que le délai tienne en millisecondes sur 31 bits.
     */
    void setMaximum(uint32_t maximum);

    /** Libellé du délai dans le menu (par défaut « Délai »). */
    void setLabel(const char* label);

    /** Voir Comparator::setMenuParent(). */
    void setMenuParent(const char* ownerKey);

    /** Glue : nouvelle valeur de l'entrée. Retourne l'état de la sortie. */
    bool run(bool input, uint32_t now);

    /** Commande valide et sortie active. */
    bool isOn() const;

    /** Délai en cours : temps restant avant le changement de la sortie. */
    uint32_t remainingMs(uint32_t now) const;

    /** Remet à zéro : entrée et sortie inactives, plus de délai en cours. */
    void reset();

    /** Avec une source : calcule la sortie. Sans source : rien (glue). */
    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    void registerParameters(ParameterList& list) override;

private:
    uint32_t delayMs() const;
    uint32_t unitMs() const;

    Mode mode = Mode::OnDelay;
    Unit unit = Unit::Seconds;
    uint32_t maximum = 3600;

    const Regulator* source = nullptr;

    const char* label = "Délai";
    const char* menuParent = nullptr;

    // Entrée connue depuis le dernier changement, à l'instant changedAt.
    bool started = false;
    bool input = false;
    uint32_t changedAt = 0;

    // Retard à la descente : entrée vraie, ou délai de retombée en cours.
    bool armed = false;

    bool output = false;
};

#endif
