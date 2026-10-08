# Sorties et actionneurs

Le pilotage d'une sortie passe par deux objets :

```
Regulator ──► Actuator ──► Output
(commande     (adapte la    (applique la commande
 0 à 1)        commande)     au matériel)
```

- l'**actionneur** lit la commande du régulateur et la transforme selon le
  type de pilotage voulu (tout-ou-rien, temporel, proportionnel) ;
- la **sortie** applique cette commande sur une broche et connaît son
  **état sûr**.

## Classes disponibles

| Actionneur | Commande envoyée aux sorties |
| --- | --- |
| `ActuatorOnOff` | 1 si la commande du régulateur ≥ 0,5, sinon 0. Pour un thermostat, un comparateur, une temporisation, une alarme ou une commande de la glue. |
| `TimeProportionalActuator` | Allume la sortie pendant `commande × période` à chaque période. Pour un PID sur relais. |
| `ActuatorPWM` | Transmet la commande telle quelle (0 à 1). Pour un PID sur sortie PWM. |
| `ThreePointActuator` | Deux sorties : ouvrir et fermer une vanne motorisée 3 points, la commande étant la position voulue. Voir [Vanne 3 points](#vanne-3-points). |

| Sortie | Broches possibles |
| --- | --- |
| `RelayOutput` | `OUTPUT_1` (GPIO 21, Relais 1), `OUTPUT_2` (GPIO 20, Relais 2) |
| `PWMOutput` | `OUTPUT_3` (GPIO 26, PWM 1), `OUTPUT_4` (GPIO 27, PWM 2) |

Les constantes sont dans `<Hardware/pinout.h>` (`Board::Rp2040::OUTPUT_x`).
Un actionneur peut piloter jusqu'à 4 sorties (`MAX_OUTPUTS`), et le processus
jusqu'à 16 sorties au total (`MAX_REGISTERED_OUTPUTS`).

## Ajouter une sortie à une installation

Déclarer l'actionneur et la sortie comme **membres** de l'installation (le
framework conserve leurs adresses) :

```cpp
#include <Outputs/ActuatorOnOff.h>
#include <Outputs/RelayOutput.h>

class MonInstallation final : public Installation
{
    // ...
private:
    Thermostat thermostat;
    ActuatorOnOff commande;
    RelayOutput relais;
};
```

Puis, dans `begin()`, après avoir initialisé et enregistré le régulateur :

```cpp
commande.begin("commande", "Commande", thermostat);

relais.begin(
    "relais",                   // clé de configuration (stable)
    "Relais chauffage",         // nom affiché
    Board::Rp2040::OUTPUT_1,
    true,                       // actif à HIGH
    false);                     // état sûr : OFF

if (!process.add(commande) || !process.connect(commande, relais))
    return fail("Relais non relié");

// Une seule fois, après avoir enregistré tous les composants :
process.registerParameters(parameterList);
```

`process.connect()` enregistre la sortie et la relie à l'actionneur en une
seule opération. Il refuse un actionneur non enregistré et une sortie déjà
reliée.

Le framework initialise ensuite les sorties au démarrage, les place en état sûr
et les pilote à chaque tour de la boucle de contrôle. Aucun autre appel n'est
nécessaire.

### Autres combinaisons

PID sur relais, avec une période de 10 s et une impulsion minimale de 0,5 s :

```cpp
actionneur.begin("pid_actuator", "Commande PID", pid, 10000, 500);   // TimeProportionalActuator
relais.begin("pid_relay", "Relais PID", Board::Rp2040::OUTPUT_1, true, false);
```

PID sur sortie PWM :

```cpp
actionneur.begin("pwm_actuator", "Actionneur PWM", pid);         // ActuatorPWM
sortie.begin("pwm_output", "Sortie PWM", Board::Rp2040::OUTPUT_3,
             true,     // actif à HIGH
             0.0);     // commande de sécurité : 0 %
```

L'enregistrement est identique : `process.add(actionneur)` puis
`process.connect(actionneur, sortie)`.

## Réglages

Tous les réglages apparaissent dans le menu et sont sauvegardés avec la
configuration. Les valeurs passées à `begin()` sont les valeurs par défaut ; une
configuration sauvegardée les remplace.

**Menu `Sorties > <nom du relais>`** (`RelayOutput`)

| Paramètre | Champ | Rôle |
| --- | --- | --- |
| Broche | `settings.pin` | Relais 1 ou Relais 2 |
| Actif à HIGH | `settings.activeHigh` | `true` : HIGH = relais activé |
| État de sécurité | `settings.safeState` | État logique appliqué en cas de repli |
| Marche mini | `settings.minOnTime` | 0 à 1800 s par pas de 5 s (0 = sans contrainte) |
| Arrêt mini | `settings.minOffTime` | 0 à 1800 s par pas de 5 s (0 = sans contrainte) |

**Temps minimaux d'un relais.** Ils protègent un appareil sensible aux cycles
courts (compresseur, contacteur) :

- un changement demandé n'est appliqué qu'une fois écoulé le temps minimal de
  l'état en cours, compté depuis le dernier basculement réel. En attendant,
  l'écran d'accueil des templates affiche `ATTENTE` ;
- au démarrage, le relais est considéré comme venant de s'arrêter : après une
  coupure de courant, un compresseur attend `Arrêt mini` avant de repartir ;
- le passage à l'**état sûr n'attend pas** la marche mini, mais il compte
  comme un basculement : l'arrêt mini protège le redémarrage suivant. Un état
  sûr à ON est lui aussi appliqué immédiatement ;
- la validation du menu met les sorties en état sûr (voir
  [État sûr](#état-sûr)) : un compresseur s'arrête, puis repart après
  `Arrêt mini`. Ce n'est pas le cas si seuls des réglages de conduite
  (consignes, commande manuelle) ont été modifiés. Elle ne relance pas les
  chronos, sauf si la broche change.

Chaque relais compte aussi ses manœuvres et sa durée en marche, avec un seuil
d'entretien (`Divers > Compteurs`, voir le README principal). `counters()`,
`onSeconds()` et `resetCounters()` y donnent accès ; une sortie PWM n'a pas
de compteurs.

Avec un `TimeProportionalActuator`, préférer son `Impulsion mini` : les temps
minimaux du relais allongent ou suppriment des impulsions et faussent la
puissance moyenne.

**Menu `Sorties > <nom de la sortie PWM>`** (`PWMOutput`)

| Paramètre | Champ | Rôle |
| --- | --- | --- |
| Broche | `settings.pin` | PWM 1 ou PWM 2 |
| Actif à HIGH | `settings.activeHigh` | `false` inverse le rapport cyclique |
| Commande de sécurité | `settings.safeCommand` | Rapport cyclique de repli, de 0 à 1 |

**Menu `Sorties > PWM commun`**

| Paramètre | Champ | Rôle |
| --- | --- | --- |
| Fréquence | `PWMOutput::sharedSettings.frequency` | 1 à 100 kHz (1 kHz par défaut), commune aux deux sorties PWM |

Les deux sorties PWM partagent le même compteur matériel, d'où une fréquence
unique. La résolution du rapport cyclique est de 0,1 %. Les commandes 0 et 1
donnent un niveau constant, sans impulsions.

**Menu `Actionneurs > <nom de l'actionneur>`** (`TimeProportionalActuator`)

| Paramètre | Champ | Rôle |
| --- | --- | --- |
| Période | `settings.period` | 1 s à 1 h, par pas de 1 s |
| Impulsion mini | `settings.minPulse` | 0 à 60 s par pas de 0,1 s, au plus la moitié de la période (0 = sans contrainte) |

**Impulsion minimale.** Avec une période P et une impulsion minimale m, une
durée à ON `commande × P` inférieure à m est supprimée, et une coupure
`P − commande × P` inférieure à m est remplacée par une période entière à ON.
Les commandes 0 et 1 (autotune) basculent toujours immédiatement. Les
commandes inférieures à m / P ne chauffent pas : choisir une période assez
longue devant m (le template PID utilise 0,5 s sur 10 s, soit 5 %).

Le menu refuse d'affecter la même broche à deux sorties. En code, utiliser une
broche distincte par sortie.

## Vanne 3 points

Un servomoteur 3 points a un fil commun et un fil par sens : alimenter
« ouvrir » ouvre la vanne, alimenter « fermer » la ferme, ne rien alimenter la
laisse en place. Il n'y a pas de recopie de position ; les fins de course du
servomoteur coupent le moteur en butée. « 3 points » désigne la commande
électrique, pas l'hydraulique (une vanne 3 voies peut avoir un autre
servomoteur).

`ThreePointActuator` reçoit la commande du régulateur (0 à 1) comme position
voulue. Le PID, l'autotune, le mode manuel et les alarmes ne changent pas.

- **Position estimée** à partir de l'état réellement appliqué des sorties et du
  temps de course. Elle reste juste malgré les temps minimaux d'un relais et
  pendant les replis.
- **Recalage** : une commande à 0 ou 1 pousse la vanne jusqu'en butée, puis
  pendant la sur-course, et remet l'estimation exactement à 0 ou 1. Au
  démarrage, la position est inconnue : la vanne fait une course complète plus
  la sur-course vers la butée la plus proche de la commande (fermeture si la
  commande est sous 50 %), puis rejoint la commande. Une marche de repli assez
  longue (fermeture) recale aussi.
- **Zone morte** : une nouvelle marche ne part que si l'écart dépasse la zone
  morte ; elle va alors jusqu'à la position voulue. Une impulsion dure donc au
  moins `zone morte × temps de course`.
- **Inversion** : arrêt, puis `Pause inversion`, puis l'autre sens. Les deux
  sens ne sont jamais commandés ensemble.
- Un arrêt commandé (commande 0 valide : PID non activé, inhibition) **ferme**
  la vanne. Une commande invalide applique le repli (voir plus bas).

### Câblage

**Ouvrir / Fermer** : une sortie par sens.

```cpp
vanne.begin("vanne", "Vanne mélange", pid, relaisOuvrir, relaisFermer, 120);

relaisOuvrir.begin("v_open", "Vanne ouvrir", Board::Rp2040::OUTPUT_1);
relaisFermer.begin("v_close", "Vanne fermer", Board::Rp2040::OUTPUT_2,
                   true, true);     // état sûr ON : fermeture en défaut

if (!process.add(vanne) ||
    !process.connect(vanne, relaisOuvrir) ||
    !process.connect(vanne, relaisFermer))
    return fail("Vanne non reliée");
```

**Marche / Sens** (recommandé avec des relais) : un relais coupe
l'alimentation, l'autre, câblé en inverseur, choisit le sens. Les deux fils ne
peuvent **physiquement** jamais être alimentés ensemble.

```
Phase / 24 V ── COM R1 (Marche)
                NO  R1 ──── COM R2 (Sens)
                            NO  R2 ── Ouvrir
                            NC  R2 ── Fermer
```

```cpp
vanne.beginRunDirection("vanne", "Vanne mélange", pid, relaisMarche, relaisSens, 120);
```

Sens ne bascule que Marche coupée (contact sans courant), puis Marche repart
100 ms plus tard : Marche prend toute l'usure.

Dans les deux cas, relier les sorties avec `process.connect()` **après** leur
`begin()`. `process.connect()` refuse toute autre sortie que les deux passées à
`begin()`.

**Sorties DC (`OUTPUT_3`, `OUTPUT_4`)** : ce sont des sorties à collecteur
ouvert, qui commutent à la masse. Un servomoteur 3 points attend le commun en
référence (⊥ ou G0) et le +24 V sur le fil du sens : **ne jamais le brancher
directement** sur ces sorties, le commun serait inversé. Elles pilotent très
bien deux relais d'interface externes (bobine entre le +24 V et la sortie,
diode de roue libre), en Ouvrir / Fermer ou en Marche / Sens avec des relais
inverseurs ; les deux relais de la carte restent alors libres. La commande de
sécurité d'une sortie PWM reliée à la vanne doit être 0 ou 1 (le menu refuse
une autre valeur).

| Montage | Sorties de la carte | Reste libre |
| --- | --- | --- |
| Vanne sur les relais de la carte | Relais 1 + 2 | Les 2 sorties DC |
| Vanne sur 2 relais externes pilotés par les sorties DC | DC 1 + 2 | Les 2 relais |

### Repli

| Câblage | Verrouillée à OFF | Choisit le repli (`État de sécurité`) |
| --- | --- | --- |
| Ouvrir / Fermer | Ouvrir | Fermer : ON = fermeture en défaut, OFF = vanne figée |
| Marche / Sens | Sens (au repos : fermer) | Marche : ON = fermeture en défaut, OFF = vanne figée |

Les fins de course coupent le moteur : une fermeture prolongée en défaut ne
l'abîme pas. Le repli coupe toujours la sortie à l'arrêt avant de mettre
l'autre en marche (voir [État sûr](#état-sûr)), mais il ne peut pas attendre
la pause d'inversion : une vanne en ouverture repart aussitôt en fermeture.
Les moteurs de vanne le supportent en général. À la reprise, l'estimation
compte la durée du repli.

### Réglages

**Menu `Actionneurs > <nom de la vanne>`** (`ThreePointActuator`)

| Paramètre | Champ | Rôle |
| --- | --- | --- |
| Temps de course | `settings.travelTime` | 10 à 600 s par pas de 5 s : durée fermée → ouverte (fiche du servomoteur) |
| Zone morte | `settings.deadband` | 0,5 à 10 % (2 % par défaut) |
| Pause inversion | `settings.reversalPause` | 100 à 5000 ms (500 ms par défaut) |
| Sur-course | `settings.overtravel` | 0 à 100 % du temps de course (20 % par défaut) |

`position()` donne la position estimée (0 à 1) et `isPositionKnown()` indique
la fin du recalage du démarrage.

### Limites

- Laisser `Marche mini` et `Arrêt mini` à 0 sur les sorties de la vanne :
  l'estimation reste juste, mais les impulsions s'allongent et la vanne
  oscille autour de la position voulue.
- Le temps de course s'ajoute au retard du process : l'autotune donne de bons
  résultats si la course est courte devant la constante de temps. Le délai
  d'une `LoopBreakAlarm` doit dépasser le temps de course.
- Chaque impulsion compte une manœuvre de relais : la zone morte règle le
  compromis entre précision et usure (seuil d'entretien, `Divers > Compteurs`).
- L'estimation n'est recalée qu'en butée. Une vanne qui reste longtemps à mi-
  course peut dériver de quelques % ; une commande à 0 ou 1 la recale.
- La position n'est pas encore dans le snapshot : un écran d'accueil ne peut
  pas l'afficher.

## État sûr

Chaque sortie a un état de repli : `safeState` pour un relais, `safeCommand`
pour une sortie PWM. Il est appliqué automatiquement :

- au démarrage, avant la première régulation ;
- quand le régulateur n'a pas de commande valide : mesure en défaut (sauf
  maintien sur défaut), heure inconnue d'une régulation programmée,
  verrouillage par une alarme de boucle ouverte, commande de la glue non
  écrite ;
- quand aucune nouvelle mesure n'arrive avant le timeout
  (`Input > Timeout mesures`) ;
- pendant l'application de réglages modifiés dans le menu ;
- si une sortie n'est pas correctement initialisée (`isHealthy()`).

Le repli se fait en deux passes : d'abord les sorties dont l'état sûr est
l'arrêt, puis celles dont l'état sûr est la marche. Deux sorties qui ne doivent
jamais être actives ensemble (les deux sens d'une vanne) ne le sont donc pas,
même un instant.

Un **arrêt commandé** n'est pas un défaut : PID non `Activé`, régulateur
inhibé par la glue, programme hors plage réglé sur `Arrêt` donnent une
commande 0 valide. La sortie est alors à l'arrêt (0 %), pas dans son état
sûr, ce qui compte pour un relais à état sûr `ON` ou une sortie PWM à
commande de sécurité non nulle.

L'état sûr est **logique** : avec `Actif à HIGH` mal réglé, un relais « OFF »
peut être physiquement fermé. Vérifier le câblage réel avant toute mise en
service.

Pour interdire qu'un relais soit configuré avec un état sûr `ON` (chauffage,
par exemple), verrouiller cet état dans `begin()` :

```cpp
relais.lockSafeState(false);   // état sûr forcé à OFF, non modifiable dans le menu
```

De même pour une sortie PWM (une résistance par relais statique, par
exemple) :

```cpp
sortie.lockSafeCommand(0.0);   // commande de sécurité forcée à 0 %
```

**Sortie PWM en tout-ou-rien.** Reliée à un `ActuatorOnOff`, une sortie PWM
reçoit 0 ou 100 % : la broche quitte alors la PWM pour un niveau constant. Elle
pilote ainsi un relais statique comme un relais (sans temps minimaux ni
compteurs).

## Afficher l'état (écran d'accueil)

Dans `printHomeScreen()`, lire la sortie via le snapshot, jamais directement
l'objet :

```cpp
const OutputSample* s = context.snapshot.find(relais);

const bool relaisActif =
    s != nullptr && s->healthy && s->appliedCommand >= 0.5;
```

`OutputSample` contient `appliedCommand` (0 à 1, réellement appliquée),
`healthy` et `waiting` (commande retardée par un temps minimal).
`waitingToStart()` indique un relais arrêté qui attend la fin de son arrêt
minimal pour démarrer.

## Limites

- Un compresseur piloté par un `TimeProportionalActuator` doit avoir une
  période longue et des temps minimaux réglés sur son relais.
- Une sortie PWM ne peut utiliser que `OUTPUT_3` ou `OUTPUT_4`. Une autre broche
  fait échouer l'initialisation et le démarrage.
- Les sorties Modbus ne sont pas encore implémentées.
