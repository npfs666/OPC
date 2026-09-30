#ifndef REGULATOR_H
#define REGULATOR_H

#include <hmi/Displayable.h>
#include <Configurable.h>

class Regulator : public Displayable, public Configurable
{
public:
    Regulator();

    void begin(const char* name);
    void begin(
        const char* key,
        const char* name);

    virtual ~Regulator() = default;

    virtual void update(uint32_t now) = 0;

    virtual void resume(uint32_t now);

    double_t readCommand() const;

    bool isCommandValid() const;

    double_t printValue() const override;

    const char* getUnit() const override;

    /**
     * Vrai si la régulation dépend de l'heure du DS3231. Une heure inconnue
     * affiche alors une alerte à la place de l'écran d'accueil.
     */
    virtual bool requiresClock() const
    {
        return false;
    }

    void registerParameters(
        ParameterList& list) override
    {
        (void)list;
    }

protected:

    void writeCommand(double_t value);

    void invalidateCommand();

    double_t command = 0;
    bool commandValid = false;
};

#endif
