// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONDITION_ALARM_H
#define CONDITION_ALARM_H

#include <Regulator/Alarm.h>

class DigitalInput;
class ProcessControl;

/**
 * Alarme sur une condition : une entrée TOR (porte ouverte...) liée dans
 * begin(), ou une condition écrite par la glue. Réglages : Active, Tempo,
 * Mémorisation. L'enregistrer avec process.add(alarme).
 *
 * - Entrée TOR : alarme quand l'entrée est active (la polarité se règle sur
 *   l'entrée). Une entrée invalide est un défaut, signalé comme une alarme ;
 *   pas avant sa première lecture valide (démarrage, retour du menu).
 * - Glue : set(condition) à chaque cycle. La condition est évaluée juste
 *   après la glue. Une condition non écrite est un défaut, signalé comme une
 *   alarme et journalisé.
 * - Un défaut passe par la tempo, comme la condition, mais n'est jamais
 *   masqué par une inhibition (allowInhibit(), voir Alarm).
 */
class ConditionAlarm : public Alarm
{
public:
    struct Settings
    {
        bool enabled = false;

        // Durée minimale de la condition avant l'alarme, en secondes.
        uint32_t delay = 0;

        // L'alarme reste signalée jusqu'à l'acquittement.
        bool latching = false;
    };

    Settings settings;

    /** Condition écrite par la glue (set()). */
    void begin(
        const char* key,
        const char* name);

    /** Condition = entrée TOR active, sans glue. */
    void begin(
        const char* key,
        const char* name,
        const DigitalInput& input);

    /** Glue : condition de ce cycle. */
    void set(bool condition);

    bool isEnabled() const override;

    /** Entrée TOR : évalue la condition. Glue : rien (voir applyLogic()). */
    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    void registerParameters(ParameterList& list) override;

private:
    friend class ProcessControl;

    /**
     * Appelé par ProcessControl après la glue. Vrai au début d'un oubli
     * d'écriture, à journaliser une fois.
     */
    bool applyLogic(uint32_t now);

    /** Tempo, inhibition et mémorisation. */
    void evaluate(bool condition, bool fault, uint32_t now);

    void restart();

    const DigitalInput* input = nullptr;

    // Entrée lue valide au moins une fois depuis le démarrage ou le menu.
    bool inputSeen = false;

    // Glue : condition écrite pendant le cycle en cours.
    bool written = false;
    bool requested = false;

    // Oubli d'écriture en cours, déjà journalisé.
    bool missing = false;

    // Tempo : condition présente depuis pendingSince ; une fois confirmée,
    // elle le reste (pas de nouvelle comparaison après 49 jours).
    bool pending = false;
    bool confirmed = false;
    uint32_t pendingSince = 0;
};

#endif
