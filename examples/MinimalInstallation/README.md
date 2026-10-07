# Installation minimale

Cet exemple montre la plus petite installation utile : une PT100 quatre fils
sur l'entrée 1, sa résistance, sa température et un écran d'accueil. Il n'utilise
ni BMP580, ni régulateur, ni sortie.

Pour aller plus loin (régulateurs, sorties, glue, alarmes), voir le
[guide d'écriture d'un template](../../src/Templates/README.md) et les
templates de `src/Templates/`.

## Utilisation

1. Copier `MinimalInstallation.h` et `MinimalInstallation.cpp` dans `src/`.
2. Dans `src/main.cpp`, inclure `<MinimalInstallation.h>` et
   remplacer l'installation instanciée (voir `selection.cpp`) :

   ```cpp
   MinimalInstallation installation;
   OPC opc(installation);
   ```

3. Conserver le reste de `main.cpp` (`setup`, `loop`, `setup1`, `loop1`,
   interruptions et watchdog).
4. Compiler avec `pio run`.

## Identifiants

- `configurationKey()` renvoie `"minimal_installation"` : c'est la clé stockée
  dans `config.json`. Elle doit rester stable ; la changer si l'exemple est
  copié pour une autre installation.
- `name()` n'est que le libellé affiché.

Les clés passées aux `begin()` des composants identifient aussi leurs
paramètres dans `config.json` : éviter de les renommer une fois l'installation
en service.

## Vérification

L'exemple est hors de `src/` : `pio run` ne le compile pas. Les tests sur
l'hôte (`bash test/host/run_tests.sh`) le compilent et le démarrent
(`test/host/test_minimal_installation.cpp`), son code d'écran mis à part
(`#ifndef OPC_HOST_TEST`) : il suit ainsi les évolutions d'OPC.
