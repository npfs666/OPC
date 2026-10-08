// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THREEPOINTACTUATOR_H
#define THREEPOINTACTUATOR_H

#include <Outputs/Actuator.h>

/**
 * Vanne motorisée 3 points (servomoteur ouvrir / fermer, sans recopie de
 * position).
 *
 * La commande du régulateur (0 à 1) est la position voulue. La position est
 * estimée à partir de l'état réellement appliqué des sorties et du temps de
 * course, puis recalée en butée : une commande à 0 ou 1 pousse la vanne en
 * butée, puis pendant la sur-course. Au démarrage, la position est inconnue :
 * la vanne fait une course complète vers la butée la plus proche de la
 * commande.
 *
 * Deux câblages :
 * - OpenClose : une sortie par sens (Ouvrir, Fermer). La sortie Ouvrir est
 *   verrouillée à l'arrêt en sécurité ; l'état sûr de Fermer choisit le repli
 *   (ON = fermeture, OFF = vanne figée).
 * - RunDirection : Marche et Sens, Sens câblé en inverseur (NO ouvrir,
 *   NC fermer). Les deux sens ne peuvent jamais être alimentés ensemble.
 *   Sens est verrouillé à OFF (fermer) ; l'état sûr de Marche choisit le
 *   repli. Sens ne bascule que Marche coupée.
 *
 * Les sorties sont reliées par process.connect(), après leur begin().
 * Voir src/Outputs/README.md.
 */
class ThreePointActuator : public Actuator
{
public:

    enum class Wiring : uint8_t
    {
        OpenClose,
        RunDirection
    };

    struct Settings
    {
        // Durée d'une course complète, fermée → ouverte, en s.
        uint32_t travelTime = 120;

        // Écart de position, en %, en dessous duquel la vanne ne bouge pas.
        double_t deadband = 2.0;

        // Arrêt minimal avant de repartir dans l'autre sens, en ms.
        uint32_t reversalPause = 500;

        // Marche prolongée en butée pour le recalage, en % de la course.
        uint32_t overtravel = 20;
    };

    Settings settings;

    ThreePointActuator();

    void begin(
        const char* name,
        Regulator& regulator,
        Output& open,
        Output& close,
        uint32_t travelTime = 120);

    void begin(
        const char* key,
        const char* name,
        Regulator& regulator,
        Output& open,
        Output& close,
        uint32_t travelTime = 120);

    void beginRunDirection(
        const char* name,
        Regulator& regulator,
        Output& run,
        Output& direction,
        uint32_t travelTime = 120);

    void beginRunDirection(
        const char* key,
        const char* name,
        Regulator& regulator,
        Output& run,
        Output& direction,
        uint32_t travelTime = 120);

    void update(uint32_t now) override;

    /** Compte la durée du repli avec l'état sûr appliqué aux sorties. */
    void resume(uint32_t now) override;

    void registerParameters(
        ParameterList& list) override;

    /**
     * Commande de sécurité à 0 ou 1 sur la sortie qui choisit le repli :
     * une sortie PWM ne doit pas hacher le moteur.
     */
    bool validateParameters(
        const ParameterEditor& editor) const override;

    /** Position estimée, de 0 (fermée) à 1 (ouverte). */
    double_t position() const;

    /** Faux jusqu'à la fin de la course de recalage du démarrage. */
    bool isPositionKnown() const;

    Wiring wiring() const;

protected:

    double_t printValue() const override;

    bool acceptOutput(Output& output) override;

private:

    enum class Motion : uint8_t
    {
        Stopped,
        Opening,
        Closing
    };

    void setup(
        const char* key,
        const char* name,
        Regulator& regulator,
        Wiring wiring,
        Output& first,
        Output& second,
        uint32_t travelTime);

    static bool isOn(const Output* output);

    Motion appliedMotion() const;

    // Intègre la position depuis le dernier appel, d'après les sorties.
    void track(uint32_t now);

    Motion wantedMotion(double_t target);

    void drive(Motion wanted, uint32_t now);

    void forceSafeOutputs();

    // Sortie verrouillée à 0 en sécurité (Ouvrir ou Sens), et celle dont
    // l'état sûr choisit le repli (Fermer ou Marche).
    Output* lockedOutput() const;
    Output* fallbackOutput() const;

    double_t travelMs() const;
    double_t overtravelMs() const;

    Wiring wiringMode = Wiring::OpenClose;

    // Ouvrir et Fermer, ou Marche et Sens.
    Output* openOrRun = nullptr;
    Output* closeOrDirection = nullptr;

    double_t estimate = 0.0;

    // Marche poussée contre la butée où se trouve l'estimation, en ms.
    double_t beyondMs = 0.0;

    // Marche continue dans le même sens, en ms (recalage du démarrage).
    double_t continuousMs = 0.0;

    bool known = false;

    // Sens du recalage du démarrage, choisi à la première commande valide.
    Motion calibration = Motion::Stopped;

    // Déplacement voulu au tour précédent (hystérésis de la zone morte).
    Motion movement = Motion::Stopped;

    Motion previousApplied = Motion::Stopped;
    Motion lastMotion = Motion::Stopped;
    uint32_t stoppedAt = 0;

    bool previousDirection = false;
    uint32_t directionChangedAt = 0;

    bool tracking = false;
    uint32_t lastTrack = 0;
};

#endif
