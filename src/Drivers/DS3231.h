#ifndef DS3231_H
#define DS3231_H

#include <Arduino.h>
#include <Wire.h>

class DS3231
{
public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x68;
    static constexpr uint8_t REGISTER_COUNT = 0x13;

    enum class Register : uint8_t
    {
        Seconds = 0x00,
        Minutes = 0x01,
        Hours = 0x02,
        DayOfWeek = 0x03,
        Date = 0x04,
        Month = 0x05,
        Year = 0x06,
        Alarm1Seconds = 0x07,
        Alarm1Minutes = 0x08,
        Alarm1Hours = 0x09,
        Alarm1DayDate = 0x0A,
        Alarm2Minutes = 0x0B,
        Alarm2Hours = 0x0C,
        Alarm2DayDate = 0x0D,
        Control = 0x0E,
        Status = 0x0F,
        AgingOffset = 0x10,
        TemperatureMsb = 0x11,
        TemperatureLsb = 0x12
    };

    /**
     * Attache le driver à un bus I2C déjà initialisé et vérifie le composant.
     */
    bool begin(
        TwoWire& wire,
        uint8_t address = DEFAULT_ADDRESS);

    bool isConnected() const;

    bool readRegister(
        Register address,
        uint8_t& value) const;

    bool readRegister(
        uint8_t address,
        uint8_t& value) const;

    bool writeRegister(
        Register address,
        uint8_t value);

    bool writeRegister(
        uint8_t address,
        uint8_t value);

    bool readRegisters(
        Register startAddress,
        uint8_t* values,
        size_t length) const;

    bool readRegisters(
        uint8_t startAddress,
        uint8_t* values,
        size_t length) const;

    bool writeRegisters(
        Register startAddress,
        const uint8_t* values,
        size_t length);

    bool writeRegisters(
        uint8_t startAddress,
        const uint8_t* values,
        size_t length);

    /**
     * Modifie les bits ordinaires d'un registre.
     * Le registre Status (flags W0C) est volontairement refusé.
     */
    bool updateRegister(
        Register address,
        uint8_t mask,
        uint8_t value);

    bool updateRegister(
        uint8_t address,
        uint8_t mask,
        uint8_t value);

private:
    bool isValidRange(
        uint8_t startAddress,
        size_t length) const;

    TwoWire* bus = nullptr;
    uint8_t deviceAddress = DEFAULT_ADDRESS;
};

#endif
