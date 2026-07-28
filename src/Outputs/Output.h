#ifndef OUTPUT_H
#define OUTPUT_H

#include <hmi/Displayable.h>
#include <Configurable.h>


class Output : public Displayable, public Configurable
{
public:
    Output();

    void begin(const char* name);

    void begin(
        const char* key,
        const char* name);

    virtual ~Output() = default;

    virtual bool begin() = 0;

    void setCommand(
        double_t value,
        uint32_t now);

    virtual void poll(uint32_t now) = 0;

    virtual void forceSafe() = 0;

    virtual bool applySettings() = 0;

    virtual bool isHealthy() const = 0;

    double_t requestedCommand() const;
    double_t appliedCommand() const;
    uint32_t lastCommandAt() const;

    void registerParameters(
        ParameterList& list) override
    {
        (void)list;
    }

protected:
    void setAppliedCommand(double_t value);

    double_t printValue() const override;
    const char* getUnit() const override;

    double_t requested = 0.0;
    double_t applied = 0.0;
    uint32_t lastCommandTime = 0;
};

#endif
