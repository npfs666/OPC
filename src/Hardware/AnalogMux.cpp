#include <Hardware/AnalogMux.h>

#include <Arduino.h>
#include <Hardware/pinout.h>

void AnalogMux::begin()
{
    Wire1.setSDA(Board::Rp2040::I2C_SDA);
    Wire1.setSCL(Board::Rp2040::I2C_SCL);

    if (!mcp.begin_I2C(Board::Mcp23017::ADDRESS, &Wire1)) {
        Serial.println("Error MCP init.");
    }

    // Au démarrage le MCP23017 à tous ses pins en entrées.
    mcp.pinMode(Board::Mcp23017::INPUT_1_EN, OUTPUT);
    mcp.pinMode(Board::Mcp23017::INPUT_2_EN, OUTPUT);
    mcp.pinMode(Board::Mcp23017::INPUT_3_EN, OUTPUT);

    mcp.pinMode(Board::Mcp23017::SW_2_WIRE, OUTPUT);
    mcp.pinMode(Board::Mcp23017::SW_2_3_WIRE, OUTPUT);
    mcp.pinMode(Board::Mcp23017::SW_3_WIRE, OUTPUT);
    mcp.pinMode(Board::Mcp23017::SW_4_WIRE, OUTPUT);

    mcp.pinMode(Board::Mcp23017::RREF1k65, OUTPUT);
    mcp.pinMode(Board::Mcp23017::RREF16k5, OUTPUT);


    mcp.pinMode(Board::Mcp23017::TC_GND, OUTPUT);
    mcp.pinMode(Board::Mcp23017::TC_VCC, OUTPUT);

    disableChannel(0);
    disableChannel(1);
    disableChannel(2);

    resetMeasurementMode();
    resetMeasurementType();

    enableChannel(1);
}

void AnalogMux::enableChannel(uint8_t channel)
{
    switch(channel)
    {
        case 0:
            mcp.digitalWrite(Board::Mcp23017::INPUT_1_EN, HIGH);
            break;

        case 1:
            mcp.digitalWrite(Board::Mcp23017::INPUT_2_EN, HIGH);
            break;

        case 2:
           mcp.digitalWrite(Board::Mcp23017::INPUT_3_EN, HIGH);
            break;
    }
}

void AnalogMux::disableChannel(uint8_t channel)
{
    switch(channel)
    {
        case 0:
            mcp.digitalWrite(Board::Mcp23017::INPUT_1_EN, LOW);
            break;

        case 1:
            mcp.digitalWrite(Board::Mcp23017::INPUT_2_EN, LOW);
            break;

        case 2:
            mcp.digitalWrite(Board::Mcp23017::INPUT_3_EN, LOW);
            break;
    }
}

void AnalogMux::resetMeasurementMode()
{
    mcp.digitalWrite(Board::Mcp23017::SW_2_WIRE, LOW);
    mcp.digitalWrite(Board::Mcp23017::SW_2_3_WIRE, LOW);
    mcp.digitalWrite(Board::Mcp23017::SW_3_WIRE, LOW);
    mcp.digitalWrite(Board::Mcp23017::SW_4_WIRE, LOW);
}

void AnalogMux::set2Wire() 
{
    resetMeasurementMode();
    mcp.digitalWrite(Board::Mcp23017::SW_2_WIRE, HIGH);
    mcp.digitalWrite(Board::Mcp23017::SW_2_3_WIRE, HIGH);
}

void AnalogMux::set3Wire()
{
    resetMeasurementMode();
    mcp.digitalWrite(Board::Mcp23017::SW_2_3_WIRE, HIGH);
    mcp.digitalWrite(Board::Mcp23017::SW_3_WIRE, HIGH);
}

void AnalogMux::set4Wire()
{
    resetMeasurementMode();
    mcp.digitalWrite(Board::Mcp23017::SW_4_WIRE, HIGH);
}



void AnalogMux::resetMeasurementType()
{
    mcp.digitalWrite(Board::Mcp23017::TC_GND, LOW);
    mcp.digitalWrite(Board::Mcp23017::TC_VCC, LOW);
    mcp.digitalWrite(Board::Mcp23017::RREF1k65, LOW);
    mcp.digitalWrite(Board::Mcp23017::RREF16k5, LOW);
}

void AnalogMux::setTC() 
{
    resetMeasurementType();
    mcp.digitalWrite(Board::Mcp23017::TC_GND, HIGH);
    mcp.digitalWrite(Board::Mcp23017::TC_VCC, HIGH);
}

void AnalogMux::setPT100()
{
    resetMeasurementType();
    mcp.digitalWrite(Board::Mcp23017::RREF1k65, HIGH);
}

void AnalogMux::setPT1000()
{
    resetMeasurementType();
    mcp.digitalWrite(Board::Mcp23017::RREF16k5, HIGH);
}