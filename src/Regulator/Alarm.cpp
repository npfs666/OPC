#include <Regulator/Alarm.h>

#include <Inputs/DigitalInput.h>

#include <cstring>

void Alarm::setAcknowledgeInput(const DigitalInput& input)
{
    acknowledgeInput = &input;
}

bool Alarm::isActive() const
{
    return active || latched;
}

bool Alarm::isLatched() const
{
    return latched && !active;
}

void Alarm::acknowledge()
{
    latched = false;
    acknowledged = active;
}

void Alarm::resume(uint32_t now)
{
    (void)now;
}

void Alarm::beginAlarm()
{
    acknowledgeInput = nullptr;
    acknowledgeInputWasActive = false;
    active = false;
    latched = false;
    acknowledged = false;
}

void Alarm::pollAcknowledgeInput()
{
    if (acknowledgeInput == nullptr)
        return;

    const bool inputActive = acknowledgeInput->isActive();

    if (inputActive && !acknowledgeInputWasActive)
        acknowledge();

    acknowledgeInputWasActive = inputActive;
}

void Alarm::applyCondition(bool confirmed, bool latching)
{
    active = confirmed;

    if (!active)
        acknowledged = false;

    if (active && latching && !acknowledged)
        latched = true;

    if (!latching)
        latched = false;

    writeCommand(isActive() ? 1.0 : 0.0);
}

void Alarm::clearAlarm()
{
    active = false;
    latched = false;
    acknowledged = false;
    writeCommand(0.0);
}

void Alarm::print(Stream& stream) const
{
    stream.print(getName());

    size_t length = std::strlen(getName());
    while (length++ < 16)
        stream.print(' ');

    stream.print(": ");

    if (!isEnabled())
        stream.println("Inactive");
    else if (isLatched())
        stream.println("ALARME memorisee");
    else if (isActive())
        stream.println("ALARME");
    else
        stream.println("OK");
}
