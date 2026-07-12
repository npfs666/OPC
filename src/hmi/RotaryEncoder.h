#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H

#include <Arduino.h>

class RotaryEncoder
{
public:

    void begin();

private:

    static void isrButton();

    static void isrRotation();
};

#endif