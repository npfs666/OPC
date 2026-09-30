#include <Measurements/HumidityPsychrometric.h>
#include <Physics/Psychrometer.h>
#include <Physics/PT100.h>

void HumidityMeasurement::update()
{
    double_t rDry = board.getResistanceValue(drySensor);
    double_t rWet = board.getResistanceValue(wetSensor);
    
    //_value = Psychrometer::relativeHumidity(board.temperature(drySensor), board.temperature(wetSensor), bme.readPressure());
}