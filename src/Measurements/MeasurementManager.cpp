#include "MeasurementManager.h"

MeasurementManager::MeasurementManager()
{
    m_count = 0;

    for(uint8_t i = 0; i < MAX_MEASUREMENTS; i++)
    {
        m_measurements[i] = nullptr;
    }
}

bool MeasurementManager::add(Measurement& measurement)
{
    if(m_count >= MAX_MEASUREMENTS)
        return false;

    m_measurements[m_count++] = &measurement;

    return true;
}

void MeasurementManager::update()
{
    for(uint8_t i = 0; i < m_count; i++)
    {
        m_measurements[i]->update();
    }
}

Measurement* MeasurementManager::get(uint8_t index)
{
    if(index >= m_count)
        return nullptr;

    return m_measurements[index];
}

const Measurement* MeasurementManager::get(uint8_t index) const
{
    if(index >= m_count)
        return nullptr;

    return m_measurements[index];
}


Measurement& MeasurementManager::operator[](uint8_t index)
{
    /*if(index >= m_count)
        return nullptr;*/

    return *m_measurements[index];
}

const Measurement& MeasurementManager::operator[](uint8_t index) const
{
    /*if(index >= m_count)
        return nullptr;*/

    return *m_measurements[index];
}


uint8_t MeasurementManager::count() const
{
    return m_count;
}