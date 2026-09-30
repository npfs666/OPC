#ifndef SOLARREGULATOR_H
#define SOLARREGULATOR_H

#include <Regulator/Regulator.h>

class Temperature;
class TimeSchedule;

/**
 * Régulateur solaire différentiel : la pompe tourne quand le capteur est plus
 * chaud que le bas du ballon.
 *
 * Option mode vacances (setHolidaySchedule()) : pendant les plages du
 * programme (la nuit), la pompe décharge le ballon dans le capteur froid
 * jusqu'à la température vacances, pour éviter les surchauffes du lendemain.
 */

class SolarRegulator : public Regulator
{
public:

    struct Settings
    {
        double_t startDelta;
        double_t stopDelta;

        double_t maximumTankTemperature;

        double_t minimumCollectorTemperature;

        /* Décharge nocturne, disponible avec setHolidaySchedule(). */
        bool holidayMode;
        double_t holidayTankTemperature;
    };

    Settings settings;

    SolarRegulator();

    void begin(const char* name,
        Temperature& collector,
        Temperature& tankTop,
        Temperature& tankBottom);

    void begin(
        const char* key,
        const char* name,
        Temperature& collector,
        Temperature& tankTop,
        Temperature& tankBottom);

    /**
     * Active l'option mode vacances : le programme définit les heures de
     * décharge (la nuit). À appeler après begin(), avant l'enregistrement des
     * paramètres. Le programme doit aussi être ajouté au ProcessControl.
     */
    void setHolidaySchedule(const TimeSchedule& schedule);

    /** Vrai si la pompe tourne pour décharger le ballon. */
    bool isDischarging() const;

    void update(uint32_t now) override;

    void resume(uint32_t now) override;

    void registerParameters(
        ParameterList& list) override;

    bool validateParameters(
        const ParameterEditor& editor)
        const override;
    
    void print(Stream& stream) const override;

private:

    Temperature* collector = nullptr;

    Temperature* tankTop = nullptr;

    Temperature* tankBottom = nullptr;

    const TimeSchedule* holidaySchedule = nullptr;

    bool running = false;
    bool discharging = false;

    bool updateDischarge(
        double_t collectorTemperature,
        double_t bottomTemperature);
};

#endif
