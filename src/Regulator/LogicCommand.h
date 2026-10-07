#ifndef LOGIC_COMMAND_H
#define LOGIC_COMMAND_H

#include <Regulator/Regulator.h>

class DigitalInput;
class Measurement;
class ProcessControl;

/**
 * Commande écrite par la glue de l'installation (processLogic()). On la relie
 * à un actionneur comme n'importe quel régulateur, et on l'enregistre avec
 * process.add(commande).
 *
 * - La glue doit l'écrire à chaque cycle (set(), setOn() ou invalidate()).
 *   Sinon, la sortie passe en état sûr et l'oubli est journalisé.
 * - dependsOn() : si une dépendance est en défaut, la sortie passe en état
 *   sûr, ou en maintien selon faultSettings, quoi que la glue ait écrit.
 * - Réglage « Commande » Auto / Marche / Arrêt, comme le thermostat : en
 *   manuel, ni la glue ni les dépendances n'agissent. disableManualMode() le
 *   retire, par exemple pour une résistance chauffante.
 */
class LogicCommand : public Regulator
{
public:
    /** Commande de la glue, ou sortie forcée par l'opérateur. */
    enum class Operation : uint8_t
    {
        Auto,
        ForcedOn,
        ForcedOff
    };

    struct Settings
    {
        /* Non sauvegardé : retour en Auto au démarrage. */
        Operation operation = Operation::Auto;
    };

    Settings settings;

    static constexpr uint8_t MAX_DEPENDENCIES = 4;

    void begin(const char* name);
    void begin(
        const char* key,
        const char* name);

    /**
     * Ajoute une dépendance (mesure, entrée TOR, ou bloc dont la commande doit
     * être valide). À appeler dans begin(). Faux si la liste est pleine.
     */
    bool dependsOn(const Measurement& measurement);
    bool dependsOn(const DigitalInput& input);
    bool dependsOn(const Regulator& block);

    /**
     * Retire le réglage « Commande » du menu : la sortie reste en Auto. À
     * appeler dans begin(), avant l'enregistrement des paramètres.
     */
    void disableManualMode();

    // ----- Écriture par la glue, à chaque cycle -----

    /** Commande de 0 à 1 (bornée). */
    void set(double_t value);

    /** Marche (1) ou arrêt (0). */
    void setOn(bool on);

    /** Commande invalide ce cycle : état sûr, sans journaliser d'oubli. */
    void invalidate();

    /** Sans effet : la commande est appliquée après la glue (applyLogic()). */
    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    /** Commande Auto (ni Marche ni Arrêt forcés). */
    bool isAutomatic() const override;

    /** Marche ou Arrêt forcés : prioritaires sur l'inhibition. */
    bool isManual() const override;

    void registerParameters(ParameterList& list) override;

private:
    friend class ProcessControl;

    /**
     * Appelé par ProcessControl après la glue : applique le mode manuel, les
     * dépendances, puis la commande écrite. Vrai au début d'un oubli
     * d'écriture, à journaliser une fois.
     */
    bool applyLogic(uint32_t now);

    MeasurementStatus dependencyStatus() const;

    struct Dependency
    {
        const Measurement* measurement = nullptr;
        const DigitalInput* input = nullptr;
        const Regulator* block = nullptr;
    };

    bool addDependency(const Dependency& dependency);

    Dependency dependencies[MAX_DEPENDENCIES];
    uint8_t dependencyCount = 0;

    bool manualModeAvailable = true;

    // Écriture de la glue pendant le cycle en cours.
    bool written = false;
    bool requestedValid = false;
    double_t requested = 0.0;

    // Oubli d'écriture en cours, déjà journalisé.
    bool missing = false;
};

#endif
