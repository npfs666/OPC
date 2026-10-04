#include <EventLog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    const char* repeatsSuffix(
        const EventEntry& entry,
        char* buffer,
        size_t size)
    {
        if (entry.repeats <= 1)
            return "";

        std::snprintf(buffer, size, " (x%u)",
                      static_cast<unsigned>(entry.repeats));
        return buffer;
    }
}

EventEntry& EventLog::slot(size_t index)
{
    return entries[(head + CAPACITY - 1 - index) % CAPACITY];
}

const EventEntry* EventLog::at(size_t index) const
{
    if (index >= used)
        return nullptr;

    return &entries[(head + CAPACITY - 1 - index) % CAPACITY];
}

size_t EventLog::count() const
{
    return used;
}

void EventLog::append(const EventEntry& entry)
{
    entries[head] = entry;
    head = (head + 1) % CAPACITY;

    if (used < CAPACITY)
        used++;
}

void EventLog::add(
    uint32_t now,
    const ClockSample& clock,
    EventKind kind,
    bool important,
    const char* text)
{
    if (text == nullptr)
        return;

    // Rafale : même événement parmi les plus récents, encore proche.
    for (size_t i = 0; i < used && i < COALESCE_DEPTH; i++)
    {
        EventEntry& recent = slot(i);

        if (recent.kind == kind &&
            now - recent.uptimeMs <= COALESCE_WINDOW_MS &&
            std::strncmp(recent.text, text, TEXT_SIZE - 1) == 0)
        {
            if (recent.repeats < UINT16_MAX)
                recent.repeats++;

            recent.uptimeMs = now;

            if (recent.important)
                unsavedImportant = true;

            return;
        }
    }

    EventEntry entry;
    entry.uptimeMs = now;
    entry.time = clock.dateTime;
    entry.timeValid = clock.valid;
    entry.kind = kind;
    entry.important = important;
    std::strncpy(entry.text, text, TEXT_SIZE - 1);
    entry.text[TEXT_SIZE - 1] = '\0';

    append(entry);

    if (unprinted < CAPACITY)
        unprinted++;

    if (important)
        unsavedImportant = true;
}

void EventLog::printNew(Stream& stream)
{
    char line[LINE_SIZE];

    while (unprinted > 0)
    {
        unprinted--;

        const EventEntry* entry = at(unprinted);

        if (entry == nullptr)
            continue;

        formatLine(*entry, line, sizeof(line));
        stream.println(line);
    }
}

bool EventLog::hasUnsavedImportant() const
{
    return unsavedImportant;
}

void EventLog::markSaved()
{
    unsavedImportant = false;
}

void EventLog::markUnsaved()
{
    unsavedImportant = true;
}

size_t EventLog::copyImportant(
    EventEntry* destination,
    size_t capacity) const
{
    size_t copied = 0;

    for (size_t i = used; i > 0 && copied < capacity; i--)
    {
        const EventEntry* entry = at(i - 1);

        if (entry->important)
            destination[copied++] = *entry;
    }

    return copied;
}

void EventLog::restore(const EventEntry& entry)
{
    EventEntry restored = entry;
    restored.important = true;
    append(restored);
}

void EventLog::clear()
{
    head = 0;
    used = 0;
    unprinted = 0;
    unsavedImportant = false;
}

size_t EventLog::formatTimestamp(
    const EventEntry& entry,
    char* buffer,
    size_t size)
{
    int written = entry.timeValid
        ? std::snprintf(
              buffer, size, "%02u/%02u %02u:%02u:%02u",
              entry.time.day, entry.time.month,
              entry.time.hour, entry.time.minute, entry.time.second)
        : std::snprintf(
              buffer, size, "+%lu s",
              static_cast<unsigned long>(entry.uptimeMs / 1000));

    return written > 0 ? std::strlen(buffer) : 0;
}

size_t EventLog::formatLine(
    const EventEntry& entry,
    char* buffer,
    size_t size)
{
    char timestamp[24];
    char repeats[16];

    formatTimestamp(entry, timestamp, sizeof(timestamp));

    const int written = std::snprintf(
        buffer, size, "%s %s%s",
        timestamp,
        entry.text,
        repeatsSuffix(entry, repeats, sizeof(repeats)));

    return written > 0 ? std::strlen(buffer) : 0;
}

size_t EventLog::formatCsv(
    const EventEntry& entry,
    char* buffer,
    size_t size)
{
    char date[32] = "-";

    if (entry.timeValid)
    {
        std::snprintf(
            date, sizeof(date), "%04u-%02u-%02u %02u:%02u:%02u",
            entry.time.year, entry.time.month, entry.time.day,
            entry.time.hour, entry.time.minute, entry.time.second);
    }

    const int written = std::snprintf(
        buffer, size, "%s;%lu;%u;%u;%s",
        date,
        static_cast<unsigned long>(entry.uptimeMs),
        static_cast<unsigned>(entry.kind),
        static_cast<unsigned>(entry.repeats),
        entry.text);

    return written > 0 ? std::strlen(buffer) : 0;
}

bool EventLog::parseCsv(const char* line, EventEntry& entry)
{
    if (line == nullptr)
        return false;

    // Quatre champs séparés par ';', puis le texte (le reste de la ligne).
    const char* fields[4];
    const char* cursor = line;

    for (size_t i = 0; i < 4; i++)
    {
        fields[i] = cursor;
        cursor = std::strchr(cursor, ';');

        if (cursor == nullptr)
            return false;

        cursor++;
    }

    EventEntry parsed;

    unsigned year, month, day, hour, minute, second;

    if (std::sscanf(fields[0], "%4u-%2u-%2u %2u:%2u:%2u",
                    &year, &month, &day, &hour, &minute, &second) == 6)
    {
        parsed.time.year = static_cast<uint16_t>(year);
        parsed.time.month = static_cast<uint8_t>(month);
        parsed.time.day = static_cast<uint8_t>(day);
        parsed.time.hour = static_cast<uint8_t>(hour);
        parsed.time.minute = static_cast<uint8_t>(minute);
        parsed.time.second = static_cast<uint8_t>(second);
        parsed.timeValid = true;
    }
    else if (fields[0][0] != '-')
    {
        return false;
    }

    const unsigned long kind = std::strtoul(fields[2], nullptr, 10);

    if (kind > static_cast<unsigned long>(EventKind::Restart))
        return false;

    parsed.uptimeMs = std::strtoul(fields[1], nullptr, 10);
    parsed.kind = static_cast<EventKind>(kind);
    parsed.repeats = static_cast<uint16_t>(
        std::strtoul(fields[3], nullptr, 10));

    if (parsed.repeats == 0)
        parsed.repeats = 1;

    // Texte sans fin de ligne.
    size_t length = std::strcspn(cursor, "\r\n");

    if (length >= TEXT_SIZE)
        length = TEXT_SIZE - 1;

    std::memcpy(parsed.text, cursor, length);
    parsed.text[length] = '\0';
    parsed.important = true;

    entry = parsed;
    return true;
}
