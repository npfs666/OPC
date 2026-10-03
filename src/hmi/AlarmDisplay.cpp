#include <hmi/AlarmDisplay.h>

#include <ProcessSnapshot.h>
#include <hmi/DisplayTextCodec.h>

#include <cstdio>
#include <cstring>

#ifndef OPC_HOST_TEST
#include <Adafruit_GFX.h>
#include <hmi/TextField.h>
#endif

size_t AlarmDisplay::format(
    const ProcessSnapshot& snapshot,
    char* buffer,
    size_t size,
    size_t maxChars,
    uint16_t& color)
{
    color = COLOR_LATCHED;

    if (buffer == nullptr || size == 0)
        return 0;

    buffer[0] = '\0';

    size_t signaled = 0;
    const AlarmSample* shown = nullptr;

    for (size_t i = 0; i < snapshot.alarmCount(); i++)
    {
        const AlarmSample* alarm = snapshot.alarmAt(i);

        if (alarm == nullptr || !alarm->active)
            continue;

        signaled++;

        // Une alarme en cours passe devant une alarme mémorisée.
        if (!alarm->latched)
            color = COLOR_ACTIVE;

        if (shown == nullptr || (shown->latched && !alarm->latched))
            shown = alarm;
    }

    if (signaled == 0)
        return 0;

    char text[48];

    if (signaled == 1)
        std::snprintf(text, sizeof(text), "ALARME %s", shown->name);
    else
        std::snprintf(text, sizeof(text), "%u ALARMES",
                      static_cast<unsigned>(signaled));

    size_t length = DisplayTextCodec::utf8ToCp437(text, buffer, size);

    if (length > maxChars && maxChars < size)
    {
        buffer[maxChars] = '\0';
        length = maxChars;
    }

    return length;
}

#ifndef OPC_HOST_TEST
void AlarmDisplay::printBanner(
    Adafruit_GFX& display,
    const ProcessSnapshot& snapshot,
    int16_t y)
{
    char text[BANNER_CHARS + 1];
    uint16_t color = COLOR_ACTIVE;

    format(snapshot, text, sizeof(text), BANNER_CHARS, color);

    TextField::print(
        display,
        (display.width() - TextField::pixelWidth(BANNER_CHARS, 2)) / 2,
        y,
        2,
        color,
        text,
        BANNER_CHARS,
        TextField::Align::Center);
}
#endif
