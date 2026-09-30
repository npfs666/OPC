#include <Hardware/AnalogMux.h>

void AnalogMux::begin()
{
    pinMode(SW_3_WIRE, OUTPUT);
    pinMode(SW_4_WIRE, OUTPUT);

    pinMode(SW_MUX_1, OUTPUT);
    pinMode(SW_MUX_2, OUTPUT);
    pinMode(SW_MUX_3, OUTPUT);

    disableChannel(0);
    disableChannel(1);
    disableChannel(2);
}

void AnalogMux::enableChannel(uint8_t channel)
{
    switch(channel)
    {
        case 0:
            digitalWrite(SW_MUX_1, HIGH);
            break;

        case 1:
            digitalWrite(SW_MUX_2, HIGH);
            break;

        case 2:
            digitalWrite(SW_MUX_3, HIGH);
            break;
    }
}

void AnalogMux::disableChannel(uint8_t channel)
{
    switch(channel)
    {
        case 0:
            digitalWrite(SW_MUX_1, LOW);
            break;

        case 1:
            digitalWrite(SW_MUX_2, LOW);
            break;

        case 2:
            digitalWrite(SW_MUX_3, LOW);
            break;
    }
}

void AnalogMux::set3Wire()
{
    digitalWrite(SW_3_WIRE, HIGH);
    digitalWrite(SW_4_WIRE, LOW);
}

void AnalogMux::set4Wire()
{
    digitalWrite(SW_3_WIRE, LOW);
    digitalWrite(SW_4_WIRE, HIGH);
}