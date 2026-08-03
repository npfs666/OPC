#ifndef TEST_FAKE_ARDUINO_H
#define TEST_FAKE_ARDUINO_H

#include <cmath>
#include <cstddef>
#include <cstdint>

using byte = uint8_t;

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
    size_t println(const char*) { return 0; }
    size_t println(char) { return 0; }
    size_t println(double) { return 0; }
    size_t println(double, int) { return 0; }
};

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
