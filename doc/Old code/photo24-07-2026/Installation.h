#ifndef INSTALLATION_H
#define INSTALLATION_H

class ProcessControl;
class MenuBuilder;
class Storage;

class Installation
{
public:

    virtual ~Installation() = default;

    /**
     * @brief Déclare toutes les mesures, régulateurs,
     * actionneurs et sorties de l'installation.
     */
    virtual void build(ProcessControl& controller) = 0;

    /**
     * @brief Construit le menu utilisateur.
     */
    virtual void buildMenu(MenuBuilder& menu) = 0;

    /**
     * @brief Charge les paramètres de l'installation.
     */
    virtual void load(Storage& storage) = 0;

    /**
     * @brief Sauvegarde les paramètres de l'installation.
     */
    virtual void save(Storage& storage) = 0;

    /**
     * @brief Réinitialise les paramètres par défaut.
     */
    virtual void factoryReset() = 0;

    /**
     * @brief Initialisation spécifique à l'installation.
     */
    virtual void setup()
    {
    }

    /**
     * @brief Traitement exécuté à chaque boucle.
     */
    virtual void loop()
    {
    }
};

#endif