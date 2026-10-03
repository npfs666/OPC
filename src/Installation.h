#ifndef INSTALLATION_H
#define INSTALLATION_H

#include <Hardware/pinout.h>
#include <hmi/MenuBuilder.h>
#include <hmi/ParameterList.h>

class SensorBoard;
class Adafruit_BMP5xx;
struct HomeScreenContext;
class ProcessControl;
class ParameterEditor;
class OPC;

class Installation
{
public:

    struct MenuSettings
    {
        uint32_t inactivityTimeoutSeconds = 10;
    };

    struct SafetySettings
    {
        uint32_t measurementTimeoutSeconds = 30;
    };

    MenuSettings menuSettings;
    SafetySettings safetySettings;

    virtual ~Installation() = default;

    // Identifiant persistant, stable et non traduit.
    virtual const char* configurationKey() const = 0;

    // Nom lisible destiné à l'utilisateur.
    virtual const char* name() const = 0;

    virtual bool begin(
        SensorBoard& board,
        Adafruit_BMP5xx& bmp580,
        ProcessControl& process) = 0;

    virtual bool requiresBMP580() const
    {
        return false;
    }

    virtual void printHomeScreen(
        HomeScreenContext& context) = 0;

    /**
     * Copie rapidement l'état propre à l'écran pendant que le contrôle est
     * verrouillé. L'affichage doit ensuite utiliser uniquement cette copie.
     */
    virtual void captureHomeScreenState()
    {
    }

    virtual bool validateParameters(
        const ParameterEditor& editor) const
    {
        (void)editor;
        return true;
    }

    virtual void onParametersApplied()
    {
    }

    /** Appelé sur le cœur de contrôle juste avant la capture du menu. */
    virtual void onMenuOpened()
    {
    }

    /** Ajoute des commandes non persistantes après la création des groupes. */
    virtual bool addMenuActions(
        MenuBuilder& menu) const
    {
        (void)menu;
        return true;
    }

    /** Exécute une commande de menu sur le cœur de contrôle. */
    virtual bool executeMenuAction(
        MenuBuilder::ActionId actionId)
    {
        (void)actionId;
        return false;
    }

    /**
     * Ramène une action à un état sûr si sa configuration n'a pas été sauvée.
     */
    virtual void onMenuActionSaveFailed(
        MenuBuilder::ActionId actionId)
    {
        (void)actionId;
    }

    /** Consomme une demande de sauvegarde produite par le contrôle. */
    virtual bool takeConfigurationSaveRequest()
    {
        return false;
    }

    ParameterList& getParameters()
    {
        return parameterList;
    }

    bool buildMenu(
        MenuBuilder& menu) const;

    uint32_t menuTimeoutMs() const
    {
        return
            menuSettings.inactivityTimeoutSeconds *
            1000UL;
    }

    uint32_t measurementTimeoutMs() const
    {
        return
            safetySettings.measurementTimeoutSeconds *
            1000UL;
    }

    /**
     * Consigne réglable à l'encodeur depuis l'écran d'accueil, ou nullptr.
     * Ses limites, son pas et ses décimales sont ceux du menu.
     */
    const Parameter* homeSetpoint() const
    {
        return homeSetpointParameter;
    }

    /**
     * Cœur contrôle, sous le mutex : applique une consigne réglée depuis
     * l'accueil, ramenée dans les limites et sur la grille du pas.
     */
    bool applyHomeSetpoint(double_t value);

    /** Cause fournie par fail() lors d'un begin() refusé, sinon nullptr. */
    const char* failureReason() const
    {
        return startupFailureReason;
    }

protected:

    /**
     * Mémorise la cause d'un échec de begin() puis retourne false.
     * Usage : return fail("Relais PID non relié");
     * Le texte doit rester valide (littéral) : il est affiché à l'écran.
     */
    bool fail(const char* reason)
    {
        startupFailureReason = reason;
        return false;
    }

    /**
     * Désigne la consigne réglable depuis l'écran d'accueil, par les clés de
     * son paramètre, déjà enregistré dans parameterList (réglage décimal
     * modifiable). À appeler à la fin de begin().
     * Usage : if (!setHomeSetpoint("thermostat", "setpoint")) return fail(...);
     */
    bool setHomeSetpoint(
        const char* ownerKey,
        const char* key);

    Parameter parameterStorage[MAX_PARAMETERS];
    ParameterList parameterList;

private:
    friend class OPC;

    bool prepareParameterRegistration();
    bool completeParameterRegistration();

    const char* startupFailureReason = nullptr;
    const Parameter* homeSetpointParameter = nullptr;
};

#endif
