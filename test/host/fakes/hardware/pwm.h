#pragma once

#include <hardware/gpio.h>

namespace FakePWM
{
    struct Slice
    {
        float divider = 1.0f;
        uint16_t wrap = 0;
        uint16_t levels[2] = {};
        bool enabled = false;
    };

    inline Slice slices[8];
}

inline uint pwm_gpio_to_slice_num(uint pin)
{
    return (pin / 2) % 8;
}

inline void pwm_set_clkdiv(uint slice, float divider)
{
    FakePWM::slices[slice].divider =
        static_cast<uint16_t>(divider * 16) / 16.0f;
}

inline void pwm_set_wrap(uint slice, uint16_t wrap)
{
    FakePWM::slices[slice].wrap = wrap;
}

inline void pwm_set_enabled(uint slice, bool enabled)
{
    FakePWM::slices[slice].enabled = enabled;
}

inline void pwm_set_gpio_level(uint pin, uint16_t level)
{
    FakePWM::slices[pwm_gpio_to_slice_num(pin)].levels[pin % 2] = level;
}
