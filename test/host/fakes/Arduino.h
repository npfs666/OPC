#ifndef TEST_FAKE_ARDUINO_H
#define TEST_FAKE_ARDUINO_H

#include <cmath>
#include <cstddef>
#include <cstdint>

using byte = uint8_t;

constexpr uint8_t OUTPUT = 1;
constexpr uint8_t INPUT = 0;
constexpr int LOW = 0;
constexpr int HIGH = 1;

namespace FakeDigitalIO
{
    inline uint8_t modes[256] = {};
    inline int levels[256] = {};
}

inline void pinMode(uint8_t pin, uint8_t mode)
{
    FakeDigitalIO::modes[pin] = mode;
}

inline void digitalWrite(uint8_t pin, bool level)
{
    FakeDigitalIO::levels[pin] = level ? HIGH : LOW;
}

inline int digitalRead(uint8_t pin)
{
    return FakeDigitalIO::levels[pin];
}

class Stream
{
public:
    virtual ~Stream() = default;

    size_t print(const char*) { return 0; }
    size_t print(char) { return 0; }
    size_t print(bool) { return 0; }
    size_t print(uint8_t) { return 0; }
    size_t print(int) { return 0; }
    size_t print(unsigned int) { return 0; }
    size_t print(long) { return 0; }
    size_t print(unsigned long) { return 0; }
    size_t print(double) { return 0; }
    size_t print(double, int) { return 0; }

    size_t println() { return 0; }
    size_t println(const char*)
    {
        printedLineCount++;
        return 0;
    }
    size_t println(char) { return 0; }
    size_t println(double) { return 0; }
    size_t println(double, int) { return 0; }

    size_t printedLineCount = 0;
};

class RP2040
{
public:
    enum resetReason_t
    {
        UNKNOWN_RESET,
        PWRON_RESET,
        RUN_PIN_RESET,
        SOFT_RESET,
        WDT_RESET
    };

    void wdt_begin(uint32_t delayMs)
    {
        watchdogTimeoutMs = delayMs;
        watchdogStarted = true;
    }

    void wdt_reset()
    {
        watchdogResetCount++;
    }

    resetReason_t getResetReason() const
    {
        return resetReason;
    }

    void resetFakeState()
    {
        resetReason = PWRON_RESET;
        watchdogTimeoutMs = 0;
        watchdogResetCount = 0;
        watchdogStarted = false;
    }

    resetReason_t resetReason = PWRON_RESET;
    uint32_t watchdogTimeoutMs = 0;
    uint32_t watchdogResetCount = 0;
    bool watchdogStarted = false;
};

inline RP2040 rp2040;

struct mutex_t
{
};

inline void mutex_init(mutex_t*)
{
}

inline void mutex_enter_blocking(mutex_t*)
{
}

inline void mutex_exit(mutex_t*)
{
}

template<typename Value>
constexpr Value constrain(
    Value value,
    Value minimum,
    Value maximum)
{
    return
        value < minimum
            ? minimum
            : value > maximum
                ? maximum
                : value;
}

#define F(value) value

#endif
