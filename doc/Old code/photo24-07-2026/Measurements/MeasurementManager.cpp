#include "MeasurementManager.h"

MeasurementManager::MeasurementManager()
{
    count = 0;

    for(uint8_t i = 0; i < MAX_MEASUREMENTS; i++)
    {
        measurements[i] = nullptr;
    }
}

bool MeasurementManager::add(Measurement& measurement)
{
    if(count >= MAX_MEASUREMENTS)
        return false;

    measurements[count++] = &measurement;

    return true;
}

void MeasurementManager::update()
{
    for(uint8_t i = 0; i < count; i++)
    {
        measurements[i]->update();
    }
}

Measurement* MeasurementManager::get(uint8_t index)
{
    if(index >= count)
        return nullptr;

    return measurements[index];
}

const Measurement* MeasurementManager::get(uint8_t index) const
{
    if(index >= count)
        return nullptr;

    return measurements[index];
}


Measurement& MeasurementManager::operator[](uint8_t index)
{
    /*if(index >= m_count)
        return nullptr;*/

    return *measurements[index];
}

const Measurement& MeasurementManager::operator[](uint8_t index) const
{
    /*if(index >= m_count)
        return nullptr;*/

    return *measurements[index];
}


uint8_t MeasurementManager::getCount() const
{
    return count;
}