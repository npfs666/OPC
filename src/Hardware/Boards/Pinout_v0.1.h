#ifndef PINOUT_h
#define PINOUT_h

/**
 * Pinout for the v0.1 OPC PCB
 * Global settings
 */

// Divers
#define DC_DC_PWM 23

// Ecran SPI TFT (SPI1)
#define LCD_SCK 14
#define LCD_MOSI 15
#define LCD_DC 12
#define LCD_CS 13
#define LCD_RESET 11

// Encodeur rotatif
#define ROTENC_A 26
#define ROTENC_B 22
#define ROTENC_CLIC 21

// Analog switches
#define SW_3_WIRE 28
#define SW_4_WIRE 27
#define SW_MUX_1 2
#define SW_MUX_2 1
#define SW_MUX_3 0

// ADS1120 (SPI)
#define ADC_CLK  18
#define ADC_MISO 16
#define ADC_MOSI 19
#define ADC_CS   17
#define ADC_DRDY 20

// BME (I²C)
#define BME_SDA 8
#define BME_SCL 9

// Sorties
#define RELAIS_1 4
#define RELAIS_2 5


#endif
