#include "TestHarness.h"

#include <Adafruit_BMP5xx.h>
#include <Hardware/SensorBoard.h>
#include <Installation.h>
#include <hmi/HomeScreen.h>
#include <hmi/HomeSetpointEditor.h>
#include <hmi/ParameterList.h>

#include <limits>

namespace
{
    void testEditorSteps()
    {
        HomeSetpointEditor editor;
        CHECK_FALSE(editor.isActive());

        editor.begin(20.0, 0.0, 200.0, 0.1, 0);
        CHECK_TRUE(editor.isActive());
        CHECK_FALSE(editor.hasChanged());

        // Crans espacés : un pas chacun, y compris le premier.
        editor.rotate(1, 1000);
        CHECK_NEAR(editor.value(), 20.1, 1e-9);
        editor.rotate(1, 1200);
        CHECK_NEAR(editor.value(), 20.2, 1e-9);
        editor.rotate(-1, 1500);
        CHECK_NEAR(editor.value(), 20.1, 1e-9);
        CHECK_TRUE(editor.hasChanged());

        // Crans rapprochés : dix pas.
        editor.rotate(1, 1530);
        CHECK_NEAR(editor.value(), 21.1, 1e-9);

        // Plusieurs crans d'un coup : rotation rapide aussi.
        editor.rotate(-2, 3000);
        CHECK_NEAR(editor.value(), 19.1, 1e-9);

        // Retour à la valeur de départ : rien à valider.
        editor.rotate(1, 4000);
        editor.rotate(-1, 4100);
        editor.begin(20.0, 0.0, 200.0, 0.1, 5000);
        editor.rotate(1, 6000);
        editor.rotate(-1, 7000);
        CHECK_FALSE(editor.hasChanged());
    }

    void testEditorLimitsAndGrid()
    {
        HomeSetpointEditor editor;

        editor.begin(199.5, 0.0, 200.0, 0.1, 0);
        editor.rotate(3, 1000);
        CHECK_NEAR(editor.value(), 200.0, 1e-9);

        editor.begin(0.3, 0.0, 200.0, 0.1, 0);
        editor.rotate(-2, 1000);
        CHECK_NEAR(editor.value(), 0.0, 1e-9);

        // Une valeur hors grille revient sur le pas.
        editor.begin(20.03, 0.0, 200.0, 0.1, 0);
        editor.rotate(1, 1000);
        CHECK_NEAR(editor.value(), 20.1, 1e-9);
    }

    void testEditorInactivity()
    {
        HomeSetpointEditor editor;
        editor.begin(20.0, 0.0, 200.0, 0.1, 0);
        editor.rotate(1, 1000);

        CHECK_FALSE(editor.inactiveFor(10999, 10000));
        CHECK_TRUE(editor.inactiveFor(11000, 10000));

        // Chaque cran relance le délai.
        editor.rotate(1, 9000);
        CHECK_FALSE(editor.inactiveFor(11000, 10000));
        CHECK_TRUE(editor.inactiveFor(19000, 10000));

        editor.end();
        CHECK_FALSE(editor.isActive());
        CHECK_FALSE(editor.inactiveFor(30000, 10000));
    }

    class SetpointInstallation final : public Installation
    {
    public:
        double_t setpoint = 20.0;
        double_t display = 1.0;
        int32_t count = 3;

        const char* name() const override
        {
            return "Consigne";
        }

        const char* configurationKey() const override
        {
            return "home_setpoint_test";
        }

        bool begin(SensorBoard&, Adafruit_BMP5xx&, ProcessControl&) override
        {
            return true;
        }

        void printHomeScreen(HomeScreenContext&) override
        {
        }

        bool registerParameters()
        {
            parameterList.begin(parameterStorage, MAX_PARAMETERS);

            auto parameters = parameterList.forOwner({
                "regulators", "Regulateur", "thermostat", "Thermostat"
            });

            parameters.addDouble(
                "setpoint", "Consigne", setpoint, 0.0, 200.0, 0.1, 1, "°C");
            parameters.addDouble(
                "measure", "Mesure", display, "°C", true, 1);
            parameters.addInteger(
                "count", "Compteur", count, 0, 10, 1);

            return !parameterList.hasError();
        }

        using Installation::setHomeSetpoint;
    };

    void testInstallationHomeSetpoint()
    {
        SetpointInstallation installation;
        CHECK_TRUE(installation.registerParameters());
        CHECK_TRUE(installation.homeSetpoint() == nullptr);
        CHECK_FALSE(installation.applyHomeSetpoint(25.0));

        // Seul un réglage décimal modifiable convient.
        CHECK_FALSE(installation.setHomeSetpoint("thermostat", "inconnu"));
        CHECK_FALSE(installation.setHomeSetpoint("thermostat", "measure"));
        CHECK_FALSE(installation.setHomeSetpoint("thermostat", "count"));
        CHECK_TRUE(installation.homeSetpoint() == nullptr);

        CHECK_TRUE(installation.setHomeSetpoint("thermostat", "setpoint"));
        CHECK_TRUE(installation.homeSetpoint() != nullptr);

        // Limites et grille du menu.
        CHECK_TRUE(installation.applyHomeSetpoint(25.04));
        CHECK_NEAR(installation.setpoint, 25.0, 1e-9);

        CHECK_TRUE(installation.applyHomeSetpoint(300.0));
        CHECK_NEAR(installation.setpoint, 200.0, 1e-9);

        CHECK_TRUE(installation.applyHomeSetpoint(-5.0));
        CHECK_NEAR(installation.setpoint, 0.0, 1e-9);

        CHECK_FALSE(installation.applyHomeSetpoint(
            std::numeric_limits<double_t>::quiet_NaN()));
        CHECK_NEAR(installation.setpoint, 0.0, 1e-9);
    }
}

void runHomeSetpointTests()
{
    TestHarness::run("consigne accueil : pas et acceleration", testEditorSteps);
    TestHarness::run("consigne accueil : limites et grille", testEditorLimitsAndGrid);
    TestHarness::run("consigne accueil : inactivite", testEditorInactivity);
    TestHarness::run("consigne accueil : installation", testInstallationHomeSetpoint);
}
