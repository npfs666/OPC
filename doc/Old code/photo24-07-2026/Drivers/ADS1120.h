#ifndef ADS1120_h
#define ADS1120_h

#include "Arduino.h"
#include <SPI.h>

#define SPI_MASTER_DUMMY   0xFF
// Commands for the ADC
#define CMD_RESET 0x07
#define CMD_START_SYNC 0x08
#define CMD_PWRDWN 0x03
#define CMD_RDATA 0x1f
#define CMD_RREG 0x20
#define CMD_WREG 0x40

// Configuration registers
#define CONFIG_REG0_ADDRESS 0x00
#define CONFIG_REG1_ADDRESS 0x01
#define CONFIG_REG2_ADDRESS 0x02
#define CONFIG_REG3_ADDRESS 0x03

// Register masks for setings
// Register 0
#define REG_MASK_MUX 0xF0
#define REG_MASK_GAIN 0x0E
#define REG_MASK_PGA_BYPASS 0x01

// Register 1
#define REG_MASK_DATARATE 0xE0
#define REG_MASK_OP_MODE 0x18
#define REG_MASK_CONV_MODE 0x04
#define REG_MASK_TEMP_MODE 0x02
#define REG_MASK_BURNOUT_SOURCES 0x01

// Register 2
#define REG_MASK_VOLTAGE_REF 0xC0
#define REG_MASK_FIR_CONF 0x30
#define REG_MASK_PWR_SWITCH 0x08
#define REG_MASK_IDAC_CURRENT 0x07

// Register 3
#define REG_MASK_IDAC1_ROUTING 0xE0
#define REG_MASK_IDAC2_ROUTING 0x1C
#define REG_MASK_DRDY_MODE 0x02
#define REG_MASK_RESERVED 0x01

#define DATARATE_20_SPS   0x00
#define DATARATE_45_SPS   0x01
#define DATARATE_90_SPS   0x02
#define DATARATE_175_SPS  0x03
#define DATARATE_330_SPS  0x04
#define DATARATE_600_SPS  0x05
#define DATARATE_1000_SPS 0x05

#define CONVERSION_SINGLE_SHOT 0x00
#define CONVERSION_CONTINUOUS  0x01

#define CURRENT_0_UA    0x00
#define CURRENT_50_UA   0x02
#define CURRENT_100_UA  0x03
#define CURRENT_250_UA  0x04
#define CURRENT_500_UA  0x05
#define CURRENT_1000_UA 0x06
#define CURRENT_1500_UA 0x07

#define IDAC_DISABLED   0x00
#define IDAC_AIN0_REFP1 0x01
#define IDAC_AIN1       0x02
#define IDAC_AIN2       0x03
#define IDAC_AIN3_REFN1 0x04
#define IDAC_REFP0      0x05
#define IDAC_REFN0      0x06

#define MUX_AINP_AIN0_AINN_AIN1 0x00
#define MUX_AINP_AIN0_AINN_AIN2 0x01
#define MUX_AINP_AIN0_AINN_AIN3 0x02
#define MUX_AINP_AIN1_AINN_AIN2 0x03
#define MUX_AINP_AIN1_AINN_AIN3 0x04
#define MUX_AINP_AIN2_AINN_AIN3 0x05
#define MUX_AINP_AIN1_AINN_AIN0 0x06
#define MUX_AINP_AIN3_AINN_AIN2 0x07
#define MUX_AINP_AIN0_AINN_AVSS 0x08
#define MUX_AINP_AIN1_AINN_AVSS 0x09
#define MUX_AINP_AIN2_AINN_AVSS 0x0A
#define MUX_AINP_AIN3_AINN_AVSS 0x0B
#define MUX_REFP_REFN_4         0x0C
#define MUX_VDD_VSS_4           0x0D
#define MUX_AINP_AINN_SHORTED   0x0E

#define VREF_INTERNAL_2048_MV     0x00
#define VREF_EXTERNAL_REFP0_REFN0 0x01
#define VREF_EXTERNAL_AIN0_AIN3   0x02
#define VREF_ANALOG_AVDD_AVSS     0x03

#define FIR_NONE          0x00
#define FIR_50HZ_AND_60HZ 0x01
#define FIR_50HZ          0x02
#define FIR_60HZ          0x03

#define TEMP_OFF 0x00
#define TEMP_ON  0x01




class ADS1120 {
  public:
    ADS1120();
    void writeRegister(uint8_t address, uint8_t value);
    uint8_t readRegister(uint8_t address);
    void begin(SPIClassRP2040 *spiClass, uint8_t clk_pin, uint8_t miso_pin, uint8_t mosi_pin, uint8_t cs_pin, uint8_t drdy_pin);
    bool isDataReady(void);
    int32_t readADC(void);
    byte * readADC_Array(void);
    int32_t readADC_Single(void);
    byte * readADC_SingleArray(void);
    double readInternalTemp(void);
    void sendCommand(uint8_t command);
    void reset(void);
    void startSync(void);
    void powerDown(void);
    void rdata(void);
    void writeRegisterMasked(uint8_t value, uint8_t mask, uint8_t address);
    void setMultiplexer(uint8_t value);
    void setGain(uint8_t gain);
    void setPGAbypass(bool value);
    void setDataRate(uint8_t value);
    void setOpMode(uint8_t value);
    void setConversionMode(uint8_t value);
    void setTemperatureMode(uint8_t value);
    void setBurnoutCurrentSources(bool value);
    void setVoltageRef(uint8_t value);
    void setFIR(uint8_t value);
    void setPowerSwitch(uint8_t value);
    void setIDACcurrent(uint8_t value);
    void setIDAC1routing(uint8_t value);
    void setIDAC2routing(uint8_t value);
    void setDRDYmode(uint8_t value);

  private:
    uint8_t cs;
    uint8_t drdy;
    uint8_t clk;
    uint8_t miso;
    uint8_t mosi;
    SPIClassRP2040 *HW_SPI;
  };
#endif

