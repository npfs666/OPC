#ifndef INSTALLATION_H
#define INSTALLATION_H

class SensorBoard;
class ProcessControl;
class MenuBuilder;
class Storage;

class Installation
{
public:

    virtual ~Installation() = default;

    /**
     * @brief Configure le matériel utilisé par l'installation.
     *
     * Exemple :
     *  - Déclaration des RTD
     *  - Configuration des entrées analogiques
     *  - Initialisation spécifique de la carte
     */
    virtual void configureHardware(SensorBoard& board) = 0;

    /**
     * @brief Déclare les mesures, régulateurs,
     * actionneurs et sorties dans le ProcessControl.
     */
    virtual void buildProcess(ProcessControl& controller) = 0;

    /**
     * @brief Construit le menu utilisateur.
     */
    virtual void buildMenu(MenuBuilder& menu) = 0;

    /**
     * @brief Charge les paramètres sauvegardés.
     */
    virtual void load(Storage& storage) = 0;

    /**
     * @brief Sauvegarde les paramètres.
     */
    virtual void save(Storage& storage) = 0;

    /**
     * @brief Initialise les paramètres usine.
     */
    virtual void factoryReset() = 0;

    /**
     * @brief Nom de l'installation.
     */
    virtual const char* name() const = 0;
};

#endif