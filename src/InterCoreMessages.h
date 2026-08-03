#ifndef INTER_CORE_MESSAGES_H
#define INTER_CORE_MESSAGES_H

#include <cstdint>

enum class InterCoreMessage : uint32_t
{
    PauseAcquisition = 1,
    ResumeAcquisition = 2,
    PrintDataAvailable = 3,
    ApplyMenuParameters = 4,
    MenuParametersApplied = 5,
    ParametersReady = 6,
    AcquisitionPaused = 7,
    MenuParametersRejected = 8,
    MenuParametersAppliedNotSaved = 9,
    ControlCoreReady = 10,
    UiCoreReady = 11
};

constexpr uint32_t interCoreMessageValue(
    InterCoreMessage message)
{
    return static_cast<uint32_t>(message);
}

#endif
