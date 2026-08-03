# Installation minimale

Cet exemple montre la plus petite installation utile : une PT100 quatre fils,
sa résistance, sa température et un écran d'accueil. Il ne configure ni BME280,
ni régulateur, ni sortie.

Pour l'utiliser :

1. Copier `MinimalInstallation.h` et `MinimalInstallation.cpp` dans `src/`.
2. Ajouter `#include <MinimalInstallation.h>` dans `src/main.cpp` à la place de
   l'en-tête de l'installation courante.
3. Remplacer uniquement les déclarations globales par :

   ```cpp
   MinimalInstallation installation;
   OPC opc(installation);
   ```

4. Conserver les fonctions `setup`, `loop`, `setup1`, `loop1` et leurs ISR déjà
   présentes dans `src/main.cpp`.
5. Compiler avec `pio run -e pico`.

Le fichier `selection.cpp` contient uniquement l'extrait correspondant à
l'injection. Il sert de référence et ne remplace pas le `main.cpp` complet.

Une installation définit deux identités différentes :

```cpp
const char* configurationKey() const override;
const char* name() const override;
```

`configurationKey()` doit renvoyer une clé technique ASCII, stable et unique,
par exemple `"minimal_installation"`. Elle est enregistrée dans le champ
`installation_id` de `config.json` et ne doit pas être renommée ou traduite.
`name()` est seulement le libellé destiné à l'utilisateur et peut évoluer.

Les clés des composants passées à `begin()` identifient leurs paramètres dans
le même fichier. Une fois le projet utilisé sur une installation réelle, éviter
également de les renommer sans prévoir une migration de configuration.

Le format actuel utilise le schéma 2. Les configurations du schéma 1 sont
volontairement rejetées pendant le développement et doivent être enregistrées
à nouveau.
