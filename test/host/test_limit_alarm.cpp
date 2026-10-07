#include "TestHarness.h"

#include <Hardware/pinout.h>
#include <Inputs/DigitalInput.h>
#include <Measurements/Temperature/Temperature.h>
#include <ProcessControl.h>
#include <ProcessSnapshot.h>
#include <Regulator/LimitAlarm.h>
#include <hmi/AlarmDisplay.h>
#include <hmi/ParameterEditor.h>
#include <hmi/ParameterList.h>

#include <cmath>
#include <cstring>

namespace
{
    using Status = MeasurementStatus;
    using Type = LimitAlarm::Type;

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

    // Régulateur de référence dont la consigne est fixée par le test.
    class SetpointSource final : public Regulator
    {
    public:
        double_t setpoint = 50.0;
        bool available = true;

        void update(uint32_t) override
        {
        }

        bool readSetpoint(double_t& value) const override
        {
            value = setpoint;
            return available;
        }
    };

    // Alarme active, sans masquage ni temporisation.
    void prepare(
        LimitAlarm& alarm,
        ControlledTemperature& temperature,
        Type type,
        double_t limit,
        double_t hysteresis)
    {
        alarm.begin("alarm", "Alarme", temperature);
        alarm.settings.enabled = true;
        alarm.settings.type = type;
        alarm.settings.limit = limit;
        alarm.settings.hysteresis = hysteresis;
        alarm.settings.startupMasking = false;
    }

    bool activeAt(
        LimitAlarm& alarm,
        ControlledTemperature& temperature,
        double_t value,
        uint32_t now = 0)
    {
        temperature.set(value);
        alarm.update(now);
        return alarm.isActive();
    }

    void testAbsoluteTypes()
    {
        ControlledTemperature temperature;
        LimitAlarm alarm;

        prepare(alarm, temperature, Type::Max, 80.0, 2.0);
        CHECK_FALSE(activeAt(alarm, temperature, 79.0));
        CHECK_TRUE(activeAt(alarm, temperature, 81.0));
        CHECK_TRUE(alarm.isCommandValid());
        CHECK_NEAR(alarm.readCommand(), 1.0, 0.0);
        CHECK_TRUE(activeAt(alarm, temperature, 78.5));   // hystérésis
        CHECK_FALSE(activeAt(alarm, temperature, 77.9));
        CHECK_NEAR(alarm.readCommand(), 0.0, 0.0);

        prepare(alarm, temperature, Type::Min, 10.0, 1.0);
        CHECK_FALSE(activeAt(alarm, temperature, 11.0));
        CHECK_TRUE(activeAt(alarm, temperature, 9.0));
        CHECK_TRUE(activeAt(alarm, temperature, 10.9));
        CHECK_FALSE(activeAt(alarm, temperature, 11.1));
    }

    void testRelativeTypes()
    {
        ControlledTemperature temperature;
        SetpointSource source;
        source.begin("source");
        LimitAlarm alarm;

        prepare(alarm, temperature, Type::DeviationHigh, 5.0, 1.0);
        alarm.setReference(source);
        CHECK_FALSE(activeAt(alarm, temperature, 54.0));
        CHECK_TRUE(activeAt(alarm, temperature, 56.0));
        CHECK_FALSE(activeAt(alarm, temperature, 53.9));

        prepare(alarm, temperature, Type::DeviationLow, 5.0, 1.0);
        alarm.setReference(source);
        CHECK_TRUE(activeAt(alarm, temperature, 44.0));
        CHECK_FALSE(activeAt(alarm, temperature, 46.1));

        prepare(alarm, temperature, Type::Band, 5.0, 1.0);
        alarm.setReference(source);
        CHECK_TRUE(activeAt(alarm, temperature, 56.0));
        CHECK_FALSE(activeAt(alarm, temperature, 52.0));
        CHECK_TRUE(activeAt(alarm, temperature, 44.0));

        // La consigne suit le régulateur (écart 4 : encore dans
        // l'hystérésis, écart 3 : fin de l'alarme).
        source.setpoint = 40.0;
        CHECK_TRUE(activeAt(alarm, temperature, 44.0));
        CHECK_FALSE(activeAt(alarm, temperature, 43.0));

        // Régulateur arrêté : alarme relative suspendue.
        CHECK_TRUE(activeAt(alarm, temperature, 60.0));
        source.available = false;
        CHECK_FALSE(activeAt(alarm, temperature, 60.0));

        // Sans référence, un type relatif ne déclenche jamais.
        LimitAlarm orphan;
        prepare(orphan, temperature, Type::Band, 5.0, 1.0);
        CHECK_FALSE(activeAt(orphan, temperature, 500.0));
    }

    void testDelay()
    {
        ControlledTemperature temperature;
        LimitAlarm alarm;
        prepare(alarm, temperature, Type::Max, 80.0, 1.0);
        alarm.settings.delay = 10;

        CHECK_FALSE(activeAt(alarm, temperature, 90.0, 1000));
        CHECK_FALSE(activeAt(alarm, temperature, 90.0, 10999));
        CHECK_TRUE(activeAt(alarm, temperature, 90.0, 11000));

        // Un retour sous le seuil relance la temporisation.
        CHECK_FALSE(activeAt(alarm, temperature, 70.0, 12000));
        CHECK_FALSE(activeAt(alarm, temperature, 90.0, 13000));
        CHECK_FALSE(activeAt(alarm, temperature, 90.0, 22999));
        CHECK_TRUE(activeAt(alarm, temperature, 90.0, 23000));
    }

    void testStartupMasking()
    {
        ControlledTemperature temperature;
        LimitAlarm alarm;
        prepare(alarm, temperature, Type::Min, 20.0, 1.0);
        alarm.settings.startupMasking = true;
        alarm.begin("alarm", "Alarme", temperature);

        // Mise en chauffe : sous le seuil sans alarme...
        CHECK_FALSE(activeAt(alarm, temperature, 15.0));
        CHECK_FALSE(activeAt(alarm, temperature, 18.0));

        // ... jusqu'à la première entrée dans la zone normale.
        CHECK_FALSE(activeAt(alarm, temperature, 25.0));
        CHECK_TRUE(activeAt(alarm, temperature, 15.0));

        // Désactivation puis réactivation : masquage réarmé.
        alarm.settings.enabled = false;
        CHECK_FALSE(activeAt(alarm, temperature, 15.0));
        alarm.settings.enabled = true;
        CHECK_FALSE(activeAt(alarm, temperature, 15.0));

        // Un dépassement haut n'est jamais masqué : Max déjà franchi à
        // l'activation (21 °C pour un seuil de 15 °C).
        LimitAlarm high;
        prepare(high, temperature, Type::Max, 15.0, 1.0);
        high.settings.startupMasking = true;
        high.begin("high", "Haute", temperature);
        high.settings.enabled = true;
        high.settings.limit = 15.0;
        CHECK_TRUE(activeAt(high, temperature, 21.0));

        // Hors bande : masqué sous la consigne, pas au-dessus.
        SetpointSource source;
        source.begin("source");
        source.setpoint = 50.0;

        LimitAlarm band;
        prepare(band, temperature, Type::Band, 5.0, 1.0);
        band.settings.startupMasking = true;
        band.begin("band", "Bande", temperature);
        band.settings.enabled = true;
        band.settings.type = Type::Band;
        band.settings.limit = 5.0;
        band.setReference(source);
        CHECK_FALSE(activeAt(band, temperature, 30.0));
        CHECK_TRUE(activeAt(band, temperature, 60.0));
    }

    void testLatchingAndAcknowledge()
    {
        ControlledTemperature temperature;
        LimitAlarm alarm;
        prepare(alarm, temperature, Type::Max, 80.0, 1.0);
        alarm.settings.latching = true;

        CHECK_TRUE(activeAt(alarm, temperature, 90.0));
        CHECK_FALSE(alarm.isLatched());

        // Cause disparue : l'alarme reste, mémorisée.
        CHECK_TRUE(activeAt(alarm, temperature, 70.0));
        CHECK_TRUE(alarm.isLatched());

        alarm.acknowledge();
        CHECK_FALSE(alarm.isActive());
        CHECK_FALSE(activeAt(alarm, temperature, 70.0));

        // Acquittée pendant la cause : reste signalée tant qu'elle dure, mais
        // n'est plus mémorisée ensuite.
        CHECK_TRUE(activeAt(alarm, temperature, 90.0));
        alarm.acknowledge();
        CHECK_TRUE(activeAt(alarm, temperature, 90.0));
        CHECK_FALSE(activeAt(alarm, temperature, 70.0));

        // Nouvel épisode : de nouveau mémorisé.
        CHECK_TRUE(activeAt(alarm, temperature, 90.0));
        CHECK_TRUE(activeAt(alarm, temperature, 70.0));
        CHECK_TRUE(alarm.isLatched());

        // Une reprise (validation du menu) ne l'efface pas.
        alarm.resume(0);
        CHECK_TRUE(alarm.isActive());
        CHECK_TRUE(alarm.isCommandValid());
    }

    void testAcknowledgeInput()
    {
        constexpr uint8_t PIN = Board::Rp2040::DIGITAL_INPUT_1;
        FakeDigitalIO::levels[PIN] = LOW;

        DigitalInput input;
        input.begin("ack", "Acquit", PIN);
        input.poll(0);

        ControlledTemperature temperature;
        LimitAlarm alarm;
        prepare(alarm, temperature, Type::Max, 80.0, 1.0);
        alarm.settings.latching = true;
        alarm.setAcknowledgeInput(input);

        CHECK_TRUE(activeAt(alarm, temperature, 90.0));
        CHECK_TRUE(activeAt(alarm, temperature, 70.0));

        // Front montant : acquittement.
        FakeDigitalIO::levels[PIN] = HIGH;
        input.poll(1);
        CHECK_FALSE(activeAt(alarm, temperature, 70.0));

        // Entrée maintenue : pas de nouvel acquittement.
        CHECK_TRUE(activeAt(alarm, temperature, 90.0));
        CHECK_TRUE(activeAt(alarm, temperature, 70.0));

        FakeDigitalIO::levels[PIN] = LOW;
    }

    void testSensorFault()
    {
        ControlledTemperature temperature;
        LimitAlarm alarm;
        prepare(alarm, temperature, Type::Max, 80.0, 1.0);
        alarm.settings.startupMasking = true;
        alarm.begin("alarm", "Alarme", temperature);

        // Pas encore de mesure : rien.
        temperature.fail(Status::NotReady);
        alarm.update(0);
        CHECK_FALSE(alarm.isActive());

        // Défaut de sonde : alarme, même masquée.
        temperature.fail(Status::Open);
        alarm.update(1000);
        CHECK_TRUE(alarm.isActive());

        alarm.settings.alarmOnFault = false;
        alarm.update(2000);
        CHECK_FALSE(alarm.isActive());

        // Désactivée : commande valide à 0.
        alarm.settings.enabled = false;
        alarm.settings.alarmOnFault = true;
        alarm.update(3000);
        CHECK_FALSE(alarm.isActive());
        CHECK_TRUE(alarm.isCommandValid());
        CHECK_NEAR(alarm.readCommand(), 0.0, 0.0);
    }

    void testParameters()
    {
        ControlledTemperature temperature;
        SetpointSource source;
        source.begin("source");

        LimitAlarm absolute;
        absolute.begin("absolute", "Absolue", temperature);

        LimitAlarm relative;
        relative.begin("relative", "Relative", temperature);
        relative.setReference(source);

        Parameter storage[20];
        ParameterList parameters;
        parameters.begin(storage, 20);
        absolute.registerParameters(parameters);
        relative.registerParameters(parameters);
        CHECK_FALSE(parameters.hasError());
        CHECK_TRUE(parameters.count() == 16);

        const Parameter* absoluteType = parameters.find("absolute", "type");
        const Parameter* relativeType = parameters.find("relative", "type");
        CHECK_TRUE(absoluteType != nullptr && absoluteType->data.selection.count == 2);
        CHECK_TRUE(relativeType != nullptr && relativeType->data.selection.count == 5);
        CHECK_TRUE(std::strcmp(absoluteType->categoryKey, "alarms") == 0);

        // Un type relatif restauré sans référence est refusé.
        ParameterEditor editor;
        editor.begin(parameters);
        editor.capture();

        for (size_t i = 0; i < editor.count(); i++)
        {
            ParameterDraft& draft = editor.get(i);

            if (draft.parameter == absoluteType)
                draft.selectionValue = static_cast<int32_t>(Type::Band);
        }

        CHECK_FALSE(absolute.validateParameters(editor));
        CHECK_TRUE(relative.validateParameters(editor));
    }

    void testProcessIntegration()
    {
        ControlledTemperature temperature;
        LimitAlarm high;
        LimitAlarm low;
        prepare(high, temperature, Type::Max, 80.0, 1.0);
        high.begin("high", "Temp. haute", temperature);
        high.settings.enabled = true;
        high.settings.startupMasking = false;
        high.settings.latching = true;
        prepare(low, temperature, Type::Min, 10.0, 1.0);
        low.begin("low", "Temp. basse", temperature);
        low.settings.enabled = true;
        low.settings.startupMasking = false;

        ProcessControl process;
        CHECK_FALSE(process.handlesMenuAction(
            ProcessControl::ACKNOWLEDGE_ALARMS_ACTION));
        CHECK_TRUE(process.add(temperature));
        CHECK_TRUE(process.add(high));
        CHECK_TRUE(process.add(low));
        CHECK_FALSE(process.add(high));
        CHECK_TRUE(process.handlesMenuAction(
            ProcessControl::ACKNOWLEDGE_ALARMS_ACTION));

        Stream log;
        ProcessSnapshot snapshot;
        char text[32];
        uint16_t color = 0;

        temperature.set(50.0);
        process.updateMeasurementsAndRegulators(1000);
        process.captureSnapshot(snapshot, 1000);
        CHECK_TRUE(snapshot.alarmCount() == 2);
        CHECK_TRUE(AlarmDisplay::format(snapshot, text, sizeof(text), 18, color) == 0);

        // Une alarme en cours : bandeau rouge et journal.
        temperature.set(90.0);
        process.updateMeasurementsAndRegulators(2000);
        process.captureSnapshot(snapshot, 2000);
        const AlarmSample* sample = snapshot.find(high);
        CHECK_TRUE(sample != nullptr && sample->active && !sample->latched);
        AlarmDisplay::format(snapshot, text, sizeof(text), 18, color);
        CHECK_TRUE(std::strcmp(text, "ALARME Temp. haute") == 0);
        CHECK_TRUE(color == AlarmDisplay::COLOR_ACTIVE);

        // Cause disparue : mémorisée, bandeau orange.
        temperature.set(50.0);
        process.updateMeasurementsAndRegulators(3000);
        process.captureSnapshot(snapshot, 3000);
        AlarmDisplay::format(snapshot, text, sizeof(text), 18, color);
        CHECK_TRUE(color == AlarmDisplay::COLOR_LATCHED);

        // Deux alarmes signalées : compte, rouge.
        temperature.set(5.0);
        process.updateMeasurementsAndRegulators(4000);
        process.captureSnapshot(snapshot, 4000);
        AlarmDisplay::format(snapshot, text, sizeof(text), 18, color);
        CHECK_TRUE(std::strcmp(text, "2 ALARMES") == 0);
        CHECK_TRUE(color == AlarmDisplay::COLOR_ACTIVE);

        // Bandeau tronqué à la place disponible.
        CHECK_TRUE(AlarmDisplay::format(snapshot, text, sizeof(text), 4, color) == 4);

        // Acquittement par l'action du menu.
        temperature.set(50.0);
        process.updateMeasurementsAndRegulators(5000);
        CHECK_TRUE(process.executeMenuAction(
            ProcessControl::ACKNOWLEDGE_ALARMS_ACTION));
        process.updateMeasurementsAndRegulators(6000);
        process.captureSnapshot(snapshot, 6000);
        CHECK_TRUE(AlarmDisplay::format(snapshot, text, sizeof(text), 18, color) == 0);

        // Journal : haute active, mémorisée, basse active, basse finie,
        // acquittement, haute finie.
        process.printStatusEvents(log);
        CHECK_TRUE(log.printedLineCount == 6);
    }

    void testLongCondition()
    {
        ControlledTemperature temperature;
        LimitAlarm alarm;
        prepare(alarm, temperature, Type::Max, 80.0, 1.0);
        alarm.settings.delay = 10;

        CHECK_FALSE(activeAt(alarm, temperature, 90.0, 1000));
        CHECK_TRUE(activeAt(alarm, temperature, 90.0, 11000));

        // Condition présente plus de 49 jours : millis() reboucle, l'alarme
        // reste signalée.
        CHECK_TRUE(activeAt(alarm, temperature, 90.0, 3000000000UL));
        CHECK_TRUE(activeAt(alarm, temperature, 90.0, 5000));

        // Fin puis retour de la condition : la tempo repart.
        CHECK_FALSE(activeAt(alarm, temperature, 70.0, 6000));
        CHECK_FALSE(activeAt(alarm, temperature, 90.0, 7000));
        CHECK_FALSE(activeAt(alarm, temperature, 90.0, 16999));
        CHECK_TRUE(activeAt(alarm, temperature, 90.0, 17000));
    }
}

void runLimitAlarmTests()
{
    TestHarness::run("alarmes : seuils absolus", testAbsoluteTypes);
    TestHarness::run("alarmes : ecarts a la consigne", testRelativeTypes);
    TestHarness::run("alarmes : temporisation", testDelay);
    TestHarness::run("alarmes : masquage au demarrage", testStartupMasking);
    TestHarness::run("alarmes : memorisation et acquittement", testLatchingAndAcknowledge);
    TestHarness::run("alarmes : entree d'acquittement", testAcknowledgeInput);
    TestHarness::run("alarmes : defaut de sonde", testSensorFault);
    TestHarness::run("alarmes : parametres", testParameters);
    TestHarness::run("alarmes : processus, journal et bandeau", testProcessIntegration);
    TestHarness::run("alarmes : condition de plus de 49 jours", testLongCondition);
}
