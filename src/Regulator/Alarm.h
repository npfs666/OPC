#ifndef ALARM_H
#define ALARM_H

#include <Regulator/Regulator.h>

class DigitalInput;

/**
 * Base des alarmes : un régulateur dont la commande vaut 1 quand l'alarme est
 * signalée. Elle peut piloter un relais par un ActuatorOnOff, ou n'être
 * qu'affichée (bandeau d'accueil, journal série). L'enregistrer avec
 * process.add(alarme), après le régulateur qu'elle surveille.
 *
 * Une classe dérivée calcule sa condition confirmée et la passe à
 * applyCondition(), qui gère la mémorisation et l'acquittement.
 */
class Alarm : public Regulator
{
public:
    /** Alarme activée dans ses réglages. */
    virtual bool isEnabled() const = 0;

    /*
     * L'inhibition d'une alarme suit d'autres règles (alarme déclarée
     * inhibable, défaut de sonde jamais masqué, mémorisation conservée) :
     * étape 1c du plan, doc/Plan_logique_installation.md.
     */
    void inhibit(bool inhibited) = delete;

    /**
     * Vrai si l'alarme doit mettre en sécurité les sorties du régulateur
     * relié (voir Regulator::setInterlock()).
     */
    virtual bool locksOutputs() const
    {
        return false;
    }

    /** Entrée numérique d'acquittement (front montant). Optionnel. */
    void setAcknowledgeInput(const DigitalInput& input);

    /** Alarme signalée, en cours ou mémorisée. */
    bool isActive() const;

    /** Mémorisée : la cause a disparu, l'acquittement est attendu. */
    bool isLatched() const;

    /**
     * Efface la mémorisation. Une alarme dont la cause est toujours
     * présente reste signalée, mais ne sera plus mémorisée à sa fin.
     */
    void acknowledge();

    /** L'état de l'alarme survit à une reprise (validation du menu). */
    void resume(uint32_t now) override;

    void print(Stream& stream) const override;

protected:
    /** Remise à zéro à l'activation de l'alarme. */
    void beginAlarm();

    /** Front montant de l'entrée d'acquittement, à appeler à chaque mise à jour. */
    void pollAcknowledgeInput();

    /**
     * Condition confirmée (temporisation comprise) : met à jour l'état,
     * la mémorisation et la commande.
     */
    void applyCondition(bool confirmed, bool latching);

    /** Alarme désactivée : état effacé, commande à 0. */
    void clearAlarm();

private:
    const DigitalInput* acknowledgeInput = nullptr;
    bool acknowledgeInputWasActive = false;

    bool active = false;        // condition confirmée
    bool latched = false;
    bool acknowledged = false;  // acquittée pendant l'épisode en cours
};

#endif
