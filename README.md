# OPC

OPC est un framework embarqué pour construire des régulateurs sur une carte
à RP2040. Il conserve une approche proche d'Arduino : une installation décrit
ses capteurs, ses mesures, sa régulation, ses sorties et son écran d'accueil,
tandis que le framework gère le cycle d'acquisition, le menu et la sécurité.

Le matériel actuel regroupe notamment trois entrées RTD, un ADS1120 et ses
multiplexeurs, un BME280, un écran ST7789, un encodeur rotatif et des sorties
relais. Le projet sert de base à des thermostats, régulateurs solaires, PID ou
psychromètres personnalisés.

## Architecture

Le traitement suit cette chaîne :

1. `SensorBoard` acquiert les entrées physiques.
2. Les classes `Measurement` exposent des grandeurs physiques validées.
3. Les `Regulator` calculent une commande normalisée.
4. Les `Actuator` adaptent cette commande au type de pilotage.
5. Les `Output` appliquent la commande au matériel et connaissent leur état
   sûr.
6. `ProcessControl` orchestre et surveille l'ensemble.

Une classe dérivée d'`Installation` assemble ces objets et dessine son écran
d'accueil. `OPC` reçoit cette installation par référence et s'occupe du reste :
acquisition sur le cœur de contrôle, interface sur le second cœur, menu,
configuration persistante et échanges inter-cœurs.

Une sortie se raccorde à un actionneur par une seule opération :

```cpp
process.add(heater);
process.connect(heater, relay);
```

`connect()` enregistre la sortie, l'attache à l'actionneur et refuse un
actionneur non enregistré ou une sortie déjà utilisée. Il ne faut donc pas
appeler séparément `actuator.addOutput(output)`.

Les principaux dossiers sont :

- `src/Measurements`, `src/Physics` et `src/Regulator` : mesures et calculs ;
- `src/Outputs` : actionneurs et sorties physiques ;
- `src/Hardware` et `src/Drivers` : carte de mesure et pilotes ;
- `src/hmi` : paramètres, menu, encodeur et affichage ;
- `src/Templates` : installations thermostat et solaire prêtes à adapter ;
- `examples/MinimalInstallation` : création minimale d'une installation.

## Choisir une installation

L'installation doit vivre aussi longtemps qu'`OPC`, car le framework en
conserve une référence. Elle est donc généralement créée au niveau global dans
`src/main.cpp` :

```cpp
#include <OPC.h>
#include <Templates/ThermostatInstallation.h>

namespace
{
    ThermostatInstallation installation;
    OPC opc(installation);
}
```

Pour commencer une application, copier et renommer le contenu de
[`examples/MinimalInstallation`](examples/MinimalInstallation), puis remplacer
le type de l'installation dans `src/main.cpp`. Le reste du démarrage
multicœur peut rester inchangé.

Chaque installation expose deux textes aux rôles distincts :

- `Installation::configurationKey()` renvoie l'identifiant technique persistant,
  par exemple `"solar_regulator"`. Il doit être ASCII, stable, non traduit et
  unique pour le type d'installation ;
- `Installation::name()` renvoie uniquement le libellé affiché. Il peut être
  corrigé, traduit ou renommé sans invalider la configuration sauvegardée.

Le framework utilise `configurationKey()` pour vérifier que le fichier chargé
appartient bien à l'installation sélectionnée.

## Compiler et tester

Le projet utilise PlatformIO. Depuis la racine :

```sh
pio run -e pico
```

Pour téléverser puis ouvrir le port série à 115200 bauds :

```sh
pio run -e pico -t upload
pio device monitor -b 115200
```

Les tests hôte nécessitent `g++` et ne demandent pas de carte connectée :

```sh
bash test/host/run_tests.sh
```

Ils couvrent les principaux calculs, régulateurs, paramètres et l'ordre de
traitement. L'ADS1120, les multiplexeurs, LittleFS et l'USB nécessitent encore
des tests d'intégration sur une carte réelle.

## Menu et configuration USB

Le clic sur l'encodeur ouvre le menu. Les modifications sont faites sur une
copie, puis validées, appliquées et sauvegardées à la sortie. Un paramètre
`readOnly` est seulement non éditable dans le menu : le firmware peut le
calculer et il peut être restauré depuis `config.json`.

LittleFS conserve la configuration interne. Quand la Pico est reliée à un PC,
le firmware expose aussi un petit volume USB contenant une copie stable nommée
`config.json`. Le port série USB CDC reste disponible en même temps. Cette
copie sert actuellement à consulter la configuration ; la modifier depuis le
PC ne reconfigure pas l'installation. Si le firmware sauvegarde pendant que le
volume est monté, la copie visible est rafraîchie après démontage ou reconnexion
du volume.

Le format courant est le schéma 2. Le champ `installation_id` contient la valeur
stable renvoyée par `configurationKey()`. Il n'y a volontairement aucune
migration depuis le schéma 1 pendant cette phase de développement : les
fichiers créés avant ce changement sont rejetés et la configuration doit être
enregistrée à nouveau.

## État et limites actuelles

- La cible PlatformIO configurée est la Raspberry Pi Pico RP2040 et le brochage
  correspond à la carte OPC actuelle.
- La chaîne matérielle et les templates sont actuellement centrés sur les
  PT100. Le type PT1000 reste déclaré, mais n'est pas le chemin matériel validé.
- La sortie physique implémentée est le relais tout-ou-rien. PWM et Modbus sont
  prévus comme extensions de l'abstraction `Output`.
- Les valeurs de calibration peuvent être stockées et affichées en lecture
  seule ; l'action de calibration depuis le menu reste à implémenter.
- Les dimensions des listes sont fixes afin d'éviter l'allocation dynamique sur
  le microcontrôleur. Leurs limites sont regroupées dans
  `src/Hardware/pinout.h`.
- Ce projet est encore en développement : vérifier les états sûrs et le
  comportement réel des sorties avant de piloter une installation.

## Licence

OPC est distribué sous licence MIT. Voir [`LICENSE`](LICENSE).
