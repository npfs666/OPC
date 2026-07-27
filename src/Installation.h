#ifndef INSTALLATION_H
#define INSTALLATION_H

#include <Hardware/pinout.h>
#include <ParameterList.h>

class SensorBoard;
class Adafruit_BME280;
class ProcessControl;
class MenuBuilder;
class Storage;

class Installation
{
public:

    virtual ~Installation() = default;

    virtual const char* name() const = 0;

    virtual bool begin(
        SensorBoard& board,
        Adafruit_BME280& bme,
        ProcessControl& process) = 0;

    virtual void buildMenu(MenuBuilder& menu) = 0;

    virtual void load(Storage& storage) = 0;

    virtual void save(Storage& storage) = 0;

    virtual void factoryReset() = 0;

    ParameterList& getParameters()
    {
        return parameterList;
    }

protected: 

    Parameter parameterStorage[MAX_PARAMETERS];
    ParameterList parameterList;
};

#endif