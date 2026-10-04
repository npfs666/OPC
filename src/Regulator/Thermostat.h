#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <Regulator/Regulator.h>
#include <Regulator/ScheduledSetpoint.h>
#include <Regulator/SetpointRamp.h>

class Temperature;
class TimeSchedule;

class Thermostat : public Regulator
{
public:
    enum class Mode : uint8_t
    {
        Heating,
        Cooling
    };

    /** Commande automatique, ou relais forcé par l'opérateur. */
    enum class Operation : uint8_t
    {
        Auto,
        ForcedOn,
        ForcedOff
    };

    struct Settings
    {
        Mode mode = Mode::Heating;
        double_t setpoint = 20.0;
        double_t hysteresis = 1.0;

        /*
         * Mode manuel : sortie forcée sans tenir compte de la mesure. Non
         * sauvegardé : retour en Auto au démarrage.
         */
        Operation operation = Operation::Auto;
    };

    Settings settings;
    SetpointRamp setpointRamp;
    ScheduledSetpoint scheduledSetpoint;

    Thermostat();

    void begin(const char* name, Temperature& temperature);
    void begin(
        const char* key,
        const char* name,
        Temperature& temperature);

    /**
     * Option : consigne pendant les plages du programme, reducedSetpoint
     * (ou arrêt, réglable) en dehors. À appeler après begin(), avant
     * l'enregistrement des paramètres. Le programme doit aussi être ajouté
     * au ProcessControl.
     */
    void setSchedule(
        const TimeSchedule& schedule,
        double_t reducedSetpoint);

    void update(uint32_t now);
    void resume(uint32_t now) override;

    bool readSetpoint(double_t& setpoint) const override;

    int8_t actionDirection() const override;

    /** Commande Auto (ni Marche ni Arrêt forcés). */
    bool isAutomatic() const override;

    void print(Stream& stream) const override;

    void registerParameters(ParameterList& list) override;

private:

    Temperature* temperature = nullptr;
};

#endif
