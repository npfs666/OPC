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
| `SolarInstallation` | 3 PT100 (capteur, haut et bas du ballon), régulateur solaire, relais de pompe |
| `PIDInstallation` | 1 PT100, PID avec rampe et autotune, relais à commande temporelle (période 10 s) |

### PID et autotune

Le menu `Regulateur` du template PID contient :

- **PID** : activation, mode (`Chauffage` / `Refroidissement`), consigne,
  `Kp`, `Ki`, `Kd`, limites de sortie ;
- **Rampe PID** : limitation de la vitesse de variation de la consigne
  (°C/min) ;
- **PID autotune** : paramètres de l'essai et action `Lancer autotune`.

Au démarrage avec la configuration par défaut, la régulation est arrêtée.

Pour un **réglage manuel** : saisir les gains dans `Regulateur > PID`, passer
`Régulation active` à `Oui` et quitter le menu.

Pour un **réglage automatique** :

1. choisir le bon mode dans `Regulateur > PID` (`Chauffage` si la sortie fait
   monter la température, `Refroidissement` sinon) ;
2. régler les limites de l'essai dans `Regulateur > PID autotune` ;
3. sélectionner `Lancer autotune`.

La sortie oscille par relais, puis les gains sont calculés (Ziegler-Nichols),
copiés dans le PID et sauvegardés. Le PID reste arrêté : relire les gains puis
activer la régulation manuellement. L'essai s'interrompt et met la sortie en
sécurité en cas de mesure invalide, de dépassement des limites, de timeout ou
d'oscillations instables.

> - Les règles de Ziegler-Nichols peuvent être agressives : commencer avec une
>   puissance et une plage de température prudentes.
> - Ne jamais choisir `Refroidissement` avec un chauffage raccordé.
> - Un compresseur ne doit pas être piloté avec la période de 10 s de ce
>   template : il faut un actionneur imposant des temps minimaux de marche et
>   d'arrêt.
> - Les limites de mesure ne protègent que l'autotune : prévoir une sécurité
>   thermique indépendante.

## Composants disponibles

| Catégorie | Classes |
| --- | --- |
| Entrée analogique | `Sensor` : PT100 / PT1000 (2, 3 ou 4 fils), thermocouple B, E, J, K, N, R, S, T |
| Mesures | `Resistance`, `TemperatureRTD`, `TemperatureTC`, `PressureBMP580`, `HumidityPsychrometer`, classes BME280 (`TemperatureBME`, `HumidityBME`, `PressureBME`) |
| Régulateurs | `Thermostat`, `SolarRegulator`, `PID` (+ `PIDAutoTune`, `SetpointRamp`) |
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
16 actionneurs, 16 sorties, 2 entrées numériques, 64 paramètres.

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

`Divers > Horloge` affiche et règle la date et l'heure du DS3231. `Valider`
écrit dans l'horloge ; `Quitter` abandonne les modifications.

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
