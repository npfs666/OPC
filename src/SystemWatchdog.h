#ifndef SYSTEM_WATCHDOG_H
#define SYSTEM_WATCHDOG_H

#include <Arduino.h>

#ifndef OPC_HOST_TEST
#include <pico/mutex.h>
#endif

/**
 * Surveille la progression des deux coeurs avec le watchdog materiel.
 *
 * Chaque coeur signale la fin de sa boucle. Le coeur de controle appelle
 * une methode qui ne nourrit le watchdog que si les deux coeurs ont
 * progresse depuis le dernier rearmement.
 */
class SystemWatchdog
{
public:
    // Modifier cette valeur pour regler le delai du watchdog general.
    static constexpr uint32_t TIMEOUT_MS = 8000;

    SystemWatchdog();

    void begin();

    void printLastResetDiagnostic(
        Stream& diagnosticOutput) const;

    void checkInControlCore();

    void checkInUiCore();

private:
    static constexpr uint32_t RP2040_MAX_TIMEOUT_MS =
        8388;

    static_assert(
        TIMEOUT_MS > 0 &&
            TIMEOUT_MS <= RP2040_MAX_TIMEOUT_MS,
        "SystemWatchdog::TIMEOUT_MS must be between 1 and 8388 ms");

    mutex_t heartbeatMutex;
    bool uiCoreCheckedIn = false;
    bool started = false;
    bool lastResetWasWatchdog = false;
};

#endif
