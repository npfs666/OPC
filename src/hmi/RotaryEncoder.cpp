#include <hmi/RotaryEncoder.h>

#include <Arduino.h>
#include <Hardware/pinout.h>

namespace
{
    /*
     * Index : ancien état AB sur les bits 3..2,
     *         nouvel état AB sur les bits 1..0.
     *
     * Les transitions impossibles et les états identiques valent 0.
     * Les rebonds produisent des transitions opposées qui s'annulent.
     */
    constexpr int8_t QUADRATURE_TRANSITIONS[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0
    };
}

void RotaryEncoder::begin()
{
    pinMode(ROTENC_A, INPUT_PULLUP);
    pinMode(ROTENC_B, INPUT_PULLUP);
    pinMode(ROTENC_CLIC, INPUT_PULLUP);

    previousState = readState();
    transitionAccumulator = 0;
    rotation = 0;
    clicked = false;
}

void RotaryEncoder::onButtonISR()
{
    const uint32_t now = millis();

    if ((now - lastButtonTime) < BUTTON_DEBOUNCE_MS)
        return;

    lastButtonTime = now;
    clicked = true;
}

void RotaryEncoder::onRotationISR()
{
    const uint8_t currentState = readState();
    const uint8_t oldState = previousState;

    if (currentState == oldState)
        return;

    previousState = currentState;

    const uint8_t transition =
        static_cast<uint8_t>(
            (oldState << 2) | currentState);

    const int8_t direction =
        QUADRATURE_TRANSITIONS[transition];

    if (direction == 0)
    {
        transitionAccumulator = 0;
        return;
    }

    transitionAccumulator += direction;

    if (transitionAccumulator >= TRANSITIONS_PER_STEP)
    {
        rotation++;
        transitionAccumulator -= TRANSITIONS_PER_STEP;
    }
    else if (transitionAccumulator <= -TRANSITIONS_PER_STEP)
    {
        rotation--;
        transitionAccumulator += TRANSITIONS_PER_STEP;
    }
}

int32_t RotaryEncoder::takeRotation()
{
    noInterrupts();

    const int32_t result = rotation;
    rotation = 0;

    interrupts();

    return result;
}

uint8_t RotaryEncoder::readState() const
{
    const uint8_t stateA =
        digitalRead(ROTENC_A) ? 1 : 0;

    const uint8_t stateB =
        digitalRead(ROTENC_B) ? 1 : 0;

    return static_cast<uint8_t>(
        (stateA << 1) | stateB);
}

bool RotaryEncoder::takeClick()
{
    noInterrupts();

    const bool result = clicked;
    clicked = false;

    interrupts();

    return result;
}
