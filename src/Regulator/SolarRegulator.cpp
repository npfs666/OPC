#include <Regulator/SolarRegulator.h>

SolarRegulator::SolarRegulator()
{
}

void SolarRegulator::begin(const char* name,
                           Temperature& collector,
                           Temperature& tankTop,
                           Temperature& tankBottom)
{
    begin(
        name,
        name,
        collector,
        tankTop,
        tankBottom);
}

void SolarRegulator::begin(
    const char* key,
    const char* name,
    Temperature& collector,
    Temperature& tankTop,
    Temperature& tankBottom)
{
    Regulator::begin(key, name);

    this->collector = &collector;
    this->tankTop = &tankTop;
    this->tankBottom = &tankBottom;

    settings.startDelta = 8.0;
    settings.stopDelta = 4.0;

    settings.maximumTankTemperature = 80.0;
    settings.minimumCollectorTemperature = 20.0;

    running = false;
}

void SolarRegulator::update(uint32_t now)
{
    (void)now;

    double_t collectorTemperature = collector->getValue();
    double_t topTemperature       = tankTop->getValue();
    double_t bottomTemperature    = tankBottom->getValue();

    bool canRun = true;

    // Ballon trop chaud
    if (topTemperature >= settings.maximumTankTemperature)
        canRun = false;

    // Capteur pas assez chaud
    if (collectorTemperature < settings.minimumCollectorTemperature)
        canRun = false;

    if (canRun)
    {
        double_t delta = collectorTemperature - bottomTemperature;

        if (!running)
        {
            if (delta >= settings.startDelta)
                running = true;
        }
        else
        {
            if (delta <= settings.stopDelta)
                running = false;
        }
    }
    else
    {
        running = false;
    }

    writeCommand(running ? 1.0 : 0.0);
}

void SolarRegulator::registerParameters(
    ParameterList& list)
{
    auto parameters = list.forOwner({
        "regulators",
        "Regulateur",
        getKey(),
        getName()
    });

    parameters.addDouble(
        "start_delta",
        "Delta démarrage",
        settings.startDelta,
        1.0,
        30.0,
        0.5,
        1,
        "K");

    parameters.addDouble(
        "stop_delta",
        "Delta arrêt",
        settings.stopDelta,
        0.0,
        20.0,
        0.5,
        1,
        "K");

    parameters.addDouble(
        "maximum_tank_temperature",
        "Temp. ballon max",
        settings.maximumTankTemperature,
        40.0,
        95.0,
        0.5,
        1,
        "°C");

    parameters.addDouble(
        "minimum_collector_temperature",
        "Temp. capteur min",
        settings.minimumCollectorTemperature,
        0.0,
        100.0,
        0.5,
        1,
        "°C");
}
