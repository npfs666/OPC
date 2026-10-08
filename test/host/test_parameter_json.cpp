// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestHarness.h"

#include <hmi/ParameterEditor.h>
#include <hmi/ParameterJson.h>
#include <hmi/ParameterList.h>

#include <cstring>

namespace
{
    using Scope = ParameterJson::Scope;

    class AcceptAll final : public ParameterRestoreValidator
    {
    public:
        bool validateRestoredParameters(
            const ParameterEditor& editor) const override
        {
            (void)editor;
            return true;
        }
    };

    enum class Mode : uint8_t
    {
        Off,
        On,
        Auto
    };

    constexpr ParameterOption MODE_OPTIONS[] = {
        {int32_t(Mode::Off), "Arret"},
        {int32_t(Mode::On), "Marche"},
        {int32_t(Mode::Auto), "Auto"}
    };

    // Une installation (consigne, mode) et une carte (calibration, N0).
    struct Fixture
    {
        Parameter storage[8];
        ParameterList list;

        double_t setpoint = 20.0;
        Mode mode = Mode::Auto;
        bool enabled = true;
        double_t reference = 1650.0;
        int32_t zero = 0;
        double_t reading = 0.0;

        Fixture()
        {
            list.begin(storage, 8);

            auto installation =
                list.forOwner({"regulators", "Regulateur", "thermo", "Thermo"});
            installation.addDouble(
                "setpoint", "Consigne", setpoint, 0.0, 50.0, 0.5, 1);
            installation.addSelection("mode", "Mode", mode, MODE_OPTIONS);
            installation.addBool("enabled", "Actif", enabled);

            ParameterOwner calibration{
                "calibration", "Calibration", "board.cal", "PT100"};
            calibration.board = true;
            auto board = list.forOwner(calibration);
            board.addDouble(
                "reference", "Rref", reference, 1500.0, 1800.0, 0.001, 3);
            board.addInteger("zero", "N0", zero, -100, 100, 1);

            ParameterOwner diagnostic{
                "calibration", "Calibration", "board.cal", "PT100", false};
            diagnostic.board = true;
            list.forOwner(diagnostic).addDouble(
                "reading", "Lecture", reading, "°C", true);
        }
    };

    bool hasEntry(JsonArrayConst entries, const char* key)
    {
        for (JsonObjectConst entry : entries)
        {
            if (std::strcmp(entry["key"].as<const char*>(), key) == 0)
                return true;
        }

        return false;
    }

    JsonObject findEntry(JsonArray entries, const char* key)
    {
        for (JsonObject entry : entries)
        {
            if (std::strcmp(entry["key"].as<const char*>(), key) == 0)
                return entry;
        }

        return JsonObject();
    }

    void testScopeSplit()
    {
        Fixture fixture;
        CHECK_FALSE(fixture.list.hasError());

        JsonDocument installation;
        JsonDocument board;
        CHECK_TRUE(ParameterJson::write(
            installation.to<JsonArray>(), fixture.list, Scope::Installation));
        CHECK_TRUE(ParameterJson::write(
            board.to<JsonArray>(), fixture.list, Scope::Board));

        // Chaque réglage persistant dans un seul fichier ; la lecture de
        // diagnostic dans aucun.
        CHECK_TRUE(installation.size() == 3);
        CHECK_TRUE(hasEntry(installation.as<JsonArrayConst>(), "setpoint"));
        CHECK_FALSE(hasEntry(installation.as<JsonArrayConst>(), "reference"));
        CHECK_TRUE(board.size() == 2);
        CHECK_TRUE(hasEntry(board.as<JsonArrayConst>(), "reference"));
        CHECK_TRUE(hasEntry(board.as<JsonArrayConst>(), "zero"));
        CHECK_FALSE(hasEntry(board.as<JsonArrayConst>(), "reading"));
    }

    void testRoundTrip()
    {
        Fixture fixture;
        fixture.setpoint = 22.5;
        fixture.mode = Mode::On;
        fixture.enabled = false;
        fixture.reference = 1651.234;
        fixture.zero = -12;

        JsonDocument installation;
        JsonDocument board;
        ParameterJson::write(
            installation.to<JsonArray>(), fixture.list, Scope::Installation);
        ParameterJson::write(board.to<JsonArray>(), fixture.list, Scope::Board);

        // Nouveau démarrage : valeurs par défaut, puis les deux fichiers.
        Fixture restored;
        ParameterEditor editor;
        editor.begin(restored.list);
        editor.capture();

        ParameterJson::ReadState state;
        CHECK_TRUE(ParameterJson::read(
            installation.as<JsonVariantConst>(), restored.list, editor,
            Scope::Installation, state));
        CHECK_TRUE(ParameterJson::read(
            board.as<JsonVariantConst>(), restored.list, editor,
            Scope::Board, state));

        const AcceptAll validator;
        const Parameter* reset[2] = {};
        CHECK_TRUE(editor.keepValidDrafts(
            validator, reset, 2, state.rejected) == 0);
        CHECK_TRUE(editor.apply());

        CHECK_NEAR(restored.setpoint, 22.5, 0.0);
        CHECK_TRUE(restored.mode == Mode::On);
        CHECK_FALSE(restored.enabled);
        CHECK_NEAR(restored.reference, 1651.234, 0.0);
        CHECK_TRUE(restored.zero == -12);
    }

    void testBoardIndependentOfInstallationFile()
    {
        Fixture fixture;
        fixture.setpoint = 30.0;
        fixture.reference = 1700.0;

        // Fichier d'une version précédente : tout dans le même tableau.
        JsonDocument legacy;
        ParameterJson::write(
            legacy.to<JsonArray>(), fixture.list, Scope::Installation);
        ParameterJson::write(
            legacy.as<JsonArray>(), fixture.list, Scope::Board);
        CHECK_TRUE(legacy.size() == 5);

        // Lu pour l'installation : les calibrations y sont ignorées.
        {
            Fixture restored;
            ParameterEditor editor;
            editor.begin(restored.list);
            editor.capture();
            ParameterJson::ReadState state;
            CHECK_TRUE(ParameterJson::read(
                legacy.as<JsonVariantConst>(), restored.list, editor,
                Scope::Installation, state));
            CHECK_TRUE(editor.apply());
            CHECK_NEAR(restored.setpoint, 30.0, 0.0);
            CHECK_NEAR(restored.reference, 1650.0, 0.0);
        }

        // Migration : seules les calibrations sont reprises, comme après un
        // changement d'installation.
        {
            Fixture restored;
            ParameterEditor editor;
            editor.begin(restored.list);
            editor.capture();
            ParameterJson::ReadState state;
            CHECK_TRUE(ParameterJson::read(
                legacy.as<JsonVariantConst>(), restored.list, editor,
                Scope::Board, state));
            CHECK_TRUE(editor.apply());
            CHECK_NEAR(restored.setpoint, 20.0, 0.0);
            CHECK_NEAR(restored.reference, 1700.0, 0.0);
        }
    }

    void testTolerantRead()
    {
        Fixture fixture;
        fixture.setpoint = 25.0;
        fixture.mode = Mode::Off;

        JsonDocument document;
        JsonArray entries = document.to<JsonArray>();
        ParameterJson::write(entries, fixture.list, Scope::Installation);

        // Réglage supprimé par une nouvelle version.
        JsonObject removed = entries.add<JsonObject>();
        removed["owner_key"] = "thermo";
        removed["key"] = "old_setting";
        removed["type"] = "double";
        removed["value"] = 1.0;

        // Liste d'options complétée : la valeur enregistrée reste valable.
        findEntry(entries, "mode")["options"].to<JsonArray>();

        // Type changé : ce réglage seul revient par défaut.
        findEntry(entries, "enabled")["type"] = "integer";

        // Doublon : la première entrée est gardée.
        JsonObject duplicate = entries.add<JsonObject>();
        duplicate["owner_key"] = "thermo";
        duplicate["key"] = "setpoint";
        duplicate["type"] = "double";
        duplicate["value"] = 40.0;

        Fixture restored;
        restored.enabled = false;
        ParameterEditor editor;
        editor.begin(restored.list);
        editor.capture();

        ParameterJson::ReadState state;
        CHECK_TRUE(ParameterJson::read(
            document.as<JsonVariantConst>(), restored.list, editor,
            Scope::Installation, state));

        const AcceptAll validator;
        const Parameter* reset[2] = {};
        CHECK_TRUE(editor.keepValidDrafts(
            validator, reset, 2, state.rejected) == 1);
        CHECK_TRUE(reset[0] != nullptr &&
                   std::strcmp(reset[0]->key, "setpoint") == 0);
        CHECK_TRUE(editor.apply());

        CHECK_NEAR(restored.setpoint, 25.0, 0.0);
        CHECK_TRUE(restored.mode == Mode::Off);
        CHECK_FALSE(restored.enabled);
    }

    void testInvalidValueResetsOwner()
    {
        Fixture fixture;
        JsonDocument document;
        JsonArray entries = document.to<JsonArray>();
        ParameterJson::write(entries, fixture.list, Scope::Board);

        // Rref hors de la plage réduite d'une nouvelle version : seul ce
        // réglage revient par défaut, N0 est gardé.
        findEntry(entries, "reference")["value"] = 2000.0;
        findEntry(entries, "zero")["value"] = 42;

        Fixture restored;
        ParameterEditor editor;
        editor.begin(restored.list);
        editor.capture();

        ParameterJson::ReadState state;
        CHECK_TRUE(ParameterJson::read(
            document.as<JsonVariantConst>(), restored.list, editor,
            Scope::Board, state));

        const AcceptAll validator;
        const Parameter* reset[2] = {};
        CHECK_TRUE(editor.keepValidDrafts(
            validator, reset, 2, state.rejected) == 1);
        CHECK_TRUE(editor.apply());

        CHECK_NEAR(restored.reference, 1650.0, 0.0);
        CHECK_TRUE(restored.zero == 42);
    }

    void testRejectsNonArray()
    {
        Fixture fixture;
        ParameterEditor editor;
        editor.begin(fixture.list);
        editor.capture();

        JsonDocument document;
        document["parameters"] = "absent";

        ParameterJson::ReadState state;
        CHECK_FALSE(ParameterJson::read(
            document["parameters"], fixture.list, editor,
            Scope::Installation, state));
        CHECK_FALSE(ParameterJson::read(
            document["missing"], fixture.list, editor,
            Scope::Board, state));
    }
}

void runParameterJsonTests()
{
    TestHarness::run("ParameterJson scope split", testScopeSplit);
    TestHarness::run("ParameterJson round trip", testRoundTrip);
    TestHarness::run(
        "ParameterJson board independent of installation",
        testBoardIndependentOfInstallationFile);
    TestHarness::run("ParameterJson tolerant read", testTolerantRead);
    TestHarness::run(
        "ParameterJson invalid value resets owner",
        testInvalidValueResetsOwner);
    TestHarness::run("ParameterJson rejects non array", testRejectsNonArray);
}
