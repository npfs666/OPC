#ifndef TEST_FAKE_WIRE_H
#define TEST_FAKE_WIRE_H

#include <array>
#include <cstddef>
#include <cstdint>

class TwoWire
{
public:
    static constexpr size_t REGISTER_COUNT = 0x13;

    bool setSDA(uint8_t pin)
    {
        sdaPin = pin;
        return pinsAccepted;
    }

    bool setSCL(uint8_t pin)
    {
        sclPin = pin;
        return pinsAccepted;
    }

    void begin()
    {
        started = true;
    }

    void beginTransmission(uint8_t address)
    {
        transmissionAddress = address;
        transmissionLength = 0;
    }

    size_t write(uint8_t value)
    {
        if (transmissionLength >=
            transmission.size())
        {
            return 0;
        }

        transmission[transmissionLength++] =
            value;

        return 1;
    }

    size_t write(
        const uint8_t* values,
        size_t length)
    {
        size_t written = 0;

        while (written < length &&
               write(values[written]) == 1)
        {
            written++;
        }

        return written;
    }

    uint8_t endTransmission()
    {
        return endTransmission(true);
    }

    uint8_t endTransmission(bool)
    {
        if (!connected ||
            transmissionAddress != deviceAddress)
        {
            return 2;
        }

        if (nextTransmissionError != 0)
        {
            const uint8_t error =
                nextTransmissionError;

            nextTransmissionError = 0;
            return error;
        }

        if (transmissionLength == 0)
            return 0;

        registerPointer = transmission[0];

        for (size_t i = 1;
             i < transmissionLength;
             i++)
        {
            if (registerPointer >=
                registers.size())
            {
                return 3;
            }

            const uint8_t address =
                registerPointer++;

            if (address == 0x0F)
            {
                const uint8_t current =
                    registers[address];

                registers[address] =
                    (current & 0x04) |
                    (transmission[i] & 0x08) |
                    (current & transmission[i] & 0x83);
            }
            else
            {
                registers[address] =
                    transmission[i];
            }
        }

        return 0;
    }

    size_t requestFrom(
        uint8_t address,
        size_t length,
        bool)
    {
        receiveLength = 0;
        receivePosition = 0;

        if (!connected ||
            address != deviceAddress)
        {
            return 0;
        }

        size_t returnedLength = length;

        if (shortRead && returnedLength > 0)
        {
            returnedLength--;
            shortRead = false;
        }

        while (receiveLength < returnedLength &&
               registerPointer < registers.size())
        {
            receive[receiveLength++] =
                registers[registerPointer++];
        }

        return receiveLength;
    }

    int available() const
    {
        return static_cast<int>(
            receiveLength - receivePosition);
    }

    int read()
    {
        if (receivePosition >= receiveLength)
            return -1;

        return receive[receivePosition++];
    }

    std::array<uint8_t, REGISTER_COUNT>
        registers = {};

    uint8_t deviceAddress = 0x68;
    uint8_t sdaPin = 0;
    uint8_t sclPin = 0;
    uint8_t nextTransmissionError = 0;

    bool connected = true;
    bool pinsAccepted = true;
    bool shortRead = false;
    bool started = false;

private:
    std::array<uint8_t, 32> transmission = {};
    std::array<uint8_t, 32> receive = {};

    size_t transmissionLength = 0;
    size_t receiveLength = 0;
    size_t receivePosition = 0;

    uint8_t transmissionAddress = 0;
    uint8_t registerPointer = 0;
};

inline TwoWire Wire;

#endif
