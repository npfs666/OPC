# OPC — Open Process Controller

OPC est un framework embarqué pour construire des régulateurs sur une carte
OPC. La cible actuellement configurée est la Raspberry Pi Pico 2 (`rpipico2`,
RP2350), avec Arduino et PlatformIO. Il conserve une approche proche d'Arduino :
une installation décrit ses capteurs, ses mesures, sa régulation, ses sorties et son écran d'accueil,
tandis que le framework gère le cycle d'acquisition, le menu et la sécurité.

La révision OPC v0.2 utilise trois entrées de mesure multiplexées, un ADS1120,
un expandeur MCP23017, un écran ST7789 de 240 × 240 pixels, un encodeur rotatif
et six broches de sortie. Le firmware intègre aussi le BMP580 pour la pression
et le DS3231 pour l'horloge ; des classes de mesure BME280 restent disponibles.
Le projet sert de base à des thermostats, régulateurs solaires, PID ou
psychromètres personnalisés.

## Application actuellement sélectionnée

[`src/main.cpp`](src/main.cpp) instancie `TestInstallation`, une installation
de développement qui assemble, avec ses valeurs par défaut :

- un thermocouple de type K sur l'entrée 1 ;
- une PT100 quatre fils sur l'entrée 2 ;
- un BMP580, nécessaire au démarrage de cette installation ;
- un calcul d'humidité relative psychrométrique à partir des deux températures
  et de la pression.

Les régulateurs, actionneurs et relais de cette installation sont actuellement
commentés : elle ne pilote aucune sortie. Son écran affiche `T1`, `T2` et `HR`,
mais `T1` pointe encore vers l'ancienne mesure RTD non enregistrée et affiche
donc `--.-`. La mesure thermocouple est bien enregistrée dans le processus.

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
configuration persistante et échanges inter-cœurs. Le cœur 0 exécute
l'acquisition et le contrôle ; le cœur 1 gère l'interface et l'affichage.
Les écrans lisent une copie des mesures et sorties (`ProcessSnapshot`),
capturée sous mutex, pour éviter de lire des données en cours de modification.

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

Les templates disponibles sont `ThermostatInstallation` (une PT100 et un
relais), `SolarInstallation` (trois PT100 et un relais de pompe) et
`PIDInstallation` (une PT100, un PID et un relais à commande temporelle).

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
l'application des réglages ou une mesure invalide, afin de ne pas rattraper brutalement
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

La sortie oscille ensuite sans bloquer la boucle de contrôle. La consultation
du menu laisse l'essai continuer. L'application de réglages modifiés ou
l'exécution d'une action interrompt l'essai et met les sorties en sécurité. Une mesure invalide, une sortie de la plage sûre, un timeout ou des
oscillations instables arrêtent également la commande. L'état sûr du relais est
verrouillé à `OFF` dans ce template.

L'écran d'accueil affiche l'état du PID ou la progression de l'autotune. Le
port série fournit aussi l'erreur éventuelle et le résultat `Ku/Tu`. En cas de
succès, `Kp`, `Ki` et `Kd` sont copiés ensemble dans le PID principal puis
sauvegardés automatiquement. Le PID reste volontairement arrêté : relire les
gains dans `Regulateur > PID`, puis passer `Régulation active` à `Oui` pour
démarrer la régulation. Si le stockage signale un échec sur le port série, les
gains restent disponibles en mémoire vive ; une nouvelle modification de
paramètre suivie de la fermeture du menu permet de retenter la sauvegarde.

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

Le projet utilise le cœur Arduino Earle Philhower et la plateforme
`maxgerhardt/platform-raspberrypi`. Les dépendances sont déclarées dans
[`platformio.ini`](platformio.ini) et installées par PlatformIO. Depuis la
racine, compiler explicitement la révision actuelle :

```sh
pio run -e opc_v02
```

Pour téléverser puis ouvrir le port série à 115200 bauds :

```sh
pio run -e opc_v02 -t upload
pio device monitor -e opc_v02 -b 115200
```

Le téléversement utilise `picotool` et 1 Mio de flash est réservé au système
de fichiers. L'environnement `opc_v01` existe encore, mais son ancien brochage
à macros ne fournit pas les espaces de noms `Board::...` utilisés par le code
actuel : il nécessite une adaptation avant utilisation. L'ancien environnement
`pico`, encore cité dans le README de l'exemple minimal, n'existe plus.

Les tests hôte nécessitent `g++` et ne demandent pas de carte connectée :

```sh
bash test/host/run_tests.sh
```

Le script utilise C++17 et accepte un autre compilateur via `CXX`. Les tests
couvrent notamment les conversions PT100 et thermocouples, la compensation de
jonction froide, la psychrométrie, les mesures BME280, le DS3231, les régulateurs,
l'autotune, les paramètres, les snapshots et le watchdog. Ils utilisent des
doublures matérielles ; l'ADS1120, les multiplexeurs, LittleFS et l'USB
nécessitent encore des tests d'intégration sur une carte réelle.

## Menu et configuration USB

Le clic sur l'encodeur ouvre le menu sans interrompre l'acquisition ni la
régulation. Le cœur de contrôle prépare une copie des paramètres ; les
modifications du menu restent sur cette copie pendant la navigation. Elles
sont automatiquement validées, appliquées et sauvegardées à la sortie ou
après inactivité (10 s par défaut, réglable dans `Divers > Menu`), sans bouton
« Appliquer ». Une consultation sans modification ne provoque ni pause, ni
réinitialisation du PID, ni sauvegarde.

Lorsqu'il y a des modifications ou une action à exécuter, les sorties sont
mises en sécurité pendant l'application et la sauvegarde. L'acquisition
redémarre ensuite et la régulation reprend avec de nouvelles mesures valides.
Un brouillon invalide reste dans le menu pour correction, sans interrompre
la régulation en cours. Les valeurs calculées pendant la navigation, notamment
les gains issus de l'autotune, sont conservées si elles n'ont pas été éditées.
Une valeur explicitement éditée dans le menu prend la priorité. Un paramètre
`readOnly` est seulement non éditable dans le menu : le firmware peut le
calculer et il peut être restauré depuis `config.json`.

`Divers > Horloge` affiche la date et l'heure actuelles sur les deux premières
lignes, rafraîchies chaque seconde. Les champs suivants règlent l'année, le mois,
le jour, l'heure, les minutes et les secondes (années 2000 à 2099, format 24 h).
Le bouton `Valider`, placé avant `Quitter`, écrit les modifications ensemble
dans le DS3231 et laisse le menu ouvert. `Quitter` et l'expiration du timeout
abandonnent les modifications non validées, même si un champ est encore en édition.
Une simple consultation ne réécrit pas l'horloge. Une date impossible ou une
erreur I²C laisse le menu ouvert avec `Erreur date / RTC` et conserve la saisie
pour correction ou nouvel essai. Ces champs ne sont pas sauvegardés dans LittleFS.

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

- Le brochage actuel est défini dans
  [`Pinout_v0.2.h`](src/Hardware/Boards/Pinout_v0.2.h). L'espace de noms
  `Board::Rp2040` conserve son nom historique malgré la cible Pico 2.
- La mesure de température RTD est implémentée pour les PT100. Le routage
  matériel, la mesure de résistance et la calibration PT1000 existent, mais
  leur conversion en température reste désactivée dans `TemperatureRTD`.
- Les thermocouples B, E, J, K, N, R, S et T disposent de conversions fondées
  sur les polynômes NIST, sans extrapolation hors domaine. Le type B est limité
  à 250–1820 °C pour la température mesurée. Le routage thermocouple impose
  deux fils et adapte le gain ADC au type sélectionné.
- La compensation de jonction froide utilise la température interne de
  l'ADS1120, corrigée par `Divers > Calibration > Thermocouples > C.J. offset`.
  Ce menu affiche aussi `Temp. ADC`. La précision réelle dépend donc notamment
  de l'écart de température entre l'ADC et le raccordement du thermocouple.
- La sortie physique implémentée est le relais tout-ou-rien. PWM et Modbus sont
  prévus comme extensions de l'abstraction `Output`.
- `Divers > Calibration` contient un profil PT100 et un profil PT1000. Chaque
  profil conserve sa résistance de référence effective, la valeur de l'étalon,
  et la température de calibration. Le sous-menu commun `Zeros ADC` conserve
  un zéro propre à chacune des trois entrées. Les actions `Mesurer N0` utilisent
  un shunt au connecteur et l'action `RAZ des N0` efface les trois
  corrections en une fois. L'action `Calibrer Rref (E1)` utilise ensuite
  l'étalon branché en quatre fils sur l'entrée 1. Une mesure instable, saturée
  ou hors plage est rejetée sans remplacer la calibration précédente.
- Le coefficient thermique de calibration est enregistré, mais sa correction
  dans le calcul de résistance est actuellement commentée.
- `Input > Timeout mesures` met les sorties en sécurité en l'absence de
  nouvelles acquisitions (30 s par défaut, réglable de 10 à 300 s).
  Un watchdog matériel distinct surveille la progression des deux cœurs et
  redémarre la carte en cas de blocage ; son délai est fixé à 8 s dans
  [`SystemWatchdog.h`](src/SystemWatchdog.h).
- Les dimensions des listes sont fixes afin d'éviter l'allocation dynamique sur
  le microcontrôleur. Leurs limites sont regroupées dans
  `src/Hardware/pinout.h`.
- Ce projet est encore en développement : vérifier les états sûrs et le
  comportement réel des sorties avant de piloter une installation.

## Licence

OPC est distribué sous licence MIT. Voir [`LICENSE`](LICENSE).
