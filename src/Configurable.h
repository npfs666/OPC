#ifndef CONFIGURABLE_h
#define CONFIGURABLE_h

#include <ParameterList.h>

class ParameterEditor;

class Configurable
{
public:

    virtual void registerParameters(ParameterList& list)
    {
    }

    virtual bool validateParameters(
        const ParameterEditor& editor) const
    {
        (void)editor;
        return true;
    }
};

#endif
