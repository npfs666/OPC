#ifndef SECOND_ORDER_FILTER_H
#define SECOND_ORDER_FILTER_H

#include <cmath>

/**
 * Filtre passe-bas numérique du 2e ordre : deux étages du 1er ordre
 * identiques en cascade, H(s) = 1 / (1 + τs)², comme le filtre d'entrée des
 * régulateurs compacts (constante τ de 0 à 100 s).
 *
 * Chaque étage : y ← y + α (x − y), avec α = dt / (τ + dt). Le pas dt est le
 * temps réellement écoulé, la cadence de mesure pouvant varier.
 */
class SecondOrderFilter
{
public:
    /** Oublie l'historique : la prochaine valeur est reprise telle quelle. */
    void reset()
    {
        started = false;
    }

    bool isStarted() const
    {
        return started;
    }

    /**
     * @param input Nouvelle valeur (finie)
     * @param dtSeconds Temps écoulé depuis la valeur précédente
     * @param timeConstant τ en secondes ; 0 ou moins : pas de filtrage
     * @return Valeur filtrée
     */
    double_t update(double_t input, double_t dtSeconds, double_t timeConstant)
    {
        if (!started || !(timeConstant > 0.0) || !(dtSeconds >= 0.0))
        {
            firstStage = input;
            secondStage = input;
            started = true;
            return input;
        }

        const double_t alpha = dtSeconds / (timeConstant + dtSeconds);

        firstStage += alpha * (input - firstStage);
        secondStage += alpha * (firstStage - secondStage);

        return secondStage;
    }

private:
    double_t firstStage = 0.0;
    double_t secondStage = 0.0;
    bool started = false;
};

#endif
