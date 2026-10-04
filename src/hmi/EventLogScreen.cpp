#include <hmi/EventLogScreen.h>

#include <EventLog.h>
#include <hmi/DisplayTextCodec.h>

#include <cstdio>
#include <cstring>

#ifndef OPC_HOST_TEST
#include <Adafruit_GFX.h>
#include <hmi/TextField.h>
#endif

namespace
{
    constexpr uint16_t COLOR_BLACK = 0x0000;
    constexpr uint16_t COLOR_WHITE = 0xFFFF;
    constexpr uint16_t COLOR_GREY = 0x8410;
    constexpr uint16_t COLOR_RED = 0xF800;
    constexpr uint16_t COLOR_ORANGE = 0xFD20;
    constexpr uint16_t COLOR_CYAN = 0x07FF;
}

size_t EventLogScreen::lastFirstIndex(const EventLog& log)
{
    return log.count() > 0 ? log.count() - 1 : 0;
}

uint16_t EventLogScreen::color(const EventEntry& entry)
{
    switch (entry.kind)
    {
    case EventKind::Fault:
        return COLOR_RED;

    case EventKind::Alarm:
        return COLOR_ORANGE;

    case EventKind::Restart:
        return COLOR_CYAN;

    default:
        return COLOR_WHITE;
    }
}

size_t EventLogScreen::formatTime(
    const EventEntry& entry,
    char* buffer,
    size_t size)
{
    const int written = entry.timeValid
        ? std::snprintf(
              buffer, size, "%02u/%02u %02u:%02u",
              entry.time.day, entry.time.month,
              entry.time.hour, entry.time.minute)
        : std::snprintf(
              buffer, size, "+%lu s",
              static_cast<unsigned long>(entry.uptimeMs / 1000));

    return written > 0 ? std::strlen(buffer) : 0;
}

size_t EventLogScreen::wrap(
    const char* text,
    char lines[MAX_TEXT_LINES][LINE_CHARS + 1])
{
    size_t count = 0;

    if (text == nullptr)
        return 0;

    while (*text == ' ')
        text++;

    while (*text != '\0' && count < MAX_TEXT_LINES)
    {
        size_t length = std::strlen(text);
        size_t cut = length;

        if (length > LINE_CHARS)
        {
            // Dernière espace dans la largeur, sinon coupe nette.
            cut = LINE_CHARS;

            for (size_t i = LINE_CHARS; i > LINE_CHARS / 2; i--)
            {
                if (text[i] == ' ')
                {
                    cut = i;
                    break;
                }
            }
        }

        std::memcpy(lines[count], text, cut);
        lines[count][cut] = '\0';
        count++;

        text += cut;

        while (*text == ' ')
            text++;
    }

    // Texte plus long que l'écran : la dernière ligne se termine par "..".
    if (*text != '\0' && count > 0)
    {
        char* last = lines[count - 1];
        size_t length = std::strlen(last);

        if (length > LINE_CHARS - 2)
            length = LINE_CHARS - 2;

        last[length] = '.';
        last[length + 1] = '.';
        last[length + 2] = '\0';
    }

    return count;
}

#ifndef OPC_HOST_TEST
void EventLogScreen::draw(
    Adafruit_GFX& display,
    const EventLog& log,
    size_t first)
{
    constexpr int16_t MARGIN = 6;
    constexpr int16_t LINE_HEIGHT = 18;
    constexpr int16_t EVENT_GAP = 6;
    constexpr int16_t TOP = 30;

    const int16_t right = display.width() - MARGIN;

    display.fillScreen(COLOR_BLACK);
    display.cp437(true);
    display.setTextWrap(false);
    display.setTextSize(2);

    char text[48];

    // En-tête : titre et position dans le journal.
    display.setTextColor(COLOR_WHITE, COLOR_BLACK);
    display.setCursor(MARGIN, 4);
    display.print("Journal");

    if (log.count() > 0)
    {
        std::snprintf(text, sizeof(text), "%u/%u",
                      static_cast<unsigned>(first + 1),
                      static_cast<unsigned>(log.count()));

        TextField::print(
            display,
            right - TextField::pixelWidth(std::strlen(text), 2),
            4,
            2,
            COLOR_GREY,
            text,
            std::strlen(text));
    }

    display.drawFastHLine(0, 24, display.width(), COLOR_GREY);

    if (log.count() == 0)
    {
        DisplayTextCodec::utf8ToCp437("Aucun événement", text, sizeof(text));
        display.setTextColor(COLOR_GREY, COLOR_BLACK);
        display.setCursor(MARGIN, TOP);
        display.print(text);
        return;
    }

    int16_t y = TOP;

    for (size_t i = first; i < log.count(); i++)
    {
        const EventEntry* entry = log.at(i);

        if (entry == nullptr)
            break;

        char encoded[EventLog::TEXT_SIZE];
        char lines[MAX_TEXT_LINES][LINE_CHARS + 1];

        DisplayTextCodec::utf8ToCp437(entry->text, encoded, sizeof(encoded));
        const size_t lineCount = wrap(encoded, lines);

        const int16_t height =
            static_cast<int16_t>((1 + lineCount) * LINE_HEIGHT);

        // Le premier événement est toujours affiché ; les suivants seulement
        // s'ils tiennent entiers.
        if (i != first && y + height > display.height())
            break;

        // Date en gris, répétitions alignées à droite.
        formatTime(*entry, text, sizeof(text));
        display.setTextColor(COLOR_GREY, COLOR_BLACK);
        display.setCursor(MARGIN, y);
        display.print(text);

        if (entry->repeats > 1)
        {
            std::snprintf(text, sizeof(text), "x%u",
                          static_cast<unsigned>(entry->repeats));
            display.setCursor(
                right - TextField::pixelWidth(std::strlen(text), 2), y);
            display.print(text);
        }

        display.setTextColor(color(*entry), COLOR_BLACK);

        for (size_t line = 0; line < lineCount; line++)
        {
            display.setCursor(
                MARGIN,
                y + static_cast<int16_t>((line + 1) * LINE_HEIGHT));
            display.print(lines[line]);
        }

        y += height + EVENT_GAP;
    }
}
#endif
