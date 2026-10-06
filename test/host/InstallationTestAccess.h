#ifndef INSTALLATION_TEST_ACCESS_H
#define INSTALLATION_TEST_ACCESS_H

#include <Installation.h>
#include <ProcessControl.h>

/**
 * Démarre une installation comme OPC::initMeasurements() : paramètres,
 * begin(), réglages communs, puis glue reliée au ProcessControl et sorties
 * initialisées. Ami d'Installation dans les tests sur l'hôte.
 */
class InstallationTestAccess
{
public:
    static bool start(
        Installation& installation,
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process)
    {
        if (!installation.prepareParameterRegistration() ||
            !installation.begin(board, bmp580, process) ||
            !installation.completeParameterRegistration() ||
            installation.getParameters().hasError())
        {
            return false;
        }

        process.setLogic(installation);
        return process.beginOutputs();
    }
};

#endif
