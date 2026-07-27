#ifndef DISPLAY_TEXT_CODEC_H
#define DISPLAY_TEXT_CODEC_H

#include <Arduino.h>

class DisplayTextCodec final
{
public:
    static size_t utf8ToCp437(
        const char* source,
        char* destination,
        size_t destinationSize);

private:
    static bool isContinuationByte(uint8_t value);

    static uint32_t decodeUtf8(
        const uint8_t*& source);

    static uint8_t unicodeToCp437(
        uint32_t codePoint);
};

#endif
