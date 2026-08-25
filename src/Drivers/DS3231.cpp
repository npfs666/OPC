#include <Drivers/DS3231.h>

bool DS3231::begin(
    TwoWire& wire,
    uint8_t address)
{
    bus = nullptr;
    deviceAddress = DEFAULT_ADDRESS;

    if (address == 0 || address > 0x7F)
        return false;

    bus = &wire;
    deviceAddress = address;

    if (isConnected())
        return true;

    bus = nullptr;
    deviceAddress = DEFAULT_ADDRESS;

    return false;
}

bool DS3231::isConnected() const
{
    if (bus == nullptr)
        return false;

    bus->beginTransmission(deviceAddress);

    return bus->endTransmission() == 0;
}

bool DS3231::readRegister(
    Register address,
    uint8_t& value) const
{
    return readRegister(
        static_cast<uint8_t>(address),
        value);
}

bool DS3231::readRegister(
    uint8_t address,
    uint8_t& value) const
{
    return readRegisters(
        address,
        &value,
        1);
}

bool DS3231::writeRegister(
    Register address,
    uint8_t value)
{
    return writeRegister(
        static_cast<uint8_t>(address),
        value);
}

bool DS3231::writeRegister(
    uint8_t address,
    uint8_t value)
{
    return writeRegisters(
        address,
        &value,
        1);
}

bool DS3231::readRegisters(
    Register startAddress,
    uint8_t* values,
    size_t length) const
{
    return readRegisters(
        static_cast<uint8_t>(startAddress),
        values,
        length);
}

bool DS3231::readRegisters(
    uint8_t startAddress,
    uint8_t* values,
    size_t length) const
{
    if (bus == nullptr ||
        values == nullptr ||
        !isValidRange(startAddress, length))
    {
        return false;
    }

    bus->beginTransmission(deviceAddress);

    if (bus->write(startAddress) != 1)
    {
        bus->endTransmission();
        return false;
    }

    if (bus->endTransmission(false) != 0)
        return false;

    const size_t received =
        bus->requestFrom(
            deviceAddress,
            length,
            true);

    if (received != length)
    {
        while (bus->available() > 0)
            bus->read();

        return false;
    }

    for (size_t i = 0; i < length; i++)
    {
        if (bus->available() <= 0)
            return false;

        values[i] = static_cast<uint8_t>(
            bus->read());
    }

    return true;
}

bool DS3231::writeRegisters(
    Register startAddress,
    const uint8_t* values,
    size_t length)
{
    return writeRegisters(
        static_cast<uint8_t>(startAddress),
        values,
        length);
}

bool DS3231::writeRegisters(
    uint8_t startAddress,
    const uint8_t* values,
    size_t length)
{
    if (bus == nullptr ||
        values == nullptr ||
        !isValidRange(startAddress, length))
    {
        return false;
    }

    bus->beginTransmission(deviceAddress);

    const bool bufferReady =
        bus->write(startAddress) == 1 &&
        bus->write(values, length) == length;

    const uint8_t result =
        bus->endTransmission();

    return bufferReady && result == 0;
}

bool DS3231::updateRegister(
    Register address,
    uint8_t mask,
    uint8_t value)
{
    return updateRegister(
        static_cast<uint8_t>(address),
        mask,
        value);
}

bool DS3231::updateRegister(
    uint8_t address,
    uint8_t mask,
    uint8_t value)
{
    if (address ==
        static_cast<uint8_t>(
            Register::Status))
    {
        return false;
    }

    uint8_t currentValue = 0;

    if (!readRegister(address, currentValue))
        return false;

    currentValue =
        (currentValue & ~mask) |
        (value & mask);

    return writeRegister(
        address,
        currentValue);
}

bool DS3231::isValidRange(
    uint8_t startAddress,
    size_t length) const
{
    return
        length > 0 &&
        startAddress < REGISTER_COUNT &&
        length <=
            static_cast<size_t>(
                REGISTER_COUNT - startAddress);
}
