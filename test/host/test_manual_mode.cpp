#include "TestHarness.h"

#include <Measurements/Temperature/Temperature.h>
#include <Regulator/PID.h>
#include <Regulator/Thermostat.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <utility>

namespace
{
    using Status = MeasurementStatus;

    class ControlledTemperature final : public Temperature
    {
    public:
        ControlledTemperature()
        {
            begin("temperature");
        }

        void update() override
        {
        }

        void set(double_t value)
        {
            setValue(value);
            setStatus(Status::Ok);
        }

        void fail(Status status)
        {
            setValue(NAN);
            setStatus(status);
        }
    };

    // PID de chauffage en régime établi à 45 °C pour 50 °C de consigne.
    void runAutomatic(PID& pid, ControlledTemperature& temperature)
    {
        pid.begin("pid", temperature);
        pid.settings.setpoint = 50.0;
        pid.settings.kp = 0.05;
        pid.settings.ti = 100.0;
        pid.settings.td = 0.0;

        temperature.set(45.0);
        for (uint32_t now = 0; now <= 20000; now += 1000)
            pid.update(now);
    }

    void testPIDTracking()
    {
        ControlledTemperature temperature;
        PID pid;
        runAutomatic(pid, temperature);

        // En Auto, la sortie manuelle suit la sortie calculée.
        const double_t automatic = pid.readCommand();
        CHECK_TRUE(automatic > 0.0 && automatic < 1.0);
        CHECK_NEAR(pid.settings.manualOutput, automatic * 100.0, 1e-9);

        // Passage en manuel : même sortie, pas d'à-coup.
        pid.settings.operation = PID::Operation::Manual;
        pid.update(21000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), automatic, 1e-9);
    }

    void testPIDManualOutput()
    {
        ControlledTemperature temperature;
        PID pid;
        runAutomatic(pid, temperature);

        pid.settings.operation = PID::Operation::Manual;
        pid.settings.manualOutput = 70.0;
        pid.update(21000);
        CHECK_NEAR(pid.readCommand(), 0.70, 1e-9);

        // Ni la mesure ni le défaut de sonde n'agissent en manuel.
        temperature.fail(Status::Open);
        pid.update(22000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.70, 1e-9);

        // Ni l'arrêt de la régulation automatique.
        pid.settings.enabled = false;
        pid.update(23000);
        CHECK_NEAR(pid.readCommand(), 0.70, 1e-9);

        // La sortie manuelle reste dans 0..100 %.
        pid.settings.manualOutput = 150.0;
        pid.update(24000);
        CHECK_NEAR(pid.readCommand(), 1.0, 1e-9);

        // Pas d'autotune en manuel.
        temperature.set(45.0);
        CHECK_FALSE(pid.startAutoTune(25000));
    }

    void testPIDBumplessReturn()
    {
        ControlledTemperature temperature;
        PID pid;
        runAutomatic(pid, temperature);

        pid.settings.operation = PID::Operation::Manual;
        pid.settings.manualOutput = 70.0;
        pid.update(21000);

        // Retour en Auto : le premier cycle garde la sortie manuelle...
        pid.settings.operation = PID::Operation::Auto;
        pid.update(22000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.70, 1e-9);

        // ... puis le calcul repart de cette valeur (intégrale recalculée).
        pid.update(23000);
        CHECK_NEAR(pid.readCommand(), 0.70, 0.01);

        // Le suivi reprend.
        CHECK_NEAR(pid.settings.manualOutput, pid.readCommand() * 100.0, 1e-9);

        // Même retour par le menu : resume() efface la commande pendant
        // l'application, la reprise part quand même de la sortie manuelle.
        pid.settings.operation = PID::Operation::Manual;
        pid.settings.manualOutput = 40.0;
        pid.update(24000);
        pid.settings.operation = PID::Operation::Auto;
        pid.resume(25000);
        CHECK_FALSE(pid.isCommandValid());

        pid.update(26000);
        CHECK_TRUE(pid.isCommandValid());
        CHECK_NEAR(pid.readCommand(), 0.40, 1e-9);

        pid.update(27000);
        CHECK_NEAR(pid.readCommand(), 0.40, 0.01);
    }

    void testThermostatManual()
    {
        ControlledTemperature temperature;
        Thermostat thermostat;
        thermostat.begin("thermostat", "Thermostat", temperature);
        thermostat.settings.setpoint = 20.0;
        thermostat.settings.hysteresis = 2.0;

        temperature.set(25.0);
        thermostat.update(0);
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);

        // Marche forcée, même au-dessus de la consigne et sur défaut.
        thermostat.settings.operation = Thermostat::Operation::ForcedOn;
        thermostat.update(1000);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        temperature.fail(Status::Short);
        thermostat.update(2000);
        CHECK_TRUE(thermostat.isCommandValid());
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        thermostat.settings.operation = Thermostat::Operation::ForcedOff;
        temperature.set(10.0);
        thermostat.update(3000);
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);

        // Retour en Auto dans la bande : l'état forcé est conservé.
        thermostat.settings.operation = Thermostat::Operation::ForcedOn;
        temperature.set(20.5);
        thermostat.update(4000);
        thermostat.settings.operation = Thermostat::Operation::Auto;
        thermostat.update(5000);
        CHECK_NEAR(thermostat.readCommand(), 1.0, 0.0);

        // Puis régulation normale.
        temperature.set(22.0);
        thermostat.update(6000);
        CHECK_NEAR(thermostat.readCommand(), 0.0, 0.0);
    }

    void testManualParametersAreNotSaved()
    {
        ControlledTemperature temperature;

        PID pid;
        pid.begin("pid", temperature);
        Thermostat thermostat;
        thermostat.begin("thermostat", "Thermostat", temperature);

        Parameter storage[24];
        ParameterList parameters;
        parameters.begin(storage, 24);
        pid.registerParameters(parameters);
        thermostat.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());

        // Non sauvegardés : retour en Auto au démarrage.
        for (const auto& key : {
                 std::pair<const char*, const char*>{"pid", "operation"},
                 {"pid", "manual_output"},
                 {"thermostat", "operation"}})
        {
            const Parameter* parameter =
                parameters.find(key.first, key.second);
            CHECK_TRUE(parameter != nullptr);
            CHECK_TRUE(parameter != nullptr && !parameter->persistent);
        }

        // Les autres réglages restent sauvegardés.
        const Parameter* setpoint = parameters.find("pid", "setpoint");
        CHECK_TRUE(setpoint != nullptr && setpoint->persistent);
    }

    // Brouillon d'un paramètre, pour simuler une saisie dans le menu.
    ParameterDraft* draftFor(
        ParameterEditor& editor,
        const Parameter* parameter)
    {
        for (size_t i = 0; i < editor.count(); i++)
        {
            if (editor.get(i).parameter == parameter)
                return &editor.get(i);
        }

        return nullptr;
    }

    void testLiveParameters()
    {
        ControlledTemperature temperature;

        PID pid;
        pid.begin("pid", temperature);
        Thermostat thermostat;
        thermostat.begin("thermostat", "Thermostat", temperature);

        Parameter storage[24];
        ParameterList parameters;
        parameters.begin(storage, 24);
        pid.registerParameters(parameters);
        thermostat.registerParameters(parameters);
        CHECK_FALSE(parameters.setLive("pid", "inconnu"));

        // Réglages de conduite : commande, sortie manuelle, consigne.
        for (const auto& key : {
                 std::pair<const char*, const char*>{"pid", "operation"},
                 {"pid", "manual_output"},
                 {"pid", "setpoint"},
                 {"thermostat", "operation"},
                 {"thermostat", "setpoint"}})
        {
            const Parameter* parameter =
                parameters.find(key.first, key.second);
            CHECK_TRUE(parameter != nullptr && parameter->live);
        }

        const Parameter* setpoint = parameters.find("pid", "setpoint");
        const Parameter* kp = parameters.find("pid", "kp");
        CHECK_TRUE(kp != nullptr && !kp->live);

        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();
        CHECK_FALSE(editor.hasOnlyLiveChanges());

        // Consigne seule : application sans arrêt.
        ParameterDraft* setpointDraft = draftFor(editor, setpoint);
        CHECK_TRUE(setpointDraft != nullptr);
        if (setpointDraft == nullptr)
            return;
        setpointDraft->numberValue += 1.0;
        CHECK_TRUE(editor.hasOnlyLiveChanges());

        // Avec un gain : application complète.
        ParameterDraft* kpDraft = draftFor(editor, kp);
        CHECK_TRUE(kpDraft != nullptr);
        if (kpDraft == nullptr)
            return;
        kpDraft->numberValue += 0.5;
        CHECK_FALSE(editor.hasOnlyLiveChanges());
    }
}

void runManualModeTests()
{
    TestHarness::run("manuel : suivi de la sortie PID", testPIDTracking);
    TestHarness::run("manuel : sortie PID fixee", testPIDManualOutput);
    TestHarness::run("manuel : retour PID sans a-coup", testPIDBumplessReturn);
    TestHarness::run("manuel : thermostat force", testThermostatManual);
    TestHarness::run("manuel : reglages non sauvegardes", testManualParametersAreNotSaved);
    TestHarness::run("menu : reglages de conduite", testLiveParameters);
}
