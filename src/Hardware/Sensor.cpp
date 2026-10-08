// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Hardware/Sensor.h>

#include <Hardware/pinout.h>
#include <hmi/ParameterList.h>

namespace
{
    constexpr ParameterOption TYPE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Sensor::Type::Pt100),
            "PT100"
        },
        {
            static_cast<int32_t>(
                Sensor::Type::Pt1000),
            "PT1000"
        }
    };

    constexpr ParameterOption WIRING_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Sensor::Wiring::TwoWire),
            "2 fils"
        },
        {
            static_cast<int32_t>(
                Sensor::Wiring::ThreeWire),
            "3 fils"
        },
        {
            static_cast<int32_t>(
                Sensor::Wiring::FourWire),
            "4 fils"
        }
    };

    constexpr ParameterOption RANGE_OPTIONS[] = {
        {
            static_cast<int32_t>(
                Sensor::Range::Precise),
            "-200..280°C"
        },
        {
            static_cast<int32_t>(
                Sensor::Range::Extended),
            "-200..850°C"
        }
    };

    constexpr ParameterOption SAMPLE_OPTIONS[] = {
        {2, "2"},
        {4, "4"},
        {8, "8"},
        {16, "16"},
        {32, "32"},
        {64, "64"},
        {128, "128"}
    };
}


Sensor::Sensor() {

}

Sensor::Sensor(const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset)
{
    begin(
        name,
        name,
        type,
        wiring,
        samples,
        offset);
}

Sensor::Sensor(
    const char* key,
    const char* name,
    Type type,
    Wiring wiring,
    uint16_t samples,
    float_t offset)
{
    begin(
        key,
        name,
        type,
        wiring,
        samples,
        offset);
}

/**
 * @brief Adds a sensor to the list
 * 
 * @param number id
 * @param type TYPE_2WIRE TYPE_3WIRE TYPE_4WIRE
 * @param switchPin mux pin
 * @param samples 4 samples -> 1bit improve, 16 -> 2bits, 64 -> 3bits, 256 -> 4bits (oversampling)
 * @param offset Sensor offset
 */
void Sensor::begin(const char* name, Type type, Wiring wiring, uint16_t samples, float_t offset)
{
    begin(
        name,
        name,
        type,
        wiring,
        samples,
        offset);
}

void Sensor::begin(
    const char* key,
    const char* name,
    Type type,
    Wiring wiring,
    uint16_t samples,
    float_t offset)
{
    beginConfiguration(key);
    ownerName = name;
    settings.type = type;
    settings.wiring = wiring;
    settings.samples = samples;
    settings.offset = offset;
    reset();
}

void Sensor::trackSaturation(int32_t value)
{
    saturatedHigh |= value >= 32767;
    saturatedLow |= value <= -32768;
}

void Sensor::add(int32_t value)
{
    trackSaturation(value);
    sum += value;
    sampleCount++;
}
void Sensor::reset()
{
    avgValue = NAN;
    lastAcquisition = MeasurementStatus::NotReady;
    saturatedHigh = false;
    saturatedLow = false;
    readableAtExtendedGain = false;
    sum = 0.0;
    sampleCount = 0.0;
}
void Sensor::compute()
{
    MeasurementStatus status = MeasurementStatus::Ok;

    if (sampleCount == 0)
        status = MeasurementStatus::NotReady;
    // La saturation haute, signature d'une ligne ouverte, est prioritaire.
    else if (saturatedHigh)
        status = readableAtExtendedGain
            ? MeasurementStatus::OverRange
            : MeasurementStatus::Open;
    else if (saturatedLow)
        status = MeasurementStatus::Invalid;

    const double_t result = status == MeasurementStatus::Ok
        ? sum / sampleCount : NAN;
    reset();
    avgValue = result;
    lastAcquisition = status;
}
double_t Sensor::readValue() const
{
    return avgValue;
}

MeasurementStatus Sensor::acquisitionStatus() const
{
    return lastAcquisition;
}

uint8_t Sensor::measurementGain(const Settings& settings)
{
    return settings.range == Range::Extended
        ? RTD_EXTENDED_GAIN
        : RTD_PRECISE_GAIN;
}

uint8_t Sensor::adcGain(const Settings& settings)
{
    return settings.wiring == Wiring::ThreeWire
        ? 2 * measurementGain(settings)
        : measurementGain(settings);
}

bool Sensor::needsRangeDiagnostic() const
{
    return settings.type != Type::Tc &&
           settings.range == Range::Precise &&
           saturatedHigh;
}

void Sensor::setRangeDiagnostic(bool readableAtExtendedGain)
{
    this->readableAtExtendedGain = readableAtExtendedGain;
}

bool Sensor::isAccumulationHalfWay()
{
    if (settings.type == Type::Tc || settings.wiring != Wiring::ThreeWire)
        return false;

    if (sampleCount == (accumulationTarget() / 2))
        return true;
    else
        return false;
}

bool Sensor::isAccumulationDone()
{
    if (sampleCount == accumulationTarget())
        return true;
    else
        return false;
}

int32_t Sensor::accumulationTarget() const
{
    if (settings.type != Type::Tc)
        return settings.samples * RTD_OVERSAMPLING;

    return settings.samples;
}

void Sensor::registerParameters(ParameterList& list)
{
    auto parameters = list.forOwner({
        "inputs",
        "Input",
        getConfigurationKey(),
        ownerName
    });

    if (settings.type == Type::Tc)
    {
        using Type = Physics::Thermocouple::Type;
        static constexpr ParameterOption types[] = {
            {int32_t(Type::B), "B"}, {int32_t(Type::E), "E"},
            {int32_t(Type::J), "J"}, {int32_t(Type::K), "K"},
            {int32_t(Type::N), "N"}, {int32_t(Type::R), "R"},
            {int32_t(Type::S), "S"}, {int32_t(Type::T), "T"}
        };
        parameters.addSelection("tc.type", "Thermocouple", settings.thermocoupleType, types);
    }
    else
    {
        parameters.addSelection(
            "sensor.type",
            "Type",
            settings.type,
            TYPE_OPTIONS);

        parameters.addSelection(
            "sensor.wiring",
            "Câblage",
            settings.wiring,
            WIRING_OPTIONS);

        parameters.addSelection(
            "sensor.range",
            "Plage",
            settings.range,
            RANGE_OPTIONS);

    }

    parameters.addDouble(
        "sensor.offset",
        "Offset",
        settings.offset,
        -5,
        5,
        0.1,
        3,
        "°C",
        false,
        0.001);

    parameters.addSelection(
        "sensor.samples",
        "Samples",
        settings.samples,
        SAMPLE_OPTIONS);

    // 0 à 100 s comme les régulateurs compacts : pas de 1 s, pas fin 0,1 s.
    parameters.addDouble(
        "sensor.filter",
        "Filtre",
        settings.filterTime,
        0.0,
        100.0,
        1.0,
        1,
        "s",
        false,
        0.1);
}
