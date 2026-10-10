// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Hardware/AnalogRoute.h>

#include <Hardware/pinout.h>

namespace
{
    namespace Pins = Board::Mcp23017;

    constexpr uint16_t pinMask(Mcp23017Pin pin)
    {
        return static_cast<uint16_t>(1u << pin);
    }

    constexpr Mcp23017Pin CHANNEL_PINS[MAX_RTD] = {
        Pins::INPUT_1_EN,
        Pins::INPUT_2_EN,
        Pins::INPUT_3_EN
    };

    constexpr AnalogRoute toRoute(uint16_t pins)
    {
        return AnalogRoute{
            static_cast<uint8_t>(pins & 0xFF),
            static_cast<uint8_t>(pins >> 8)};
    }
}

AnalogRoute AnalogRoute::forInput(
    const Sensor::Settings& settings,
    uint8_t channel)
{
    using Type = Sensor::Type;
    using Wiring = Sensor::Wiring;

    const Wiring wiring =
        settings.type == Type::Tc ? Wiring::TwoWire : settings.wiring;

    uint16_t pins = 0;

    switch (wiring)
    {
    case Wiring::TwoWire:
        pins |= pinMask(Pins::SW_2_WIRE) | pinMask(Pins::SW_2_3_WIRE);
        break;

    case Wiring::ThreeWire:
        pins |= pinMask(Pins::SW_2_3_WIRE) | pinMask(Pins::SW_3_WIRE);
        break;

    case Wiring::FourWire:
        pins |= pinMask(Pins::SW_4_WIRE);
        break;
    }

    switch (settings.type)
    {
    case Type::Pt100:
        pins |= pinMask(Pins::RREF1k65);
        break;

    case Type::Pt1000:
        pins |= pinMask(Pins::RREF16k5);
        break;

    case Type::Tc:
        pins |= pinMask(Pins::TC_GND) | pinMask(Pins::TC_VCC);
        break;
    }

    if (channel < MAX_RTD)
        pins |= pinMask(CHANNEL_PINS[channel]);

    return toRoute(pins);
}

AnalogRoute AnalogRoute::outputPins()
{
    uint16_t pins =
        pinMask(Pins::SW_2_WIRE) |
        pinMask(Pins::SW_2_3_WIRE) |
        pinMask(Pins::SW_3_WIRE) |
        pinMask(Pins::SW_4_WIRE) |
        pinMask(Pins::RREF1k65) |
        pinMask(Pins::RREF16k5) |
        pinMask(Pins::TC_GND) |
        pinMask(Pins::TC_VCC);

    for (const Mcp23017Pin pin : CHANNEL_PINS)
        pins |= pinMask(pin);

    return toRoute(pins);
}
