#include "SystemWatchdog.h"

SystemWatchdog::SystemWatchdog()
{
    mutex_init(&heartbeatMutex);
}



void SystemWatchdog::begin()
{
    lastResetWasWatchdog =
        rp2040.getResetReason() ==
            RP2040::WDT_RESET;

    mutex_enter_blocking(&heartbeatMutex);
    uiCoreCheckedIn = false;
    started = true;
    mutex_exit(&heartbeatMutex);

    rp2040.wdt_begin(TIMEOUT_MS);
}



void SystemWatchdog::printLastResetDiagnostic(
    Stream& diagnosticOutput) const
{
    if (lastResetWasWatchdog)
        diagnosticOutput.println(
            "Previous reset caused by system watchdog");
}



void SystemWatchdog::checkInControlCore()
{
    bool shouldFeed = false;

    mutex_enter_blocking(&heartbeatMutex);

    if (started &&
        uiCoreCheckedIn)
    {
        uiCoreCheckedIn = false;
        shouldFeed = true;
    }

    mutex_exit(&heartbeatMutex);

    if (shouldFeed)
        rp2040.wdt_reset();
}



void SystemWatchdog::checkInUiCore()
{
    mutex_enter_blocking(&heartbeatMutex);

    if (started)
        uiCoreCheckedIn = true;

    mutex_exit(&heartbeatMutex);
}
