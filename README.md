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
- `src/Templates` : installations thermostat, solaire et PID prêtes à adapter ;
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

### Installation PID avec autotune optionnel

Le template `PIDInstallation` assemble une PT100 quatre fils, un PID, un
actionneur temporel et le relais 1. Pour le sélectionner dans `src/main.cpp` :

```cpp
#include <Templates/PIDInstallation.h>

namespace
{
    PIDInstallation installation;
    OPC opc(installation);
}
```

Avec la configuration par défaut, la sortie reste arrêtée après le démarrage.
Le choix `Régulation active` est persistant : s'il a été sauvegardé à `Oui`,
le PID reprendra après le redémarrage et la réception de mesures valides. Le
menu `Regulateur` contient trois sous-menus indépendants :

- `PID` regroupe `Régulation active`, le mode, la consigne, `Kp`, `Ki`, `Kd`
  et les limites de sortie. Il permet donc un réglage entièrement manuel ;
- `Rampe PID` permet d'activer une limitation distincte de la vitesse de montée
  et de descente de la consigne, en `°C/min` ;
- `PID autotune` regroupe les niveaux de sortie d'essai, la demi-bande, les
  limites de mesure, le timeout, la période minimale, la stabilité, les cycles
  et l'action `Lancer autotune`.

Lors du passage depuis l'ancien `TunePIDInstallation`, la consigne, le mode et
les gains du PID sont conservés. Les réglages propres à l'autotune reprennent
leurs valeurs par défaut lors de cette migration, car ils sont désormais
stockés dans leur propre sous-menu.

Quand la rampe est activée, la consigne appliquée part de la première mesure
valide puis rejoint progressivement la consigne cible. Elle est figée pendant
une pause du menu ou une mesure invalide, afin de ne pas rattraper brutalement
le temps perdu. L'autotune utilise directement sa consigne cible et ignore la
rampe.

`ThermostatInstallation` expose le même composant dans
`Regulateur > Rampe thermostat`. Le thermostat applique alors son hystérésis
autour de la consigne progressive, et non autour de la cible finale.

Avant tout essai matériel, vérifier le brochage et la polarité réelle du relais
(`Actif à HIGH`) : l'état logique sûr est `OFF`, mais il doit aussi correspondre
à une sortie physiquement désactivée. Pour un réglage manuel, saisir les gains
dans `Regulateur > PID`, passer `Régulation active` à `Oui`, puis quitter le
menu. Pour un réglage automatique, choisir d'abord dans ce même menu
`Chauffage` si la sortie fait monter la température, ou `Refroidissement` si
elle la fait descendre. Régler ensuite les limites de l'essai dans
`Regulateur > PID autotune`, puis sélectionner `Lancer autotune`. Cette action
applique les valeurs éditées, ferme le menu et démarre l'essai ; elle n'est ni
un paramètre ni une valeur persistante. Si la désactivation du PID ne peut pas
être sauvegardée, l'essai est annulé et la sortie reste sûre.

La sortie oscille ensuite sans bloquer la boucle de contrôle. Ouvrir le menu
pendant l'essai suffit à l'annuler, car le framework met alors les sorties en
sécurité. Une mesure invalide, une sortie de la plage sûre, un timeout ou des
oscillations instables arrêtent également la commande. L'état sûr du relais est
verrouillé à `OFF` dans ce template.

L'écran d'accueil affiche l'état du PID ou la progression de l'autotune. Le
port série fournit aussi l'erreur éventuelle et le résultat `Ku/Tu`. En cas de
succès, `Kp`, `Ki` et `Kd` sont copiés ensemble dans le PID principal puis
sauvegardés automatiquement. Le PID reste volontairement arrêté : relire les
gains dans `Regulateur > PID`, puis passer `Régulation active` à `Oui` pour
démarrer la régulation. Si le stockage signale un échec sur le port série, les
gains restent disponibles en mémoire vive ; ouvrir puis quitter le menu permet
de tenter une nouvelle sauvegarde avant de redémarrer la carte.

L'essai par relais suit automatiquement le mode choisi et applique ensuite les
règles PID classiques de Ziegler-Nichols. Ces règles peuvent être agressives :
commencer avec une puissance et une plage de température prudentes.

Ne jamais sélectionner `Refroidissement` lorsqu'un chauffage est raccordé : la
boucle agirait dans le mauvais sens. Un compresseur ne doit pas non plus être
piloté directement avec la période de 10 s de ce template ; il nécessite un
actionneur dédié qui impose ses temps minimaux de marche et d'arrêt.

Les limites `mesure min/max` protègent l'essai d'autotune uniquement. La
régulation PID normale doit rester surveillée par une sécurité thermique
indépendante adaptée à l'installation.

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
- `Divers > Calibration` contient un profil PT100 et un profil PT1000. Chaque
  profil conserve sa résistance de référence effective, la valeur de l'étalon,
  et la température de calibration. Le sous-menu commun `Zeros ADC` conserve
  un zéro propre à chacune des trois entrées. Les actions `Mesurer N0` utilisent
  un shunt au connecteur et l'action `Remettre les N0 à zéro` efface les trois
  corrections en une fois. L'action `Calibrer Rref (E1)` utilise ensuite
  l'étalon branché en quatre fils sur l'entrée 1. Une mesure instable, saturée
  ou hors plage est rejetée sans remplacer la calibration précédente.
- Les dimensions des listes sont fixes afin d'éviter l'allocation dynamique sur
  le microcontrôleur. Leurs limites sont regroupées dans
  `src/Hardware/pinout.h`.
- Ce projet est encore en développement : vérifier les états sûrs et le
  comportement réel des sorties avant de piloter une installation.

## Licence

OPC est distribué sous licence MIT. Voir [`LICENSE`](LICENSE).
