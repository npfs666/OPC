# OPC — Open Process Controller

OPC est un framework embarqué pour construire des régulateurs (thermostat,
régulation solaire, PID, psychromètre…) sur la carte OPC, basée sur une
Raspberry Pi Pico 2 (RP2350).

L'approche reste proche d'Arduino : vous décrivez une **installation**
(capteurs, mesures, régulation, sorties, écran d'accueil) et le framework
s'occupe du reste : acquisition, menu, sauvegarde de la configuration et
mise en sécurité des sorties.

> Projet en développement : vérifiez toujours les états sûrs et le comportement
> réel des sorties avant de piloter une installation.

## Matériel (carte v0.2)

| Fonction | Composant |
| --- | --- |
| Microcontrôleur | Raspberry Pi Pico 2 (RP2350) |
| Entrées analogiques | 3 entrées multiplexées, ADC ADS1120 (PT100, PT1000, thermocouples) |
| Entrées numériques | 2 entrées isolées ISO1212 |
| Sorties | 2 relais, 2 sorties PWM |
| Interface | Écran ST7789 240 × 240, encodeur rotatif avec clic |
| Capteurs embarqués | BMP580 (pression), DS3231 (horloge) |
| Expandeur | MCP23017 (routage des entrées) |

Le brochage complet est dans
[Pinout_v0.2.h](src/Hardware/Boards/Pinout_v0.2.h).

## Compiler et téléverser

Prérequis : [PlatformIO](https://platformio.org/). Les dépendances sont
déclarées dans [platformio.ini](platformio.ini) et installées automatiquement.

```sh
pio run                  # compiler
pio run -t upload        # téléverser (picotool)
pio device monitor       # moniteur série, 115200 bauds
```

Le projet ne contient qu'un environnement, `opc_v02`, sélectionné par défaut.

## Fonctionnement

Les données suivent une chaîne simple :

```
Sensor ──► Measurement ──► Regulator ──► Actuator ──► Output
(entrée)   (grandeur       (commande     (adaptation   (relais,
            physique)       0 à 1)        au pilotage)  PWM)
```

- **Cœur 0** : acquisition, calcul des mesures, régulation et pilotage des
  sorties (`ProcessControl`).
- **Cœur 1** : écran, encodeur et menu.

L'écran n'accède jamais directement aux objets de contrôle : il lit une copie
cohérente des mesures et sorties (`ProcessSnapshot`).

## Créer une installation

Une installation est une classe dérivée de [Installation](src/Installation.h).
Elle déclare ses composants en membres, les initialise dans `begin()` et
dessine son écran dans `printHomeScreen()`. Un exemple complet et minimal est
disponible dans [examples/MinimalInstallation](examples/MinimalInstallation).

### 1. Déclarer les composants

```cpp
#include <Installation.h>
#include <Hardware/Sensor.h>
#include <Measurements/Resistance.h>
#include <Measurements/Temperature/TemperatureRTD.h>
#include <Regulator/Thermostat.h>
#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

class MonInstallation final : public Installation
{
public:
    const char* configurationKey() const override { return "mon_installation"; }
    const char* name() const override { return "Mon installation"; }

    bool begin(SensorBoard& board, Adafruit_BMP5xx& bmp580,
               ProcessControl& process) override;

    void printHomeScreen(HomeScreenContext& context) override;

private:
    Sensor sonde;
    Resistance resistance;
    TemperatureRTD temperature;
    Thermostat thermostat;
    ActuatorOnOff commande;
    RelayOutput relais;
};
```

Les composants doivent être des **membres** (pas des variables locales de
`begin()`) : le framework conserve leurs adresses.

### 2. Les assembler dans `begin()`

```cpp
bool MonInstallation::begin(SensorBoard& board, Adafruit_BMP5xx& bmp580,
                            ProcessControl& process)
{
    (void)bmp580;

    // Entrée physique : PT100 4 fils, 16 échantillons, offset 0
    sonde.begin("sonde", "Sonde", Sensor::Type::Pt100,
                Sensor::Wiring::FourWire, 16, 0.0f);
    if (!board.addSensor(sonde))
        return fail("Sonde : entrée indisponible");

    // Mesures
    resistance.begin("Resistance", board, sonde);
    temperature.begin("Temperature", resistance);
    if (!process.add(resistance) || !process.add(temperature))
        return fail("Mesures non enregistrées");

    // Régulation
    thermostat.begin("thermostat", "Thermostat", temperature);
    if (!process.add(thermostat))
        return fail("Thermostat non enregistré");

    // Sortie : relais 1, actif à HIGH, état sûr OFF
    commande.begin("commande", "Commande", thermostat);
    relais.begin("relais", "Relais 1", Board::Rp2040::OUTPUT_1, true, false);
    if (!process.add(commande) || !process.connect(commande, relais))
        return fail("Relais non relié");

    // Paramètres exposés dans le menu, une seule fois à la fin
    board.registerParameters(parameterList);
    process.registerParameters(parameterList);

    if (parameterList.hasError())
        return fail("Paramètres invalides");

    return true;
}
```

Points à retenir :

- `fail("…")` renvoie `false` et affiche la cause sur l'écran d'erreur de
  démarrage.
- `process.connect(actionneur, sortie)` enregistre la sortie et la relie à
  l'actionneur ; il n'y a rien d'autre à appeler.
- Le premier texte passé à `begin()` est une **clé de configuration** : stable,
  en ASCII, unique. Le second est le libellé affiché, modifiable librement.
- De même, `configurationKey()` identifie l'installation dans `config.json`
  et ne doit plus changer une fois l'installation utilisée. `name()` n'est
  qu'un libellé.
- Si l'installation a besoin du BMP580, surcharger `requiresBMP580()` pour
  renvoyer `true`.

### 3. Dessiner l'écran d'accueil

`printHomeScreen()` reçoit un `HomeScreenContext` contenant l'écran
(`context.display`, API Adafruit GFX), un indicateur `fullRefresh` et le
snapshot des données :

```cpp
const MeasurementSample* s = context.snapshot.find(temperature);

if (s == nullptr || !s->valid)
    context.display.print("--.-");
else
    context.display.print(s->value, s->decimals);
```

Utilisez uniquement le snapshot pour lire les valeurs, jamais les objets
directement (ils appartiennent à l'autre cœur).

### 4. Sélectionner l'installation

Dans [main.cpp](src/Templates/main.cpp), remplacer l'installation instanciée :

```cpp
#include <MonInstallation.h>

namespace
{
    MonInstallation installation;
    OPC opc(installation);
    // ...
}
```

Le reste du fichier (`setup`, `loop`, `setup1`, `loop1`, interruptions) reste
inchangé.

## Templates fournis

Prêts à l'emploi ou à copier comme point de départ, dans
[src/Templates](src/Templates) :

| Template | Contenu |
| --- | --- |
| `ThermostatInstallation` | 1 PT100, thermostat avec hystérésis et rampe de consigne, relais 1 |
| `SolarInstallation` | 3 PT100 (capteur, haut et bas du ballon), régulateur solaire avec décharge nocturne en mode vacances (relais 1), appoint électrique en heures creuses (relais 2) |
| `PIDInstallation` | 1 PT100, PID avec rampe et autotune, relais à commande temporelle (période 10 s) |
| `ScheduleInstallation` | 2 programmes horaires hebdomadaires sur les relais 1 et 2, sans sonde ; heure et état des relais à l'accueil |

### PID et autotune

Le menu `Regulateur` du template PID contient :

- **PID** : activation, mode (`Chauffage` / `Refroidissement`), consigne,
  `Kp`, `Ti`, `Td`, limites de sortie ;
- **Rampe PID** : limitation de la vitesse de variation de la consigne
  (°C/min) ;
- **PID autotune** : paramètres de l'essai, règle de calcul des gains et
  action `Lancer autotune`.

Au démarrage avec la configuration par défaut, la régulation est arrêtée.

Le PID utilise la forme standard des régulateurs de température :
`sortie = Kp · (e + 1/Ti · ∫e dt + Td · dérivée)`.

- `Kp` : fraction de sortie par °C d'écart ;
- `Ti` : temps intégral en secondes (0 = pas d'action intégrale) ;
- `Td` : temps dérivé en secondes (0 = pas d'action dérivée). La dérivée porte
  sur la mesure et passe par un filtre de constante `Td / 10`, qui limite
  l'effet du bruit de mesure.

L'intégrale est conservée lors d'une application de réglages, d'une mesure
invalide ou d'un changement de gains : la régulation reprend sans à-coup. Elle
repart de zéro quand le PID est arrêté puis réactivé, ou quand le mode change.
La consigne est réglable de -50 à 250 °C par défaut ; une installation peut
changer cette plage avec `setSetpointLimits()`.

> Les configurations enregistrées avec l'ancienne forme `Ki` / `Kd` ne sont pas
> converties : après la mise à jour, `Ti` et `Td` valent 0. Ressaisir les gains
> ou relancer l'autotune.

Pour un **réglage manuel** : saisir les gains dans `Regulateur > PID`, passer
`Régulation active` à `Oui` et quitter le menu.

Pour un **réglage automatique** :

1. choisir le bon mode dans `Regulateur > PID` (`Chauffage` si la sortie fait
   monter la température, `Refroidissement` sinon) ;
2. régler les limites de l'essai dans `Regulateur > PID autotune` ;
3. sélectionner `Lancer autotune`.

La sortie oscille par relais, puis les gains sont calculés à partir du gain
et de la période critiques (`Ku`, `Tu`) selon la `Règle` choisie :

| Règle | Kp | Ti | Td | Comportement |
| --- | --- | --- | --- | --- |
| `Tyreus-Luyben` (défaut) | Ku / 2,2 | 2,2 Tu | Tu / 6,3 | prudent, adapté aux process thermiques lents |
| `Sans dépass.` | Ku / 5 | Tu / 2 | Tu / 3 | dépassement quasi nul |
| `Peu dépass.` | Ku / 3 | Tu / 2 | Tu / 3 | dépassement réduit |
| `Z-N classique` | 0,6 Ku | Tu / 2 | Tu / 8 | rapide, environ 25 % de dépassement |

Les gains sont copiés dans le PID et sauvegardés ; le port série affiche `Ku`,
`Tu` et les gains obtenus. Le PID reste arrêté : relire les gains puis
activer la régulation manuellement. L'essai s'interrompt et met la sortie en
sécurité en cas de mesure invalide, de dépassement des limites, de timeout ou
d'oscillations instables.

> - La règle `Z-N classique` peut être agressive : commencer avec une
>   puissance et une plage de température prudentes.
> - Ne jamais choisir `Refroidissement` avec un chauffage raccordé.
> - Un compresseur ne doit pas être piloté avec la période de 10 s de ce
>   template : il faut un actionneur imposant des temps minimaux de marche et
>   d'arrêt.
> - Les limites de mesure ne protègent que l'autotune : prévoir une sécurité
>   thermique indépendante.

### Programmation horaire

`TimeSchedule` est un régulateur dont la commande vaut 1 pendant une plage
horaire et 0 sinon, d'après l'heure du DS3231. Il se relie comme un thermostat :

```cpp
programme.begin("prog_eclairage", "Eclairage", process.clock());
commande.begin("cmd_eclairage", "Eclairage", programme);          // ActuatorOnOff
relais.begin("relais_eclairage", "Relais 1", Board::Rp2040::OUTPUT_1, true, false);

if (!process.add(programme) || !process.add(commande) ||
    !process.connect(commande, relais))
    return fail("Programmation non reliée");
```

Menu `Programmation > Eclairage` :

- **Mode** : `Auto` (suit les plages), `Marche forcée` ou `Arrêt forcé`
  (dérogation manuelle, conservée après un redémarrage) ;
- **Plage 1** à **Plage 6** : `Jours` (`Désactivée`, `Chaque jour`, `Lun-Ven`,
  `Sam-Dim` ou un jour précis), `Début` et `Fin` au format HH:MM. La molette
  règle d'abord l'heure, un clic passe aux minutes (pas de 5 min), un second
  clic valide.

La sortie est active si au moins une plage est en cours. Début inclus, fin
exclue. Une fin avant le début fait passer la plage par minuit : les jours
choisis sont ceux du début (`Lun-Ven 22:00 → 06:00` se termine samedi à
06:00). Un début égal à la fin couvre la journée entière.

L'état est recalculé en permanence à partir de l'heure courante : après une
coupure de courant ou un réglage de l'horloge, la sortie reprend directement
le bon état.

> - Si l'heure du DS3231 est perdue (pile vide), le mode `Auto` met les
>   sorties en état sûr jusqu'au réglage de l'horloge, et l'écran d'accueil
>   est remplacé par une **alerte horloge** (un clic ouvre le menu). Les modes
>   forcés restent utilisables.
> - Les plages sont en heure légale : le passage à l'heure d'été ou d'hiver
>   est automatique (voir [Horloge](#horloge)).
> - Une installation sans entrée analogique est cadencée toutes les secondes.

#### Programmer un thermostat ou un PID

Un programme peut aussi piloter la **consigne** d'un `Thermostat` ou d'un
`PID` : consigne normale pendant les plages, consigne réduite (ou arrêt) en
dehors.

```cpp
programme.begin("prog_chauffe", "Chauffe", process.clock());
thermostat.begin("thermostat", "Thermostat", temperature);
thermostat.setSchedule(programme, 16.0);   // consigne réduite par défaut : 16 °C

if (!process.add(programme) || !process.add(thermostat))
    return fail("Régulation non enregistrée");
```

`setSchedule()` s'appelle après `begin()` et avant l'enregistrement des
paramètres. Le programme est ajouté au `ProcessControl` pour apparaître dans
le menu, mais il n'est relié à aucun actionneur. Le menu du régulateur gagne :

- **Cons. réduite** : consigne appliquée hors plage ;
- **Hors plage** : `Réduite` ou `Arrêt` (sorties en état sûr, PID remis à
  zéro pour repartir sans à-coup).

La rampe de consigne, si elle est activée, adoucit les passages réduite →
normale. Les modes `Marche forcée` / `Arrêt forcé` du programme servent de
dérogation. Un autotune PID ignore le programme pendant l'essai. Sans heure
valide, la régulation s'arrête et l'alerte horloge s'affiche.

#### Régulateur solaire : mode vacances

Sans puisage, un ballon solaire surchauffe en quelques jours. En mode
vacances, le `SolarRegulator` le décharge la nuit : pendant les plages d'un
programme, la pompe fait circuler l'eau du bas du ballon dans le capteur froid,
qui rayonne la chaleur.

```cpp
nuit.begin("holiday_night", "Décharge nuit", process.clock());
solaire.setHolidaySchedule(nuit);
process.add(nuit);
```

Menu du régulateur solaire :

- **Mode vacances** : `Oui` pour autoriser la décharge nocturne ;
- **Temp. vacances** : température du bas du ballon à atteindre (50 °C par
  défaut).

La décharge démarre si le bas du ballon dépasse la température vacances de
2 K et le capteur est plus froid que lui d'au moins `Delta démarrage`. Elle
s'arrête à la température vacances ou sous `Delta arrêt`. En journée, la
charge solaire reste normale, limitée par `Temp. ballon max`.

#### Template solaire

`SolarInstallation` combine les deux usages :

- **Relais 1, pompe** : charge solaire, et décharge de 23:00 à 06:00 en mode
  vacances (`Programmation > Décharge nuit`) ;
- **Relais 2, appoint électrique** : thermostat sur le haut du ballon
  (55 °C, hystérésis 5 K), actif uniquement en heures creuses, de 22:00 à
  06:00 par défaut (`Programmation > Heures creuses`). Son état de sécurité
  est verrouillé sur arrêt.

L'écran d'accueil affiche `DECHARGE` ou `VACANCES` à la place de l'état de la
pompe, ainsi que l'état de l'appoint.

## Composants disponibles

| Catégorie | Classes |
| --- | --- |
| Entrée analogique | `Sensor` : PT100 / PT1000 (2, 3 ou 4 fils), thermocouple B, E, J, K, N, R, S, T |
| Mesures | `Resistance`, `TemperatureRTD`, `TemperatureTC`, `PressureBMP580`, `HumidityPsychrometer`, classes BME280 (`TemperatureBME`, `HumidityBME`, `PressureBME`) |
| Régulateurs | `Thermostat`, `SolarRegulator`, `PID` (+ `PIDAutoTune`, `SetpointRamp`), `TimeSchedule` (programmation horaire) |
| Actionneurs | `ActuatorOnOff`, `ActuatorPWM`, `TimeProportionalActuator` |
| Sorties | `RelayOutput`, `PWMOutput` — voir [src/Outputs/README.md](src/Outputs/README.md) |
| Entrées numériques | `DigitalInput` — voir [src/Inputs/readme.md](src/Inputs/readme.md) |

Notes sur les mesures :

- Les thermocouples utilisent les polynômes NIST, sans extrapolation hors de
  leur domaine. La compensation de soudure froide utilise la température
  interne de l'ADS1120 : sa précision dépend de l'écart de température entre
  l'ADC et le bornier.
- L'humidité psychrométrique se calcule à partir d'une température sèche,
  d'une température humide et de la pression (`Psychrometer`).

Les tailles des listes internes sont fixes (pas d'allocation dynamique) et
réglables dans [pinout.h](src/Hardware/pinout.h) : 16 mesures, 16 régulateurs,
16 actionneurs, 16 sorties, 2 entrées numériques, 128 paramètres (un
`TimeSchedule` en utilise 19).

## Utilisation

### Menu

Un clic sur l'encodeur ouvre le menu **sans interrompre** la régulation. Les
modifications sont faites sur une copie, puis validées, appliquées et
sauvegardées automatiquement à la sortie du menu ou après 10 s d'inactivité
(réglable dans `Divers > Menu`). Une simple consultation ne change rien.

Pendant l'application de modifications, les sorties sont mises en sécurité,
puis la régulation reprend dès que de nouvelles mesures valides arrivent. Une
valeur invalide reste dans le menu pour correction.

### Calibration

`Divers > Calibration` contient :

- **Zéros ADC** : correction du zéro de chaque entrée (`Mesurer N0` avec un
  shunt au connecteur, `RAZ des N0` pour tout effacer) ;
- **Profils PT100 et PT1000** : résistance de référence, valeur de l'étalon
  et action `Calibrer Rref (E1)` avec un étalon branché en 4 fils sur
  l'entrée 1 ;
- **Thermocouples** : `C.J. offset` pour corriger la soudure froide, avec
  affichage de la température de l'ADC.

Une mesure de calibration instable ou hors plage est rejetée sans écraser la
calibration existante.

### Horloge

`Divers > Horloge` affiche et règle la date et l'heure locales. `Valider`
écrit dans l'horloge ; `Quitter` abandonne les modifications.

Le DS3231 contient l'heure **UTC**. L'heure locale est calculée à chaque
lecture d'après `Divers > Fuseau horaire` :

- **Décalage UTC** : décalage de l'heure d'hiver (`1.00 h` pour la France,
  réglable au quart d'heure) ;
- **Heure d'été** : `Europe` (par défaut) ou `Aucune`. La règle européenne
  passe à l'heure d'été le dernier dimanche de mars et revient à l'heure
  d'hiver le dernier dimanche d'octobre, à 01:00 UTC.

Le changement d'heure ne réécrit donc jamais le DS3231, et il est appliqué
même si la carte était éteinte à ce moment-là. Lors du réglage manuel pendant
l'heure répétée d'octobre (02:00 à 03:00), l'heure d'été est retenue.

### Configuration et USB

La configuration est sauvegardée dans la flash (LittleFS). Branchée à un PC,
la carte apparaît comme un petit disque USB contenant une copie `config.json`,
en plus du port série. Cette copie est **en lecture seule** : la modifier
depuis le PC ne change pas la configuration.

## Sécurité

- **Timeout des mesures** : sans nouvelle acquisition pendant 30 s (réglable
  de 10 à 300 s dans `Input > Timeout mesures`), les sorties passent en état
  sûr.
- **Watchdog matériel** : surveille les deux cœurs et redémarre la carte après
  8 s de blocage.
- **Commande invalide** : un actionneur recevant une commande invalide force
  ses sorties en état sûr.

L'état sûr logique d'un relais est `OFF` : vérifier qu'il correspond aussi à
une sortie physiquement désactivée (paramètre `Actif à HIGH`).

## Organisation du code

```
src/
├── Templates/     main.cpp et installations prêtes à l'emploi
├── Installation.* classe de base d'une installation
├── OPC.*          cœur du framework (init, boucles, menu, stockage)
├── ProcessControl.*  orchestration mesures → régulation → sorties
├── Hardware/      carte de mesure, capteurs, brochage
├── Drivers/       ADS1120, DS3231
├── Measurements/  grandeurs physiques
├── Physics/       conversions PT100, thermocouples, psychrométrie
├── Regulator/     régulateurs
├── Outputs/       actionneurs et sorties
├── Inputs/        entrées numériques
└── hmi/           menu, paramètres, encodeur, affichage
```

## Licence

MIT — voir [LICENSE](LICENSE).
