#include <hmi/TextField.h>

#include <Adafruit_GFX.h>

#include <cstring>

void TextField::print(
    Adafruit_GFX& display,
    int16_t x,
    int16_t y,
    uint8_t size,
    uint16_t color,
    const char* text,
    size_t width,
    Align align,
    uint16_t background)
{
    char field[48];

    if (width >= sizeof(field))
        width = sizeof(field) - 1;

    size_t length = text != nullptr ? std::strlen(text) : 0;

    if (length > width)
        length = width;

    const size_t left =
        align == Align::Left
            ? 0
            : align == Align::Center
                ? (width - length) / 2
                : width - length;

    std::memset(field, ' ', width);

    if (length > 0)
        std::memcpy(field + left, text, length);

    field[width] = '\0';

    display.setTextSize(size);
    display.setTextColor(color, background);
    display.setCursor(x, y);
    display.print(field);
}
