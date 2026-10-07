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
- Pour régler une consigne à l'encodeur depuis l'écran d'accueil (voir
  [Consigne depuis l'accueil](#consigne-depuis-laccueil)), la désigner à la
  fin de `begin()` par les clés de son paramètre :

  ```cpp
  if (!setHomeSetpoint("thermostat", "setpoint"))
      return fail("Consigne d'accueil introuvable");
  ```

  Pendant le réglage, `HomeScreenContext::editingSetpoint` est vrai et
  `editedSetpoint` contient la valeur à afficher à la place de la consigne.

### 3. Ajouter de la glue (facultatif)

Les liaisons de `begin()` (mesure → régulateur → sortie, programme →
consigne, alarme → verrouillage) suffisent pour un thermostat ou un PID. Pour
ce qu'elles ne savent pas exprimer (conditions ET / OU entre équipements,
priorités, action d'un équipement sur un autre), l'installation surcharge
`processLogic()` : c'est sa **glue**, propre à l'application.

Une sortie pilotée par la glue est une `LogicCommand`
([Regulator/LogicCommand.h](src/Regulator/LogicCommand.h)), reliée à un
actionneur comme un régulateur. Exemple : un circulateur qui tourne quand le
thermostat chauffe, sauf pendant un délestage signalé sur l'entrée TOR 1.

```cpp
// Membres : DigitalInput delestage; LogicCommand circulateur;
//           ActuatorOnOff commandeCirculateur; RelayOutput relais2;

// Dans begin()
delestage.begin("delestage", "Delestage", Board::Rp2040::DIGITAL_INPUT_1);
circulateur.begin("circulateur", "Circulateur");
circulateur.dependsOn(delestage);
commandeCirculateur.begin("cde_circulateur", "Cde circulateur", circulateur);
relais2.begin("relais_2", "Relais 2", Board::Rp2040::OUTPUT_2, true, false);

if (!process.add(delestage) || !process.add(circulateur) ||
    !process.add(commandeCirculateur) ||
    !process.connect(commandeCirculateur, relais2))
    return fail("Circulateur non relié");

// Glue
void MonInstallation::processLogic(uint32_t now)
{
    (void)now;

    const bool chauffe =
        thermostat.isCommandValid() && thermostat.readCommand() > 0.5;

    circulateur.setOn(chauffe && !delestage.isActive());
}
```

Règles de la glue :

- Elle est appelée à chaque cycle de mesure, **après les régulateurs et les
  alarmes, avant les actionneurs** : elle lit leur état à jour, et ses
  commandes servent dès ce cycle. Elle n'est pas appelée pendant une pause du
  menu ni après un timeout de mesure ; `resumeLogic()` est appelée à la
  reprise, pour réarmer son état.
- Elle tourne sur le cœur 0, sous `processDataMutex` : **non bloquante**, sans
  `delay()`, écriture de fichier ni longues sorties série.
- Elle ne pilote **jamais** une sortie directement : toujours une
  `LogicCommand` reliée à un actionneur. Les sécurités des sorties restent
  ainsi actives (état sûr, temps minimaux des relais, compteurs).
- Elle écrit **chaque `LogicCommand` à chaque appel** (`setOn()`, `set()` de
  0 à 1, ou `invalidate()`). Une commande non écrite passe en état sûr, et le
  journal note `<nom> : non écrite`.
- `dependsOn()` (mesure, entrée TOR ou régulateur, 4 au plus) : si une
  dépendance est en défaut, la sortie passe en état sûr quoi que la glue
  écrive, ou en maintien selon le réglage `Si défaut` (voir
  [Repli sur défaut de sonde](#repli-sur-défaut-de-sonde)).
  `lockFaultAction()` l'impose. Pendant un défaut, une glue qui n'écrit rien
  n'est pas un oubli.
- Une `LogicCommand` a le réglage `Commande` Auto / Marche / Arrêt (voir
  [Mode manuel](#mode-manuel)). `disableManualMode()`, dans `begin()`, le
  retire pour une sortie qui ne doit jamais être forcée (résistance
  chauffante...).
- Elle peut être verrouillée par une alarme (`setInterlock()`), comme un
  régulateur.
- Elle peut **inhiber** un régulateur (`regulateur.inhibit(true)`) : arrêt
  commandé pour un dégivrage, un délestage, l'été... Voir
  [Inhibition par la glue](#inhibition-par-la-glue).
- Tout seuil ou délai qui peut varier d'un site à l'autre doit être un
  paramètre du menu, pas une constante de la glue.

Le template `TestIO` sert de banc d'essai de la glue.

#### Inhibition par la glue

`inhibit(true)` arrête un régulateur sans toucher à ses réglages : sa commande
vaut 0 et reste **valide** (arrêt commandé, pas un défaut), dès le cycle en
cours. L'inhibition n'est pas sauvegardée ; elle reste en place jusqu'au
prochain appel, **même après une pause du menu**, et la glue la réécrit à
chaque cycle. Rien n'est journalisé.

- Le **mode manuel** (et la dérogation d'un programme horaire) passe avant :
  l'opérateur garde le dernier mot. Un **verrouillage par alarme** aussi
  (état sûr).
- **Thermostat** : plus de consigne active, ce qui suspend les alarmes
  relatives à sa consigne. À la levée, départ à l'arrêt et rampe repartie de
  la mesure, comme après le menu.
- **PID** : sortie 0, autotune en cours abandonné (et refusé tant qu'il est
  inhibé). À la levée, il repart comme une réactivation : intégrale nulle,
  rampe repartie de la mesure.
- **Programme horaire** : sortie 0 quelle que soit la plage.
  `isActive()`, lu par la glue, n'est pas modifié.
- **Comparateur, temporisation** : à l'arrêt ; à la levée, ils repartent de
  zéro.
- Une **alarme de boucle ouverte** ne surveille pas un régulateur inhibé ; sa
  fenêtre repart à zéro à la levée.

L'écran d'accueil d'un template affiche lui-même l'état qui a causé
l'inhibition (`DEGIVRAGE`...).

**Alarmes.** Une alarme ne peut être inhibée que si le template l'a déclarée
inhibable dans `begin()` (`alarme.allowInhibit()`) : sans cela, `inhibit()`
est sans effet, et une glue ne peut pas faire taire une alarme de
verrouillage. Une alarme inhibée (température haute pendant un dégivrage, par
exemple) :

- ne se déclenche pas sur son seuil ou sa boucle ouverte ; son retard repart
  de zéro à la levée ;
- signale toujours un **défaut de sonde** (réglage `Sur défaut`) ;
- reste **mémorisée** si elle l'était, relais compris, jusqu'à
  l'acquittement.

### 4. Dessiner l'écran d'accueil

`printHomeScreen()` reçoit un `HomeScreenContext` contenant l'écran
(`context.display`, API Adafruit GFX), un indicateur `fullRefresh` et le
snapshot des données. `MeasurementDisplay::print()`
([hmi/MeasurementDisplay.h](src/hmi/MeasurementDisplay.h)) affiche une mesure
au curseur courant : sa valeur et son unité, ou son état en couleur si elle
est invalide (voir [Défauts de mesure](#défauts-de-mesure)).

```cpp
#include <hmi/MeasurementDisplay.h>

context.display.setCursor(8, 48);
context.display.setTextSize(3);

MeasurementDisplay::print(
    context.display,
    context.snapshot.find(temperature),
    0xFFFF,     // couleur de la valeur
    0x0000,     // fond
    7);         // place disponible : au-delà, libellé court ("RUPT")
```

`MeasurementDisplay::format()` et `MeasurementDisplay::color()` donnent le
texte et la couleur séparément, par exemple pour centrer le texte.

Utilisez uniquement le snapshot pour lire les valeurs, jamais les objets
directement (ils appartiennent à l'autre cœur).

### 5. Sélectionner l'installation

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
| `SolarInstallation` | 3 PT100 (capteur, haut et bas du ballon), pompe solaire par comparateurs et glue avec décharge nocturne en mode vacances (relais 1), appoint électrique en heures creuses (relais 2) |
| `PIDInstallation` | 1 PT100, PID avec rampe et autotune, relais à commande temporelle (période 10 s, impulsion minimale 0,5 s) |
| `ScheduleInstallation` | 2 programmes horaires hebdomadaires sur les relais 1 et 2, sans sonde ; heure et état des relais à l'accueil |
| `TestIO` | Test matériel : entrée N → relais N et PWM N (50 %, 20 kHz). Banc d'essai de la glue : la sortie 2 dépend aussi de la PT100, mode manuel sur la commande 1 seulement, action « Simuler oubli glue » |

### PID et autotune

Le menu `Regulateur` du template PID contient :

- **PID** : commande `Auto` / `Manuel` et sortie manuelle (voir
  [Mode manuel](#mode-manuel)), activation, mode (`Chauffage` /
  `Refroidissement`), consigne, `Kp`, `Ti`, `Td`, limites de sortie, repli
  sur défaut de sonde ;
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
>   template : allonger la période et régler `Marche mini` et `Arrêt mini`
>   sur son relais (voir [Outputs/README.md](src/Outputs/README.md)).
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

#### Template solaire

`SolarInstallation` est l'exemple type d'un template **blocs + glue** : des
comparateurs donnent les conditions, et quelques lignes de glue
(`processLogic()`) les combinent pour piloter la pompe.

- **Relais 1, pompe solaire** (`LogicCommand`, menu `Regulateur > Pompe
  solaire`) :
  - **charge** quand le capteur est plus chaud que le bas du ballon
    (`Charge` : marche à 8 K, arrêt à 4 K), que le capteur atteint
    `Temp. capteur min` (20 °C) et que le haut du ballon reste sous
    `Temp. ballon max` (80 °C) ;
  - **décharge nocturne en mode vacances** : sans puisage, un ballon solaire
    surchauffe en quelques jours. Pendant les plages du programme
    `Programmation > Décharge nuit` (23:00 à 06:00 par défaut), la pompe fait
    circuler l'eau du bas du ballon dans le capteur froid, qui rayonne la
    chaleur. Menu `Regulateur > Vacances` : `Mode vacances`, puis
    `Ballon décharge` (dès 52 °C, jusqu'à 50 °C en bas de ballon) et
    `Delta décharge` (bas du ballon plus chaud que le capteur : marche à 8 K,
    arrêt à 4 K). Sans heure valide, pas de décharge ; la charge continue ;
  - une sonde en défaut arrête la pompe (état sûr verrouillé). La pompe a le
    réglage `Commande` Auto / Marche / Arrêt.
- **Relais 2, appoint électrique** : thermostat sur le haut du ballon
  (55 °C, hystérésis 5 K), actif uniquement en heures creuses, de 22:00 à
  06:00 par défaut (`Programmation > Heures creuses`). Son état de sécurité
  est verrouillé sur arrêt.

La glue du template :

```cpp
const bool charging =
    chargeDelta.isOn() && !tankMaximum.isOn() && collectorMinimum.isOn();

bool night = false;
const bool nightWindow = holidaySchedule.isActive(night) && night;

discharging = !charging && holidayMode && nightWindow &&
              dischargeTank.isOn() && dischargeDelta.isOn();

pumpCommand.setOn(charging || discharging);
```

Chaque comparateur garde sa propre hystérésis : quand `Temp. ballon max`
interrompt la charge, elle reprend dès que le ballon redescend si l'écart
est encore au-dessus de `Delta arrêt`.

L'écran d'accueil affiche `DECHARGE` ou `VACANCES` à la place de l'état de la
pompe, ainsi que l'état de l'appoint.

## Composants disponibles

| Catégorie | Classes |
| --- | --- |
| Entrée analogique | `Sensor` : PT100 / PT1000 (2, 3 ou 4 fils), thermocouple B, E, J, K, N, R, S, T |
| Mesures | `Resistance`, `TemperatureRTD`, `TemperatureTC`, `PressureBMP580`, `HumidityPsychrometer`, classes BME280 (`TemperatureBME`, `HumidityBME`, `PressureBME`) |
| Régulateurs | `Thermostat`, `PID` (+ `PIDAutoTune`, `SetpointRamp`), `TimeSchedule` (programmation horaire) |
| Comparateur | `Comparator` : seuil ou différentiel à hystérésis, voir [Comparateur](#comparateur) |
| Temporisation | `DelayTimer` : retard à la montée ou à la descente, voir [Temporisation](#temporisation) |
| Glue | `LogicCommand` : sortie écrite par `processLogic()`, voir [Ajouter de la glue](#3-ajouter-de-la-glue-facultatif) |
| Alarmes | `LimitAlarm` (seuil), `LoopBreakAlarm` (boucle ouverte), `ConditionAlarm` (entrée TOR ou glue), voir [Alarmes](#alarmes) |
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
- Une PT100 ou PT1000 se mesure sur l'une des deux plages suivantes,
  choisies dans `Input > <sonde> > Plage` :

  | Plage | Gain | Étendue | Usage |
  | --- | --- | --- | --- |
  | `-200..280°C` (défaut) | 8 | -200 à environ +280 °C | Chauffage, eau, air : meilleure résolution |
  | `-200..850°C` | 4 | Toute la norme, -200 à +850 °C | Fours, fumées : pas de quantification deux fois plus grand |

  Pour changer la valeur par défaut dans une installation, après
  `begin()` : `sonde.settings.range = Sensor::Range::Extended;`. La consigne
  d'un PID est limitée à 250 °C par défaut : l'élargir avec
  `setSetpointLimits()`.
- **Filtre d'entrée** (`Input > <sonde> > Filtre`) : filtre numérique du
  2e ordre, H = 1 / (1 + τs)², de constante τ réglable de 0 à 100,0 s
  (0 par défaut, sans filtre), comme sur les régulateurs compacts. Il
  s'applique à la température calculée, à chaque cycle de mesure, et
  complète le suréchantillonnage (`Samples`), qui lisse à l'intérieur d'un
  cycle. Un défaut de sonde n'est pas retardé : le filtre repart de la
  première valeur valide. Le filtre ajoute du retard à la régulation :
  relancer l'autotune après l'avoir modifié.

Les tailles des listes internes sont fixes (pas d'allocation dynamique) et
réglables dans [pinout.h](src/Hardware/pinout.h) : 16 mesures, 16 régulateurs,
16 actionneurs, 16 sorties, 2 entrées numériques, 8 alarmes, 192 paramètres
(un `TimeSchedule` en utilise 19, une `LimitAlarm` 8, une `ConditionAlarm` 3).

### Comparateur

`Comparator` ([Regulator/Comparator.h](src/Regulator/Comparator.h)) compare
une mesure (**seuil**) ou la différence de deux mesures (**différentiel**) à
deux seuils, avec hystérésis. Sa commande vaut 1 en marche, 0 à l'arrêt :

- **Au-dessus** : marche quand la valeur atteint le seuil de marche, arrêt
  quand elle redescend au seuil d'arrêt, seuils compris ; entre les deux,
  l'état est conservé. **En dessous** : l'inverse.
- `useSingleThreshold()` : un seul réglage, sans hystérésis (marche au-delà
  du seuil, seuil compris).
- Une mesure en défaut rend la commande invalide. Au retour de la mesure, ou
  après une reprise du menu, le comparateur repart de l'arrêt.
- Réglages dans `Regulateur > <nom>` : `Seuil marche` et `Seuil arrêt`
  (libellés avec `setLabels()`, bornes et pas avec `setRange()`). Un écart de
  °C se règle en K. Un seuil d'arrêt du mauvais côté du seuil de marche est
  refusé.

On le relie directement à un actionneur, ou on le lit depuis la glue
(`isOn()`, voir [Ajouter de la glue](#3-ajouter-de-la-glue-facultatif)) :

```cpp
// Hors-gel sans glue : relais en marche à 5 °C, à l'arrêt à 7 °C.
horsGel.begin("hors_gel", "Hors-gel", temperature,
              Comparator::Direction::Below, 5.0, 7.0);
commandeHorsGel.begin("cde_hors_gel", "Cde hors-gel", horsGel);

// Différentiel : capteur - bas du ballon, marche à 8 K, arrêt à 4 K.
charge.begin("charge", "Charge", capteur, basBallon,
             Comparator::Direction::Above, 8.0, 4.0);
```

### Temporisation

`DelayTimer` ([Regulator/DelayTimer.h](src/Regulator/DelayTimer.h)) retarde
un changement d'état. Sa commande vaut 1 quand la sortie est active :

- **Retard à la montée** (`Mode::OnDelay`) : sortie active quand l'entrée est
  vraie sans interruption depuis le délai. Une entrée fausse coupe la sortie
  et remet le délai à zéro.
- **Retard à la descente** (`Mode::OffDelay`) : sortie active dès que
  l'entrée est vraie, et encore pendant le délai après sa retombée
  (post-circulation).
- Le délai se règle dans `Regulateur > <nom>`, en secondes, minutes ou heures
  selon l'unité choisie par le template (`Unit`). Maximum par défaut :
  3600 s, 1440 min ou 48 h (`setMaximum()`) ; libellé avec `setLabel()`.
- Une pause (menu, timeout de mesure) **ne remet pas le délai à zéro** : le
  temps continue de compter, et un réglage au menu ne repousse pas une
  échéance de plusieurs heures. Le débordement de `millis()` est sans effet.

Deux usages :

```cpp
// Relié à un régulateur dans begin(), sans glue : circulateur qui tourne
// encore 3 min après l'arrêt du chauffage.
postCirculation.begin("post_circ", "Post-circulation",
                      DelayTimer::Mode::OffDelay, 3, DelayTimer::Unit::Minutes);
postCirculation.setSource(thermostat);
commandeCirculateur.begin("cde_circ", "Cde circulateur", postCirculation);

// Piloté par la glue : dégivrage après 6 h de froid.
const bool degivrer = intervalle.run(etat == Etat::Froid, now);
```

Avec une source, une commande invalide de la source rend la temporisation
invalide (état sûr de ses sorties) et la remet à zéro.

## Utilisation

### Menu

Un clic sur l'encodeur ouvre le menu **sans interrompre** la régulation. Les
modifications sont faites sur une copie, puis validées, appliquées et
sauvegardées automatiquement à la sortie du menu ou après 10 s d'inactivité
(réglable dans `Divers > Menu`). Une simple consultation ne change rien.

Pendant l'application de modifications, les sorties sont mises en sécurité,
puis la régulation reprend dès que de nouvelles mesures valides arrivent. Une
valeur invalide reste dans le menu pour correction.

Exception : les **réglages de conduite** sont appliqués sans arrêter la
régulation (ni pause de l'acquisition, ni état sûr), quand ce sont les seuls
réglages modifiés : consigne et consigne réduite, `Commande` (Auto / Manuel /
Marche / Arrêt) et `Sortie man.` du PID et du thermostat. Ils sont ensuite
sauvegardés par la boucle de contrôle. Si un autre réglage est modifié en même
temps, l'application complète s'applique à l'ensemble. Une installation
marque ses propres réglages de conduite avec `parameterList.setLive(clé
propriétaire, clé)`.

### Consigne depuis l'accueil

Comme les touches ▲/▼ d'un régulateur compact, l'encodeur règle la consigne
directement depuis l'écran d'accueil (templates thermostat et PID) :

- tourner : la consigne s'affiche en jaune et change d'un pas (0,1 °C) par
  cran, dans le même sens que l'édition d'une valeur dans le menu ; une
  rotation rapide la change de dix pas (1 °C) par cran ;
- **seul un clic valide**. Sans action pendant le délai du menu
  (`Divers > Menu > Timeout`, 10 s par défaut), le réglage est abandonné et
  la consigne en place est conservée ;
- hors réglage, un clic ouvre le menu comme avant.

La consigne reste dans les limites et sur le pas du menu. Elle est appliquée
**sans arrêter la régulation** : contrairement à une validation du menu, ni
l'acquisition ni les sorties ne sont interrompues, et la rampe de consigne
s'applique si elle est active. La configuration est sauvegardée 10 s après le
dernier réglage, pour ne pas écrire la flash à chaque cran : une coupure de
courant dans ces 10 s perd le réglage.

### Mode manuel

Le PID, le thermostat et les commandes de la glue ont un réglage `Commande`,
en tête de leur menu `Regulateur` :

- **PID** : `Auto` ou `Manuel`. En manuel, la sortie vaut `Sortie man.`
  (0 à 100 %). En Auto, `Sortie man.` suit la sortie calculée : un passage en
  manuel part de la sortie du moment. Au retour en Auto, l'intégrale est
  recalculée pour que la sortie reparte de la valeur manuelle, et la rampe de
  consigne repart de la mesure : pas d'à-coup. Un autotune en cours est
  abandonné, et ne peut pas être lancé en manuel ;
- **Thermostat** : `Auto`, `Marche` ou `Arrêt`. Au retour en Auto, l'état
  forcé est conservé tant que la mesure reste dans la bande d'hystérésis ;
- **Commande de la glue** (`LogicCommand`) : `Auto`, `Marche` ou `Arrêt`. En
  manuel, ni la glue ni les dépendances n'agissent. Le template peut retirer
  ce réglage (`disableManualMode()`).

En manuel, la sortie s'applique **sans tenir compte de la mesure** : ni défaut
de sonde, ni repli, ni `Activé` du PID, ni inhibition par la glue. Les sécurités des sorties restent
actives (timeout d'acquisition, temps minimaux des relais). L'écran d'accueil
affiche `MANUEL` en orange. Passer d'Auto à Manuel, revenir, ou changer la
sortie manuelle n'interrompt pas les sorties : ce sont des réglages de
conduite (voir [Menu](#menu)).

Le mode manuel n'est **pas sauvegardé** : au démarrage, la régulation repart
toujours en Auto.

### Journal des événements

`Divers > Journal` affiche les derniers événements, du plus récent au plus
ancien, sans ordinateur : l'heure en gris, puis le texte en couleur (défaut
en rouge, alarme en orange, redémarrage en cyan). La molette fait défiler, un
clic revient au menu.

| Événement | Exemple | Conservé en flash |
| --- | --- | --- |
| Démarrage et sa cause | `Mise sous tension`, `Redémarrage (watchdog)`, `Redémarrage (baisse tension)` | oui |
| Défaut de sonde et sa fin | `Temperature : RUPTURE`, `Temperature : OK` | oui |
| Alarme | `Alarme Temp. haute : ACTIVE`, `MEMORISEE`, `FIN` | oui |
| Acquittement, remise à zéro, actions du menu | `Alarmes acquittées`, `RAZ compteurs Relais 1`, `Action : Mesurer N0 E1` | oui |
| Entretien, heure perdue ou retrouvée | `Entretien Relais 1 (150000 man.)`, `Heure perdue` | oui |
| Réglages modifiés, consigne réglée à l'accueil | `Réglages modifiés`, `Consigne : 21.5 °C` | non |

Chaque événement porte l'heure locale (`04/10 14:32:05`), ou le temps depuis
le démarrage si l'heure est inconnue (`+42 s`). Le port série affiche les
mêmes lignes au fil de l'eau.

Le journal garde les **64 derniers événements en RAM**. Un événement qui se
répète (sonde qui bagote) moins d'une minute après l'un des quatre derniers
est regroupé : `x37` à côté de l'heure au lieu de 37 lignes.

Pour ménager la flash, seuls les événements importants sont écrits dans
`/events.csv`, **par lots** : au plus une écriture toutes les 15 minutes, et
aucune pendant les 2 premières minutes après un démarrage (une boucle de
redémarrages n'use pas la flash). Une coupure de courant perd au plus les
15 dernières minutes d'événements. Le fichier est relu au démarrage.

### Compteurs d'entretien

`Divers > Compteurs` affiche, en lecture seule :

- **Heures carte** : durée de fonctionnement cumulée de la carte ;
- pour chaque relais (`Divers > Compteurs > <relais>`) : **Manœuvres**
  (basculements réels, mises en sécurité comprises) et **Heures ON** (durée
  cumulée en marche).

Chaque relais a un **Seuil entret.** en manœuvres (0 = aucun, par pas de
10 000 ; un relais standard tient environ 150 000 manœuvres à pleine charge).
Une fois le seuil atteint, l'accueil affiche `ENTRETIEN <relais>` en orange
(quand aucune alarme n'est signalée) et le journal l'indique une fois.
Après l'entretien, l'action **RAZ compteurs** du relais remet ses compteurs à
zéro, sans arrêter la régulation.

Les compteurs sont sauvegardés à part, dans `/counters.json`, toutes les
heures et après une remise à zéro, pour ménager la flash : une coupure de
courant perd au plus une heure de comptage. Ils sont relus au démarrage, par
la clé de configuration de chaque relais.

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

Les calibrations se font sur la plage `-200..280°C` et valent aussi pour la
plage `-200..850°C` : Rref ne dépend pas du gain, et N0, mesuré en points ADC,
est mis à l'échelle du gain de chaque entrée (deux fois plus de points en
3 fils, deux fois moins sur la plage large).

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

## Défauts de mesure

Chaque mesure porte un état (`getStatus()`, `MeasurementStatus`) en plus de
sa valeur. Seul l'état `Ok` rend la mesure valide ; dans tous les autres cas,
les régulateurs mettent leurs sorties en état sûr, comme avant.

| État | Écran (long / court) | Cause |
| --- | --- | --- |
| `NotReady` | `...` (gris) | Pas encore de mesure depuis le démarrage |
| `Open` | `RUPTURE` / `RUPT` (rouge) | ADC saturé vers le haut : sonde ou ligne coupée |
| `Short` | `C-CIRCUIT` / `C-C` (rouge) | Résistance < 10 Ω (PT100), < 100 Ω (PT1000) |
| `UnderRange` | `TROP BAS` / `BAS` (orange) | Sous l'étendue : RTD sous -200 °C, thermocouple sous son domaine NIST |
| `OverRange` | `TROP HAUT` / `HAUT` (orange) | Au-dessus de l'étendue : RTD au-delà de sa plage (environ 280 ou 850 °C), thermocouple au-delà de son domaine NIST |
| `Invalid` | `ERREUR` / `ERR` (rouge) | ADC saturé vers le bas (câblage inversé), jonction froide inconnue, calcul impossible |

- **Thermocouple** : la polarisation 1 MΩ (+ vers VCC, − vers GND) sature
  l'entrée quand le thermocouple est ouvert. Un court-circuit au bornier
  (température de la jonction froide affichée) et une inversion de polarité
  ne sont pas détectables.
- **RTD** : une ligne ouverte sature la source de courant, mais un
  dépassement de la plage `-200..280°C` sature aussi l'ADC. Après une
  saturation sur cette plage, une conversion supplémentaire au gain de la
  plage large (environ 16 ms) les départage : lisible, l'entrée est
  `TROP HAUT` ; encore saturée, elle est en `RUPTURE`. Sur la plage
  `-200..850°C`, une saturation est forcément une rupture.
- Les mesures calculées reprennent l'état de leur source : une température
  RTD affiche la rupture de sa résistance, une humidité psychrométrique celle
  de la sonde sèche ou humide.
- Le port série affiche l'état à la place de la valeur. Chaque changement
  d'état est inscrit au [journal](#journal-des-événements), par exemple
  `04/10 14:32:05 Temperature : RUPTURE`. Le passage `...` -> `OK` du
  démarrage n'est pas journalisé.

Pour une mesure personnalisée, appeler `setStatus()` avec la cause plutôt que
`setValid(false)`, qui donne `Invalid`.

### Repli sur défaut de sonde

Le PID et le thermostat proposent dans leur menu `Regulateur` :

- **Si défaut** : `Sécurité` (défaut) met les sorties en état sûr ;
  `Maintien` garde la commande d'avant le défaut, pour passer une coupure
  brève de la sonde (connecteur, parasite) ;
- **Maintien max** : durée du maintien, de 5 à 600 s, après laquelle les
  sorties passent en état sûr.

Le maintien ne s'applique qu'à un vrai défaut (rupture, court-circuit, hors
étendue), et seulement si la commande était valide juste avant : pas de
maintien avant la première mesure ni après une validation du menu. Pendant
un maintien, l'écran affiche `REPLI`. Le PID garde son intégrale et reprend
sans à-coup au retour de la mesure.

Il n'existe volontairement pas de sortie forcée : sans mesure, l'état sûr
reste la règle. `lockFaultAction(Regulator::FaultAction::SafeState)` retire le
choix du menu (l'appoint électrique du template solaire est verrouillé
ainsi, comme la pompe du template solaire).

## Alarmes

Une `LimitAlarm` surveille une mesure, sur le modèle des fonctions d'alarme
des régulateurs compacts. Les templates thermostat, PID et solaire en ont une,
**désactivée par défaut**, affichée seulement : menu `Alarmes > <nom>`.

| Type | Alarme quand |
| --- | --- |
| `Max` | mesure > seuil |
| `Min` | mesure < seuil |
| `Écart haut` | mesure > consigne + seuil |
| `Écart bas` | mesure < consigne − seuil |
| `Hors bande` | \|mesure − consigne\| > seuil |

Les types relatifs lisent la consigne active (rampe et programme compris) du
régulateur de référence donné par `setReference()` ; sans référence, seuls
`Max` et `Min` sont proposés. Quand ce régulateur est arrêté, l'alarme
relative est suspendue.

Réglages :

- **Seuil** et **Hystérésis** : l'alarme cesse quand la mesure revient de
  l'hystérésis en deçà du seuil ;
- **Tempo** : durée minimale au-delà du seuil avant l'alarme (0 à 3600 s) ;
- **Masquage dém.** : pour les alarmes basses (`Min`, `Écart bas`, `Hors
  bande` sous la consigne), pas d'alarme tant que la mesure n'est pas d'abord
  entrée dans la zone normale, au démarrage, à l'activation de l'alarme ou au
  redémarrage du régulateur de référence. Évite une alarme `Min` à chaque
  mise en chauffe. Un dépassement haut (`Max`, `Écart haut`) n'est jamais
  masqué ;
- **Mémorisation** : l'alarme reste signalée après la disparition de sa cause,
  jusqu'à l'acquittement ;
- **Sur défaut** : un défaut de sonde (rupture, court-circuit, hors étendue)
  déclenche l'alarme, sans masquage ni attente de la zone normale.

**Acquittement** : action `Alarmes > Acquitter` (sans état sûr ni sauvegarde),
ou entrée numérique avec `setAcknowledgeInput()` (front montant). Une alarme
acquittée pendant que sa cause dure reste signalée jusqu'à la fin de la cause,
mais n'est pas mémorisée.

**Affichage** : bandeau `ALARME <nom>` (ou `<n> ALARMES`) à l'accueil, rouge
tant qu'une cause est présente, orange quand l'alarme est seulement mémorisée.
Chaque changement est inscrit au [journal](#journal-des-événements) :
`Alarme Temp. haute : ACTIVE`, puis `MEMORISEE` ou `FIN`.

### Alarme de boucle ouverte

Une `LoopBreakAlarm` surveille la **boucle** d'un régulateur : quand sa
commande est en butée, la mesure doit se rapprocher de la consigne. Elle
détecte ce que la surveillance de sonde ne voit pas :

| Situation | Attendu | Cause probable sinon |
| --- | --- | --- |
| Chauffage à 100 % (relais collé en continu) | la mesure monte | Résistance grillée, contacteur ou SSR qui ne colle plus, sonde sortie du process |
| Chauffage à 0 %, mesure au-dessus de la consigne | la mesure baisse | Contacteur ou SSR collé, source de chaleur externe |
| Froid | sens inverse | Compresseur ou ventilateur en panne |

La surveillance s'arme quand la commande est à sa butée (`Sortie max` /
`Sortie min` du PID, ON / OFF du thermostat) et que l'écart à la consigne
dépasse `Variation min.`. Chaque rapprochement de la consigne d'au moins
`Variation min.` relance la fenêtre ; sans rapprochement pendant `Temps
détect.`, l'alarme se déclenche. Rien n'est surveillé en manuel, en
autotune, régulateur arrêté ou sur défaut de sonde.

Réglages (menu `Alarmes > Boucle`) :

| Réglage | Défaut | Rôle |
| --- | --- | --- |
| `Active` | Non | L'alarme n'a aucun effet tant qu'elle est désactivée |
| `Temps détect.` | 0 (automatique) | 0 = 2 × Ti du PID (au moins 60 s), 600 s pour un thermostat. Sinon, durée en secondes |
| `Variation min.` | 2 °C | Rapprochement attendu pendant la fenêtre, et bande autour de la consigne sans surveillance |
| `Mémorisation` | Oui | L'alarme reste signalée jusqu'à l'acquittement |
| `Mise en sécu.` | Oui | Sorties du régulateur en état sûr tant que l'alarme est signalée |

Avec `Mise en sécu.`, le régulateur est **verrouillé** jusqu'à l'acquittement
(`Alarmes > Acquitter`) : sa commande est invalide, ses sorties en état sûr,
l'écran PID affiche `SECURITE` et le PID fige son intégrale. Le verrouillage
ne s'applique qu'en régulation automatique : en manuel, l'opérateur garde la
main. Sans mémorisation, la sécurité se lève avec sa cause et la surveillance
reprend : l'alarme revient après un nouveau temps de détection si la boucle
est toujours ouverte.

L'alarme est **optionnelle** : un régulateur sans `LoopBreakAlarm` reliée
n'est jamais verrouillé. Les templates PID et thermostat en déclarent une,
désactivée par défaut :

```cpp
boucle.begin("pid_loop", "Boucle PID", temperature, pid);
if (!process.add(boucle))
    return fail("Alarme de boucle non enregistrée");
```

Un régulateur personnalisé peut être surveillé s'il fournit `readSetpoint()`,
`readOutputLimits()`, `actionDirection()`, `isAutomatic()` et, pour le temps
de détection automatique, `integralTime()`.

### Alarme sur condition

`ConditionAlarm` ([Regulator/ConditionAlarm.h](src/Regulator/ConditionAlarm.h))
signale une condition qui n'est pas un seuil de mesure. Réglages dans
`Alarmes > <nom>` : `Active`, `Tempo` (durée minimale de la condition, 0 à
3600 s) et `Mémorisation`.

- **Sur une entrée TOR**, liée dans `begin()`, sans glue : alarme quand
  l'entrée est active (la polarité se règle sur l'entrée, qui doit être
  enregistrée avec `process.add()`). Une entrée invalide est un défaut,
  signalé comme une alarme, mais pas avant sa première lecture valide
  (démarrage, retour du menu, anti-rebond).
- **Sur une condition de la glue** : `alarme.set(condition)` à chaque cycle,
  évaluée juste après la glue. Une condition non écrite est un défaut :
  alarme, et `<nom> : non écrite` dans le journal.
- Un défaut passe par la tempo, mais n'est jamais masqué par une
  [inhibition](#inhibition-par-la-glue).

```cpp
// Porte de chambre froide ouverte plus de 5 min.
porte.begin("porte", "Porte", Board::Rp2040::DIGITAL_INPUT_1);
alarmePorte.begin("alarme_porte", "Porte ouverte", porte);
alarmePorte.settings.delay = 300;
if (!process.add(porte) || !process.add(alarmePorte))
    return fail("Alarme porte non enregistrée");
```

Comme les autres alarmes, elle est **désactivée par défaut**, s'affiche dans
le bandeau d'accueil, se journalise et peut piloter un relais.

### Câbler une alarme sur un relais

La commande d'une alarme vaut 1 quand elle est signalée : elle se branche comme
un thermostat.

```cpp
alarme.begin("alarm_max", "Temp. max", temperature);
alarme.setReference(thermostat);          // optionnel : types relatifs
commandeAlarme.begin("alarm_cmd", "Alarme", alarme);       // ActuatorOnOff
relaisAlarme.begin("alarm_relay", "Relais 2", Board::Rp2040::OUTPUT_2,
                   false,   // actif à LOW : bobine collée hors alarme
                   true);   // état sûr ON = alarme

if (!process.add(alarme) || !process.add(commandeAlarme) ||
    !process.connect(commandeAlarme, relaisAlarme))
    return fail("Alarme non reliée");
```

Avec `Actif à HIGH` à non et l'état sûr à `ON`, le relais est **à sécurité
positive** : collé tant que tout va bien, il retombe en alarme, sur coupure de
courant ou sur fil coupé. L'écran affiche alors l'état logique (`ON` =
alarme). Une sortie ne peut être reliée qu'à un actionneur : une alarme par
relais.

Ajouter l'alarme au `ProcessControl` après son régulateur de référence, pour
qu'elle lise la consigne du même cycle. Une alarme ne remplace pas un
thermostat de sécurité indépendant : elle dépend de la même sonde et du même
logiciel que la régulation.

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
├── ProcessLogic.h interface de la glue (processLogic())
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
