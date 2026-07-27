#ifndef CONFIGURABLE_h
#define CONFIGURABLE_h

#include <ParameterList.h>

class Configurable
{
public:

    virtual void registerParameters(ParameterList& list)
    {
    }
};

#endif