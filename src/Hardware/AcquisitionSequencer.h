// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACQUISITIONSEQUENCER_H
#define ACQUISITIONSEQUENCER_H

#include <atomic>
#include <cstdint>
#include <Hardware/Sensor.h>

/**
 * ADC et multiplexeurs vus par le séquenceur (SensorBoard sur la carte, un
 * double dans les tests). Tout est appelé depuis loop(), ISR désarmée. Seule
 * restartConversions() est appelée interruptions coupées, juste avant
 * d'armer l'ISR.
 */
class AcquisitionHardware
{
public:
    /**
     * Montage et référence, configuration de l'ADC, puis connexion de la
     * voie. false : le routage n'est pas prouvé, l'entrée ne peut pas être
     * mesurée.
     */
    virtual bool routeInput(
        const Sensor::Settings& settings,
        uint8_t channel) = 0;

    // Fin d'acquisition : le routage a-t-il tenu ? false : mesure rejetée.
    virtual bool confirmInput() = 0;

    // Voie coupée, montage conservé jusqu'au routage suivant.
    virtual void disconnectInput(const Sensor::Settings& settings) = 0;

    // Fin des conversions continues (ADC en single-shot).
    virtual void stopConversions() = 0;

    // 3 fils : inversion des deux sources de courant (chopping).
    virtual void invertCurrents() = 0;

    // Gain de l'étendue large, même voie et même routage.
    virtual void selectExtendedGain(const Sensor::Settings& settings) = 0;

    // Température interne de l'ADC : une conversion unique.
    virtual void beginTemperature() = 0;
    virtual void endTemperature(int32_t raw) = 0;

    // Température interne non lue : jonction froide inconnue.
    virtual void failTemperature() = 0;

    // Réinitialisation de l'ADC après une étape sans conversion.
    virtual void resetConverter() = 0;

    virtual uint32_t milliseconds() = 0;

    virtual uint32_t disableInterrupts() = 0;
    virtual void restoreInterrupts(uint32_t state) = 0;

    // START/SYNC, puis acquittement du front DRDY en attente.
    virtual void restartConversions(bool continuous) = 0;

protected:
    ~AcquisitionHardware() = default;
};

/**
 * Séquencement de l'acquisition hors ISR.
 *
 * L'ISR DRDY lit la conversion et la passe à onConversion(). Celle-ci ignore
 * les conversions d'établissement, accumule l'échantillon et désarme l'ISR à
 * mi-série (3 fils), en fin de série ou après une conversion unique.
 * poll(), dans loop(), enchaîne alors l'étape suivante : inversion des
 * courants, diagnostic d'étendue, voie suivante, température de l'ADC, cycle
 * suivant.
 *
 * Partage du SPI : l'ISR ne lit que si elle est armée, et poll() ne touche au
 * matériel que si elle est désarmée. L'armement vient en dernier,
 * interruptions coupées, juste après la relance des conversions.
 *
 * Défauts : une entrée dont le routage n'est pas prouvé, au départ ou en fin
 * d'acquisition, est rejetée (Invalid) pour ce cycle. Une étape sans
 * conversion dans le délai prévu (ADC muet) réinitialise l'ADC et rejette
 * l'entrée. Le cycle continue dans tous les cas, et une entrée en défaut
 * garde sa durée normale : un MCP muet ne fait pas tourner le cycle à vide.
 */
class AcquisitionSequencer
{
public:
    // Défauts vus pendant un cycle.
    struct CycleReport
    {
        bool routingFault = false;     // routage non prouvé sur une entrée
        bool converterTimeout = false; // étape sans conversion : ADC muet
    };

    explicit AcquisitionSequencer(AcquisitionHardware& hardware);

    // loop() : démarre le cycle sur la première entrée. Sans entrée, rien.
    void start(Sensor* const* sensors, uint8_t count);

    // loop() : désarme l'ISR. Le matériel reste à la charge de l'appelant.
    void stop();

    // loop() : étape suivante, une fois l'ISR désarmée ou le délai écoulé.
    void poll();

    // loop() : vrai une fois par cycle complet (entrées et température ADC).
    bool takeCycleCompleted();

    // Défauts du dernier cycle complet.
    const CycleReport& lastCycle() const { return lastReport; }

    // Étapes sans conversion depuis le démarrage.
    uint32_t converterTimeouts() const { return timeouts; }

    bool isRunning() const { return step != Step::Stopped; }

    // ISR : une conversion est attendue.
    bool isArmed() const
    {
        return armed.load(std::memory_order_acquire);
    }

    // ISR : conversion lue sur l'ADC.
    void onConversion(int32_t value);

private:
    enum class Step : uint8_t
    {
        Stopped,
        Accumulating,
        Diagnosing,
        Temperature,
        Waiting // entrée en défaut : sa durée normale s'écoule
    };

    enum class Event : uint8_t
    {
        None,
        HalfWay,
        SeriesDone,
        SingleDone
    };

    Sensor& current() const { return *sensors[channel]; }

    void handle(Event reached);
    void handleTimeout();
    void routeCurrent();
    void finishInput();
    void rejectInput();
    void nextInput();
    void completeCycle();
    void arm(Step nextStep, uint16_t conversionsToDiscard, bool continuous);
    void disarm(Event reached);
    void startTimer(uint32_t durationMs);

    // Conversions à ignorer après un changement de configuration.
    static uint16_t settlingConversions(const Sensor::Settings& settings);

    // Durée normale de l'acquisition d'une entrée.
    static uint32_t inputDurationMs(const Sensor::Settings& settings);

    AcquisitionHardware& hardware;
    Sensor* const* sensors = nullptr;
    uint8_t count = 0;
    uint8_t channel = 0;
    bool cycleCompleted = false;
    CycleReport report;
    CycleReport lastReport;
    uint32_t timeouts = 0;

    // Délai de l'étape en cours.
    uint32_t stepStartedAt = 0;
    uint32_t stepDurationMs = 0;

    // Écrits avant l'armement, lus par l'ISR.
    Step step = Step::Stopped;
    uint16_t discard = 0;

    // Écrit par l'ISR avant le désarmement, lu par poll().
    int32_t singleValue = 0;

    std::atomic<bool> armed{false};
    std::atomic<Event> event{Event::None};
};

#endif
