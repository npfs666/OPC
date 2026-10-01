#include <hmi/MeasurementDisplay.h>

#include <ProcessSnapshot.h>
#include <hmi/DisplayTextCodec.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef OPC_HOST_TEST
#include <Adafruit_GFX.h>
#endif

namespace
{
    MeasurementStatus statusOf(const MeasurementSample* sample)
    {
        // Absente du snapshot : pas encore de cycle de mesure depuis le
        // démarrage ou l'application des réglages du menu.
        if (sample == nullptr)
            return MeasurementStatus::NotReady;

        // Garde-fou : une valeur non finie n'est jamais affichée.
        if (sample->status == MeasurementStatus::Ok &&
            !std::isfinite(sample->value))
            return MeasurementStatus::Invalid;

        return sample->status;
    }
}

size_t MeasurementDisplay::format(
    const MeasurementSample* sample,
    char* buffer,
    size_t size,
    size_t maxLabelLength)
{
    if (buffer == nullptr || size == 0)
        return 0;

    const MeasurementStatus status = statusOf(sample);

    if (status != MeasurementStatus::Ok)
    {
        return DisplayTextCodec::utf8ToCp437(
            measurementStatusLabel(status, maxLabelLength),
            buffer,
            size);
    }

    const int written = std::snprintf(
        buffer,
        size,
        "%.*f",
        static_cast<int>(sample->decimals),
        sample->value);

    if (written < 0)
    {
        buffer[0] = '\0';
        return 0;
    }

    size_t length = std::strlen(buffer);

    if (sample->unit != nullptr &&
        sample->unit[0] != '\0' &&
        length + 1 < size)
    {
        buffer[length++] = ' ';
        length += DisplayTextCodec::utf8ToCp437(
            sample->unit,
            buffer + length,
            size - length);
    }

    return length;
}

uint16_t MeasurementDisplay::color(
    const MeasurementSample* sample,
    uint16_t valueColor)
{
    switch (statusOf(sample))
    {
    case MeasurementStatus::Ok:
        return valueColor;

    case MeasurementStatus::NotReady:
        return COLOR_NOT_READY;

    case MeasurementStatus::UnderRange:
    case MeasurementStatus::OverRange:
        return COLOR_OUT_OF_RANGE;

    default:
        return COLOR_FAULT;
    }
}

#ifndef OPC_HOST_TEST
void MeasurementDisplay::print(
    Adafruit_GFX& display,
    const MeasurementSample* sample,
    uint16_t valueColor,
    uint16_t backgroundColor,
    size_t maxLabelLength)
{
    char text[32];

    format(sample, text, sizeof(text), maxLabelLength);

    display.setTextColor(
        color(sample, valueColor),
        backgroundColor);
    display.print(text);
}
#endif
