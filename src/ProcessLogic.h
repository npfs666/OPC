#ifndef PROCESS_LOGIC_H
#define PROCESS_LOGIC_H

#include <cstdint>

/**
 * Glue d'une installation : la logique propre à l'application, exécutée par
 * ProcessControl à chaque cycle de mesure, après les régulateurs et les
 * alarmes, avant les actionneurs. Implémentée par Installation.
 *
 * Elle n'est pas appelée pendant une pause du menu ni après un timeout de
 * mesure : comme le reste du cycle, elle attend la mesure suivante.
 */
class ProcessLogic
{
public:
    virtual ~ProcessLogic() = default;

    /**
     * Cœur contrôle, sous processDataMutex. Non bloquante : pas de delay(),
     * d'écriture de fichier ni de longues sorties série. Écrit chaque
     * LogicCommand de l'installation à chaque appel.
     */
    virtual void processLogic(uint32_t now) = 0;

    /** Reprise après une pause (menu, timeout) : réarmer l'état de la glue. */
    virtual void resumeLogic(uint32_t now) = 0;
};

#endif
