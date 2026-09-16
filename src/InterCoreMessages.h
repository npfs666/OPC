#ifndef INTER_CORE_MESSAGES_H
#define INTER_CORE_MESSAGES_H

#include <cstdint>

enum class InterCoreMessage : uint32_t
{
    CaptureMenuParameters = 1,
    ResumeAcquisition = 2,
    PrintDataAvailable = 3,
    ApplyMenuParameters = 4,
    MenuParametersApplied = 5,
    ParametersReady = 6,
    MenuParametersCaptured = 7,
    MenuParametersRejected = 8,
    MenuParametersAppliedNotSaved = 9,
    ControlCoreReady = 10,
    UiCoreReady = 11,
    CaptureClockParameters = 12,
    ClockParametersCaptured = 13,
    ApplyClockParameters = 14,
    ClockParametersApplied = 15,
    ClockParametersRejected = 16
};

constexpr uint32_t interCoreMessageValue(
    InterCoreMessage message)
{
    return static_cast<uint32_t>(message);
}

#endif
