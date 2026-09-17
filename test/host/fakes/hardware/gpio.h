#pragma once

#include <cstdint>

using uint = unsigned int;

enum gpio_function { GPIO_FUNC_PWM, GPIO_FUNC_SIO };

namespace FakeGPIO
{
    inline gpio_function functions[30] = {};
    inline bool levels[30] = {};
    inline bool outputs[30] = {};
}

inline void gpio_set_function(uint pin, gpio_function function)
{
    FakeGPIO::functions[pin] = function;
}

inline void gpio_put(uint pin, bool level)
{
    FakeGPIO::levels[pin] = level;
}

inline void gpio_set_dir(uint pin, bool output)
{
    FakeGPIO::outputs[pin] = output;
}
