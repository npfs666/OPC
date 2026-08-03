#ifndef TEST_FAKE_PRINT_SIZE_H
#define TEST_FAKE_PRINT_SIZE_H

#include <cstddef>
#include <cstdint>

class PrintSize
{
public:
    size_t print(double, uint8_t)
    {
        return 1;
    }
};

#endif
