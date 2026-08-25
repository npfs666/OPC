#ifndef PINOUT_h
#define PINOUT_h

#include <Arduino.h>
#include <Hardware/Mcp23017Pin.h>

/**
 * Pinout for the v0.2 OPC PCB
 * Global settings
 */

namespace Board
{
    namespace Rp2040
    {   
        // Divers
        inline constexpr uint8_t DC_DC_PWM  = 23;

        // Ecran (SPI1)
        inline constexpr uint8_t LCD_SCK    = 14;
        inline constexpr uint8_t LCD_MOSI   = 15;
        inline constexpr uint8_t LCD_DC     = 12;
        inline constexpr uint8_t LCD_CS     = 13;

        // ADS1120 (SPI0)
        inline constexpr uint8_t ADC_CLK    = 18;
        inline constexpr uint8_t ADC_MISO   = 16;
        inline constexpr uint8_t ADC_MOSI   = 19;
        inline constexpr uint8_t ADC_CS     = 17;
        inline constexpr uint8_t ADC_DRDY   = 22;

        // I²C : Commun à BME, DS3231, MCP23017
        inline constexpr uint8_t I2C_SDA = 6;
        inline constexpr uint8_t I2C_SCL = 7;

        // Encodeur rotatif
        inline constexpr uint8_t ROTENC_A   = 11;
        inline constexpr uint8_t ROTENC_B   = 10;
        inline constexpr uint8_t ROTENC_CLIC= 34;

        // Sorties
        inline constexpr uint8_t OUTPUT_1 = 21;
        inline constexpr uint8_t OUTPUT_2 = 20;
        inline constexpr uint8_t OUTPUT_3 = 31;
        inline constexpr uint8_t OUTPUT_4 = 32;
        inline constexpr uint8_t OUTPUT_5 = 8;
        inline constexpr uint8_t OUTPUT_6 = 9;
    }

    namespace Mcp0
    {
        inline constexpr uint8_t ADDRESS = 0x20;

        inline constexpr Mcp23017Pin SW_2_3_WIRE = Mcp23017Pin::GPA0;
        inline constexpr Mcp23017Pin SW_4WIRE   = Mcp23017Pin::GPA1;
        inline constexpr Mcp23017Pin RREF16k5   = Mcp23017Pin::GPA2;
        inline constexpr Mcp23017Pin TC_VCC     = Mcp23017Pin::GPA3;
        inline constexpr Mcp23017Pin SW_2_WIRE  = Mcp23017Pin::GPA4;
        inline constexpr Mcp23017Pin SW_3_WIRE  = Mcp23017Pin::GPA5;
        inline constexpr Mcp23017Pin TC_GND     = Mcp23017Pin::GPA6;
        inline constexpr Mcp23017Pin RREF1k65   = Mcp23017Pin::GPA7;

        inline constexpr Mcp23017Pin INPUT_1_EN = Mcp23017Pin::GPB0;
        inline constexpr Mcp23017Pin INPUT_2_EN = Mcp23017Pin::GPB1;
        inline constexpr Mcp23017Pin INPUT_3_EN = Mcp23017Pin::GPB2;
    }

    namespace BME
    {
        inline constexpr uint8_t ADDRESS = 0x46;
    }

    namespace DS3231
    {
        inline constexpr uint8_t ADDRESS = 0x68;
    }
}


#endif
