#include <hmi/RotaryEncoder.h>

#include <Hardware/pinout.h>

void RotaryEncoder::begin()
{
    pinMode(ROTENC_A, INPUT_PULLUP);
    pinMode(ROTENC_B, INPUT_PULLUP);
    pinMode(ROTENC_CLIC, INPUT_PULLUP);
}

void RotaryEncoder::onButtonISR()
{
    const uint32_t now = micros();

    if ((now - lastButtonTime) < BUTTON_DEBOUNCE_US)
        return;

    lastButtonTime = now;
    clicked = true;
}

void RotaryEncoder::onRotationISR()
{
    if (digitalRead(ROTENC_B))
        rotation--;
    else
        rotation++;
}

int32_t RotaryEncoder::takeRotation()
{
    noInterrupts();

    const int32_t result = rotation;
    rotation = 0;

    interrupts();

    return result;
}

bool RotaryEncoder::takeClick()
{
    noInterrupts();

    const bool result = clicked;
    clicked = false;

    interrupts();

    return result;
}
