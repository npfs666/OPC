#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H

#include <Arduino.h>

class RotaryEncoder
{
public:
    void begin();

    void onRotationISR();
    void onButtonISR();

    int32_t takeRotation();
    bool takeClick();

private:
    static constexpr uint32_t BUTTON_DEBOUNCE_US = 50000;

    volatile int32_t rotation = 0;
    volatile bool clicked = false;
    volatile uint32_t lastButtonTime = 0;
};

#endif
