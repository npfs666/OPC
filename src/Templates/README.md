# Écrire un template

Ce guide s'adresse à qui écrit ou adapte un template OPC : installateur,
contributeur, ou assistant de programmation. Il donne les principes, les
règles à respecter et les pièges connus. Le [README principal](../../README.md)
détaille chaque bloc ; [examples/MinimalInstallation](../../examples/MinimalInstallation)
montre la plus petite installation possible.

## 1. Le rôle d'un template

OPC fournit des **blocs** sûrs et réglables au menu : mesures, régulateurs,
comparateurs, temporisations, alarmes, sorties. Le template est la **glue**
entre ces blocs et l'application réelle :

1. il **déclare** les blocs (en membres de la classe) ;
2. il les **relie** dans `begin()` : mesure → régulateur → actionneur →
   sortie, programme → consigne, alarme → verrouillage ;
3. seulement si les liaisons ne suffisent pas, il **combine** leurs états
   dans `processLogic()` : conditions ET / OU, priorités, cycles d'un
   équipement, action d'un équipement sur un autre ;
4. il **dessine** son écran d'accueil.

Un thermostat ou un PID n'ont besoin que des étapes 1, 2 et 4. Le
[template solaire](SolarInstallation.cpp) et la
[chambre froide](ColdRoomInstallation.cpp) montrent l'étape 3.

### Bloc ou glue ?

- Un **bloc** est une fonction générique, utile à plusieurs applications :
  thermostat, PID, comparateur, temporisation, programme horaire, alarme.
- La **glue** est ce qui n'a de sens que pour cette application : « la pompe
  tourne si le capteur est plus chaud que le ballon ET que le ballon n'est
  pas plein ».
- Une glue qui contient un seuil, une hystérésis ou une minuterie écrite à la
  main utilise sans doute mal un bloc existant. Si aucun bloc ne convient,
  l'écrire dans le template est permis ; quand le même morceau sert à
  plusieurs templates, il devient un bloc d'OPC.

## 2. Structure d'un template

Un template est une classe dérivée d'[Installation](../Installation.h), dans
un `.h` et un `.cpp`. Pour l'utiliser, l'instancier dans `src/main.cpp` :

```cpp
#include <Templates/MonInstallation.h>

namespace
{
    MonInstallation installation;
    OPC opc(installation);
    // ...
}
```

| Méthode | Obligatoire | Rôle |
|---|---|---|
| `configurationKey()` | oui | clé stable de l'installation dans `config.json` |
| `name()` | oui | libellé affiché |
| `begin(board, bmp580, process)` | oui | déclare, relie et enregistre les blocs ; `fail("cause")` en cas d'erreur |
| `printHomeScreen(context)` | oui | dessine l'écran d'accueil, depuis le snapshot seulement |
| `captureHomeScreenState()` | non | copie l'état propre à l'écran (cœur contrôle, sous verrou) |
| `processLogic(now)` | non | la glue, à chaque cycle de mesure |
| `resumeLogic(now)` | non | reprise après une pause (menu, timeout) |
| `addMenuActions()` / `executeMenuAction()` | non | commandes du menu (« Lancer autotune »...) |
| `validateParameters(editor)` | non | refuse une combinaison de réglages incohérente |
| `requiresBMP580()` | non | `true` si l'installation utilise le BMP580 |

### `begin()` pas à pas

1. Sondes : `sensor.begin(...)`, puis `board.addSensor(sensor)`.
2. Mesures : `resistance.begin(...)`, `temperature.begin(...)`, puis
   `process.add(...)`. Entrées TOR : `input.begin(...)`, `process.add(input)`.
3. Régulateurs et blocs : `begin(...)`, réglages par défaut dans `settings`,
   puis `process.add(...)`.
4. Sorties : `actionneur.begin(..., regulateur)`, `sortie.begin(...)`, puis
   `process.add(actionneur)` et `process.connect(actionneur, sortie)`.
5. Alarmes : `begin(...)`, puis `process.add(alarme)`.
6. Paramètres, une seule fois à la fin :
   `board.registerParameters(parameterList)`, réglages propres au template
   (`parameterList.forOwner(...)`), puis
   `process.registerParameters(parameterList)`.
7. Si besoin : `setHomeSetpoint("cle_thermostat", "setpoint")`.

Vérifier chaque retour et terminer par `return fail("cause")` en cas
d'échec : la cause s'affiche sur l'écran d'erreur de démarrage.

## 3. Règles

### Sécurité

- **Ne jamais piloter une sortie directement.** Une sortie de la glue passe
  par une `LogicCommand`, reliée à un actionneur comme un régulateur. L'état
  sûr, les temps mini des relais, le timeout de mesure et les compteurs
  restent ainsi actifs.
- **Écrire chaque `LogicCommand` à chaque cycle** (`setOn()`, `set()` ou
  `invalidate()`). Une commande non écrite passe en état sûr, et le journal
  note `<nom> : non écrite`. Même règle pour une `ConditionAlarm` écrite par
  la glue (`set()`), qui se déclenche alors.
- **Déclarer les dépendances** : `commande.dependsOn(mesure)` (mesure,
  entrée TOR ou régulateur, 4 au plus). Une dépendance en défaut met la
  sortie en état sûr, sans avoir à tester `isValid()` dans la glue.
- **Verrouiller ce qui ne doit jamais changer**, pour une résistance
  chauffante par exemple :
  - `commande.disableManualMode()` : pas de marche forcée au menu ;
  - `commande.lockFaultAction(Regulator::FaultAction::SafeState)` : pas de
    maintien sur défaut ;
  - `relais.lockSafeState(false)` ou `sortiePWM.lockSafeCommand(0.0)` : état
    sûr à l'arrêt, non modifiable.
- **Une alarme n'est inhibable que si le template le déclare**
  (`alarme.allowInhibit()`). Ne pas déclarer inhibable une alarme qui
  verrouille une sortie.

### Glue

- Elle tourne sur le cœur 0, sous verrou, à chaque cycle de mesure : **non
  bloquante**. Pas de `delay()`, d'attente active, d'écriture de fichier ni
  de longues sorties série. Les durées passent par une `DelayTimer`, jamais
  par `delay()` ni un calcul manuel sur `millis()`.
- Elle est appelée après les régulateurs et les alarmes, avant les
  actionneurs : elle lit leur état à jour, et ses commandes servent dès ce
  cycle.
- Elle n'est pas appelée pendant une pause du menu ni après un timeout de
  mesure. Le temps continue : une `DelayTimer` ne repart pas de zéro.
- Pour arrêter un régulateur sans toucher à ses réglages : `inhibit(true)`.
  Sa commande vaut 0 dès ce cycle ; le mode manuel passe avant.

### Réglages

- **Tout ce qui varie d'un site à l'autre est un réglage**, pas une
  constante : seuils (`Comparator`), durées (`DelayTimer`), options
  (`parameterList.forOwner(...).addBool(...)`). L'installateur règle sur
  place au menu, et un firmware compilé reste adaptable.
- **Clés stables** : le premier texte de chaque `begin()` et
  `configurationKey()` identifient les réglages dans `config.json`. Les
  renommer perd les réglages sauvegardés. Les libellés (second texte) sont
  libres. Préfixer les clés par celle du template (`cold_room_...`).
- Réglages de conduite (consigne, commande manuelle) : ils sont appliqués
  sans arrêter la régulation (`ParameterList::setLive()`).
- Sous-menus : `comparateur.setMenuParent("cle")` ou
  `temporisation.setMenuParent("cle")` range un bloc dans le menu d'un autre
  propriétaire, **enregistré avant lui** (bloc ajouté avant au `process`, ou
  réglage du template enregistré avant `process.registerParameters()`).
- Actions de menu : identifiants **1 à 31** (32 à 37, 48 à 64 et 70 sont
  réservés).

### Écran d'accueil

- Lire **uniquement le snapshot** (`context.snapshot.find(...)`) et la copie
  faite par `captureHomeScreenState()`. Les objets de contrôle appartiennent
  à l'autre cœur.
- Mettre le code de dessin sous `#ifndef OPC_HOST_TEST` (inclusions et
  corps de `printHomeScreen()`) : le template reste testable sur l'hôte.
- `TextField`, `MeasurementDisplay` et `AlarmDisplay` (dans `src/hmi/`)
  évitent de réécrire les champs à largeur fixe, les mesures en défaut et le
  bandeau d'alarmes. Chaque template affiche lui-même l'état de sa glue
  (`DEGIVRAGE`, `DECHARGE`...).

### Capacités

16 mesures, 16 régulateurs (blocs compris), 16 actionneurs, 16 sorties,
2 entrées TOR, 8 alarmes et 192 réglages ([pinout.h](../Hardware/pinout.h)).
Un `TimeSchedule` utilise 19 réglages, une `LimitAlarm` 8, une
`ConditionAlarm` 3.

## 4. Les blocs de la glue

| Besoin | Bloc | Lecture / écriture dans la glue |
|---|---|---|
| Piloter une sortie | `LogicCommand` | `setOn()`, `set()`, `invalidate()` |
| Seuil ou écart entre deux mesures | `Comparator` | `isOn()` |
| Retard, post-circulation, intervalle | `DelayTimer` | `run(entrée, now)`, `remainingMs()` |
| Plage horaire | `TimeSchedule` | `isActive(actif)` |
| Arrêter un régulateur | tout régulateur | `inhibit(true / false)` |
| Masquer une alarme | alarme + `allowInhibit()` | `inhibit(true / false)` |
| Signaler une condition | `ConditionAlarm` | `set(condition)` |
| Lire une commande | tout régulateur | `isCommandValid()`, `readCommand()` |
| Lire une mesure, une entrée | `Measurement`, `DigitalInput` | `isValid()`, `getValue()`, `isActive()` |

Un `Comparator` ou une `DelayTimer` (`setSource()`) peuvent aussi être reliés
directement à un actionneur dans `begin()`, sans glue : hors-gel,
post-circulation.

## 5. Pièges connus

- **Alarmes avant la glue.** Les alarmes sont évaluées avant la glue : une
  alarme qu'elle inhibe l'est au cycle suivant. Pour qu'elle le soit dès le
  démarrage, poser l'inhibition initiale dans `begin()`
  (`ColdRoomInstallation`).
- **Une temporisation se lit à chaque cycle.** `run()` doit être appelé à
  chaque cycle avec son entrée, même fausse, pour se remettre à zéro.
  Calculer toutes les temporisations en tête de la glue, puis décider.
- **Changement d'étape.** Une temporisation pilotée par l'étape d'un cycle
  démarre au cycle qui suit le changement d'étape.
- **Mode manuel.** Il passe avant l'inhibition et ignore les défauts de
  sonde : un compresseur forcé en `Marche` tourne même pendant un dégivrage.
- **Temps minimaux des relais.** Un relais peut attendre son arrêt mini
  avant de repartir : l'écran affiche `ATTENTE`.

## 6. Exemple court : cuve d'eau chaude d'un brasseur

Deux cuves, chacune avec son thermostat. La cuve 1 chauffe la nuit, en heures
creuses (contact du compteur sur l'entrée TOR 1), si l'opérateur a armé la
chauffe la veille. L'abonnement électrique ne permet pas de chauffer les deux
cuves à la fois : la cuve 2 attend.

```cpp
// begin() : thermostats, relais, entrée heures creuses, puis le réglage
// d'armement, enregistré comme réglage de conduite.
auto options = parameterList.forOwner({
    "regulators", "Regulateur", "brew_options", "Brassage"});
options.addBool("night_heating", "Chauffe nuit", nightHeating);
parameterList.setLive("brew_options", "night_heating");

// Glue
void BrewInstallation::processLogic(uint32_t now)
{
    (void)now;

    // Cuve 1 : seulement en heures creuses, et si la chauffe est armée.
    tank1.inhibit(!(nightHeating && offPeak.isActive()));

    // Délestage : la cuve 2 attend que la cuve 1 ne chauffe plus.
    const bool tank1Heating =
        tank1.isCommandValid() && tank1.readCommand() >= 0.5;

    tank2.inhibit(tank1Heating);
}
```

Tout le reste (sondes, consignes, hystérésis, sécurités) vient des blocs.

## 7. Tester un template

Un template complet se teste sur l'hôte avec
[TemplateBench](../../test/host/TemplateBench.h) : il est démarré comme par
OPC, ses sondes sont simulées par leur résistance, les cycles de 1 s font
avancer `millis()`, et ses réglages se modifient comme au menu.

```cpp
struct MonBanc : TemplateBench<MonInstallation> {};

MonBanc banc;
banc.setTemperature(0, 6.0);          // sonde 0 (ordre d'addSensor())
banc.setBool("mon_option", "enabled", true);
banc.advance(600);                    // 10 min de cycles
CHECK_TRUE(FakeDigitalIO::levels[Board::Rp2040::OUTPUT_1] == HIGH);
```

Voir `test/host/test_solar.cpp`, `test/host/test_cold_room.cpp` et, pour
l'exemple minimal, `test/host/test_minimal_installation.cpp`
(`banc.measurement(i)` lit une mesure comme l'écran). Ajouter
le fichier de test et le `.cpp` du template à `test/host/run_tests.sh`, puis
compiler aussi le firmware (`pio run`) pour vérifier l'écran.

## 8. Avant de partager un template

- [ ] `pio run` et `bash test/host/run_tests.sh` passent.
- [ ] Les clés sont stables et préfixées ; aucune n'entre en conflit.
- [ ] Chaque `LogicCommand` est écrite à chaque cycle et déclare ses
      dépendances.
- [ ] Les sorties dangereuses ont leur état sûr verrouillé, et pas de mode
      manuel si nécessaire.
- [ ] Aucune constante de réglage dans la glue.
- [ ] La glue est non bloquante.
- [ ] L'écran ne lit que le snapshot ; le dessin est sous
      `#ifndef OPC_HOST_TEST`.
- [ ] Le commentaire en tête du `.h` décrit le matériel (sondes, entrées,
      sorties) et le fonctionnement.
