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

    /**
     * Déclare l'alarme inhibable par la glue. À appeler dans begin() : sans
     * cela, inhibit() est sans effet, pour qu'une glue ne puisse pas faire
     * taire n'importe quelle alarme (une alarme de verrouillage, par exemple).
     */
    void allowInhibit();

    bool isInhibitable() const;

    /**
     * Glue : inhibe une alarme déclarée inhibable (dégivrage...). Non
     * sauvegardé, conservé jusqu'au prochain appel. Une alarme inhibée ne peut
     * pas se déclencher sur sa condition, et son retard repart de zéro à la
     * levée. Un défaut de sonde n'est jamais masqué, et une alarme mémorisée
     * le reste jusqu'à l'acquittement (sa commande n'est pas forcée à 0).
     * Une classe dérivée lit isInhibited() pour écarter sa condition.
     */
    void inhibit(bool inhibited) override;

    bool isInhibited() const override;

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

    bool inhibitable = false;
    bool alarmInhibited = false;

    bool active = false;        // condition confirmée
    bool latched = false;
    bool acknowledged = false;  // acquittée pendant l'épisode en cours
};

#endif
