#ifndef PARAMETER_EDITOR_H
#define PARAMETER_EDITOR_H

#include <hmi/ParameterList.h>
#include <Hardware/pinout.h>

struct ParameterDraft
{
    const Parameter* parameter = nullptr;

    bool booleanValue = false;
    int32_t integerValue = 0;
    double_t numberValue = 0.0;
    int32_t selectionValue = 0;
};

class ParameterEditor;

/** Validation croisée des brouillons (composants, installation). */
class ParameterRestoreValidator
{
public:
    virtual ~ParameterRestoreValidator() = default;

    virtual bool validateRestoredParameters(
        const ParameterEditor& editor) const = 0;
};

class ParameterEditor
{
public:
    void begin(const ParameterList& parameters);
    void capture(const char* ownerKey = nullptr);
    bool validate() const;
    bool apply();

    /**
     * Restauration tolérante, après capture() des réglages par défaut puis
     * lecture du fichier dans les brouillons. Si l'ensemble est refusé :
     *  - un réglage hors plage revient seul à sa valeur par défaut ;
     *  - puis les propriétaires sont repris un à un, et celui que la
     *    validation croisée refuse revient entièrement par défaut.
     * Les autres réglages lus sont conservés, au lieu de perdre tout le
     * fichier (calibrations comprises).
     *
     * @param resetOwners premier réglage de chaque propriétaire remis
     *        (en tout ou partie) par défaut, au plus capacity
     * @return nombre de propriétaires remis par défaut (0 : tout est gardé)
     */
    size_t keepValidDrafts(
        const ParameterRestoreValidator& validator,
        const Parameter** resetOwners,
        size_t capacity);
    bool hasChanges(const char* ownerKey = nullptr) const;

    /**
     * Vrai s'il y a des modifications et qu'elles ne portent que sur des
     * réglages de conduite (Parameter::live).
     */
    bool hasOnlyLiveChanges() const;
    // À appeler sur le cœur de contrôle avant de valider le menu.
    void refreshUnchanged();

    size_t count() const;
    ParameterDraft& get(size_t index);
    const ParameterDraft& get(size_t index) const;

    const ParameterDraft* find(
        const char* ownerKey,
        const char* parameterKey) const;

private:
    ParameterDraft drafts[MAX_PARAMETERS];
    double_t capturedValues[MAX_PARAMETERS] = {};
    size_t draftCount = 0;
    bool isChanged(size_t index) const;

    /** Brouillon lié, dans les bornes et options de son paramètre. */
    static bool isDraftValid(const ParameterDraft& draft);

    bool isValidSet(const ParameterRestoreValidator& validator) const;
};

#endif
