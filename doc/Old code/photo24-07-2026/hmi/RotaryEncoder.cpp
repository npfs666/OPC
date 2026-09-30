#include<hmi/RotaryEncoder.h>
#include<Hardware/pinout.h>

void RotaryEncoder::begin()
{
    attachInterrupt(
        digitalPinToInterrupt(ROTENC_A),
        isrRotation,
        FALLING);

    attachInterrupt(
        digitalPinToInterrupt(ROTENC_CLIC),
        isrButton,
        FALLING);
}

void RotaryEncoder::isrButton()
{
    //nav.doNav(navCmds::enterCmd);
}

void RotaryEncoder::isrRotation()
{
    /*if(digitalRead(ROTENC_B))
        nav.doNav(navCmds::upCmd);
    else
        nav.doNav(navCmds::downCmd);*/
}