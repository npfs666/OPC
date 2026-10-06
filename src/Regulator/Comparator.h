#ifndef COMPARATOR_H
#define COMPARATOR_H

#include <Regulator/Regulator.h>

class Measurement;

/**
 * Comparateur à hystérésis, sur une mesure (seuil) ou sur la différence de
 * deux mesures (différentiel). Sa commande vaut 1 en marche, 0 à l'arrêt.
 *
 * - Au-dessus : marche quand la valeur atteint le seuil de marche (>=), arrêt
 *   quand elle redescend au seuil d'arrêt (<=). Entre les deux, l'état est
 *   conservé.
 * - En dessous : l'inverse (marche <= seuil de marche, arrêt >= seuil
 *   d'arrêt).
 * - Seuil unique (useSingleThreshold()) : un seul réglage, sans hystérésis :
 *   marche au-delà du seuil, seuil compris.
 *
 * Une mesure en défaut rend la commande invalide et remet le comparateur à
 * l'arrêt. On peut le relier directement à un actionneur dans begin() (un
 * hors-gel, par exemple), ou le lire depuis la glue (isOn()).
 */
class Comparator : public Regulator
{
public:
    enum class Direction : uint8_t
    {
        Above,
        Below
    };

    struct Settings
    {
        double_t onThreshold = 0.0;
        double_t offThreshold = 0.0;
    };

    Settings settings;

    /** Seuil sur une mesure. */
    void begin(
        const char* key,
        const char* name,
        const Measurement& input,
        Direction direction,
        double_t onThreshold,
        double_t offThreshold);

    /** Différentiel : valeur comparée = first - second. */
    void begin(
        const char* key,
        const char* name,
        const Measurement& first,
        const Measurement& second,
        Direction direction,
        double_t onThreshold,
        double_t offThreshold);

    /**
     * Bornes, pas et décimales des seuils dans le menu. Par défaut : -50 à
     * 250, pas de 0,5, une décimale. À appeler avant l'enregistrement des
     * paramètres.
     */
    void setRange(
        double_t minimum,
        double_t maximum,
        double_t step = 0.5,
        uint8_t decimals = 1);

    /** Libellés des seuils dans le menu (par défaut « Seuil marche »...). */
    void setLabels(
        const char* onLabel,
        const char* offLabel);

    /** Un seul seuil, sans hystérésis : le seuil d'arrêt suit celui de marche. */
    void useSingleThreshold();

    /**
     * Range le sous-menu du comparateur dans celui d'un autre propriétaire,
     * dont les paramètres doivent être enregistrés avant (voir
     * ParameterOwner::parentOwnerKey).
     */
    void setMenuParent(const char* ownerKey);

    /** Commande valide et en marche. */
    bool isOn() const;

    /** Valeur comparée du dernier cycle ; faux si une mesure est en défaut. */
    bool readValue(double_t& value) const;

    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    void registerParameters(ParameterList& list) override;

    bool validateParameters(const ParameterEditor& editor) const override;

private:
    void beginComparator(
        const char* key,
        const char* name,
        Direction direction,
        double_t onThreshold,
        double_t offThreshold);

    bool computeValue(double_t& value) const;

    /** Unité des seuils : celle de la mesure, K pour un écart de °C. */
    const char* thresholdUnit() const;

    const Measurement* first = nullptr;
    const Measurement* second = nullptr;

    Direction direction = Direction::Above;
    bool singleThreshold = false;

    double_t minimum = -50.0;
    double_t maximum = 250.0;
    double_t step = 0.5;
    uint8_t decimals = 1;

    const char* menuParent = nullptr;

    const char* onLabel = "Seuil marche";
    const char* offLabel = "Seuil arrêt";

    bool on = false;
    bool valueValid = false;
    double_t lastValue = 0.0;
};

#endif
