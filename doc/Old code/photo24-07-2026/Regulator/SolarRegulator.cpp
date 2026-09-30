#include <Regulator/SolarRegulator.h>

SolarRegulator::SolarRegulator(
    const char* name,
    Temperature& collector,
    Temperature& tankTop,
    Temperature& tankBottom)
    : Regulator(name),
      collector(collector),
      tankTop(tankTop),
      tankBottom(tankBottom)
{
    settings.startDelta = 8.0;
    settings.stopDelta = 4.0;

    settings.maximumTankTemperature = 80.0;
    settings.minimumCollectorTemperature = 20.0;
}

void SolarRegulator::update(uint32_t now)
{
    (void)now;

    double_t collectorTemperature = collector.getValue();
    double_t topTemperature       = tankTop.getValue();
    double_t bottomTemperature    = tankBottom.getValue();

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