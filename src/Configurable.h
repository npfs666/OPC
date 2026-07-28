#ifndef CONFIGURABLE_h
#define CONFIGURABLE_h

#include <ParameterList.h>

class ParameterEditor;

class Configurable
{
public:

    virtual ~Configurable() = default;

    virtual void registerParameters(ParameterList& list)
    {
        (void)list;
    }

    virtual bool validateParameters(
        const ParameterEditor& editor) const
    {
        (void)editor;
        return true;
    }

protected:

    void beginConfiguration(const char* key)
    {
        configurationKey =
            key != nullptr ? key : "";
    }

    const char* getConfigurationKey() const
    {
        return configurationKey;
    }

private:

    const char* configurationKey = "";
};

#endif
