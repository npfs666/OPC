#include <hmi/DisplayTextCodec.h>

size_t DisplayTextCodec::utf8ToCp437(
    const char* source,
    char* destination,
    size_t destinationSize)
{
    if (destination == nullptr ||
        destinationSize == 0)
    {
        return 0;
    }

    destination[0] = '\0';

    if (source == nullptr)
        return 0;

    const uint8_t* cursor =
        reinterpret_cast<const uint8_t*>(source);

    size_t written = 0;

    while (*cursor != 0 &&
           (written + 1) < destinationSize)
    {
        const uint32_t codePoint =
            decodeUtf8(cursor);

        destination[written] =
            static_cast<char>(
                unicodeToCp437(codePoint));

        written++;
    }

    destination[written] = '\0';

    return written;
}

bool DisplayTextCodec::isContinuationByte(
    uint8_t value)
{
    return (value & 0xC0) == 0x80;
}

uint32_t DisplayTextCodec::decodeUtf8(
    const uint8_t*& source)
{
    constexpr uint32_t REPLACEMENT_CHARACTER = 0xFFFD;

    const uint8_t first = source[0];

    if (first < 0x80)
    {
        source++;
        return first;
    }

    if (first >= 0xC2 &&
        first <= 0xDF &&
        isContinuationByte(source[1]))
    {
        const uint32_t codePoint =
            (static_cast<uint32_t>(first & 0x1F) << 6) |
            static_cast<uint32_t>(source[1] & 0x3F);

        source += 2;
        return codePoint;
    }

    if (first >= 0xE0 &&
        first <= 0xEF &&
        isContinuationByte(source[1]) &&
        isContinuationByte(source[2]))
    {
        const uint32_t codePoint =
            (static_cast<uint32_t>(first & 0x0F) << 12) |
            (static_cast<uint32_t>(source[1] & 0x3F) << 6) |
            static_cast<uint32_t>(source[2] & 0x3F);

        const bool isOverlong =
            codePoint < 0x800;

        const bool isSurrogate =
            codePoint >= 0xD800 &&
            codePoint <= 0xDFFF;

        source += 3;

        if (isOverlong || isSurrogate)
            return REPLACEMENT_CHARACTER;

        return codePoint;
    }

    if (first >= 0xF0 &&
        first <= 0xF4 &&
        isContinuationByte(source[1]) &&
        isContinuationByte(source[2]) &&
        isContinuationByte(source[3]))
    {
        const uint32_t codePoint =
            (static_cast<uint32_t>(first & 0x07) << 18) |
            (static_cast<uint32_t>(source[1] & 0x3F) << 12) |
            (static_cast<uint32_t>(source[2] & 0x3F) << 6) |
            static_cast<uint32_t>(source[3] & 0x3F);

        source += 4;

        if (codePoint < 0x10000 ||
            codePoint > 0x10FFFF)
        {
            return REPLACEMENT_CHARACTER;
        }

        return codePoint;
    }

    source++;
    return REPLACEMENT_CHARACTER;
}

uint8_t DisplayTextCodec::unicodeToCp437(
    uint32_t codePoint)
{
    if (codePoint < 0x80)
        return static_cast<uint8_t>(codePoint);

    switch (codePoint)
    {
    case 0x00A0:
        return ' ';

    case 0x00AB:
        return 0xAE;

    case 0x00B0:
        return 0xF8;

    case 0x00B2:
        return 0xFD;

    case 0x00B3:
        return '3';

    case 0x00B5:
        return 0xE6;

    case 0x00BB:
        return 0xAF;

    case 0x00C4:
        return 0x8E;

    case 0x00C7:
        return 0x80;

    case 0x00C9:
        return 0x90;

    case 0x00D6:
        return 0x99;

    case 0x00DC:
        return 0x9A;

    case 0x00DF:
        return 0xE1;

    case 0x00E0:
        return 0x85;

    case 0x00E2:
        return 0x83;

    case 0x00E4:
        return 0x84;

    case 0x00E7:
        return 0x87;

    case 0x00E8:
        return 0x8A;

    case 0x00E9:
        return 0x82;

    case 0x00EA:
        return 0x88;

    case 0x00EB:
        return 0x89;

    case 0x00EE:
        return 0x8C;

    case 0x00EF:
        return 0x8B;

    case 0x00F4:
        return 0x93;

    case 0x00F6:
        return 0x94;

    case 0x00F9:
        return 0x97;

    case 0x00FB:
        return 0x96;

    case 0x00FC:
        return 0x81;

    case 0x03A9:
        return 0xEA;

    case 0x2019:
        return '\'';

    default:
        return '?';
    }
}
