# Entrées numériques

`DigitalInput` représente une entrée logique, notamment une voie de l'ISO1212.
Chaque objet lit un GPIO configuré en `INPUT`, applique la polarité choisie et
filtre éventuellement les changements d'état sans bloquer le programme.
Il est indépendant des mesures analogiques et de `SensorBoard`.

## Brochage OPC v0.2

Les constantes sont accessibles avec `#include <Hardware/pinout.h>` :

| Constante | GPIO | Fonction |
| --- | --- | --- |
| `Board::Rp2040::DIGITAL_INPUT_1` | 8 | Première voie ISO1212 |
| `Board::Rp2040::DIGITAL_INPUT_2` | 9 | Deuxième voie ISO1212 |

Elles remplacent les anciens noms `OUTPUT_5` et `OUTPUT_6`.
Les broches restent définies dans le code ; elles ne sont pas éditables dans
le menu. Ne pas affecter le même GPIO à plusieurs composants.

## Déclarer les entrées dans une installation

Inclure la classe dans le `.h` de l'installation et ajouter des membres :

```cpp
#include <Inputs/DigitalInput.h>

// Membres de la classe dérivée d'Installation :
DigitalInput autorisation;
DigitalInput niveau;
```

Ces objets doivent vivre aussi longtemps que l'installation : `ProcessControl`
conserve leurs adresses. Ne pas les déclarer comme variables locales de `begin()`.

Dans le `.cpp`, inclure `<ProcessControl.h>`, puis compléter la méthode
`Installation::begin(...)` :

```cpp
autorisation.begin(
    "autorisation", "Autorisation",
    Board::Rp2040::DIGITAL_INPUT_1);

niveau.begin(
    "niveau", "Niveau",
    Board::Rp2040::DIGITAL_INPUT_2,
    true,   // Actif quand le GPIO est HIGH
    20);    // État stable pendant 20 ms avant validation

if (!process.add(autorisation) || !process.add(niveau))
    return false;

// Après l'enregistrement de tous les composants, une seule fois :
process.registerParameters(parameterList);
```

Le premier texte est une clé de configuration stable et unique dans
l'installation ; le second est le nom affiché. La surcharge
`begin("Niveau", pin)` utilise le même texte pour ces deux rôles.

`process.add()` renvoie `false` si l'objet est déjà enregistré ou si la liste
est pleine. Sa capacité est `MAX_DIGITAL_INPUTS` (2 par défaut).
L'enregistrement seul ne remplace pas l'appel à `begin()`.

## Lire l'état dans le contrôle

Sur le cœur de contrôle, par exemple dans `update()` d'un régulateur personnalisé :

```cpp
if (!autorisation.isValid() || !autorisation.isActive())
{
    invalidateCommand(); // Méthode protégée de Regulator
    return;
}

// Calcul habituel de la commande du régulateur...
```

Le régulateur doit recevoir une référence à l'entrée de l'installation.
`isActive()` renvoie l'état logique filtré et vaut `false` lorsque l'état est
invalide. `isValid()` permet de distinguer « inactif » de « pas encore acquis ».
Cette validité décrit l'acquisition logicielle, pas un diagnostic de câblage.

`OPC::controlPoll()` acquiert les entrées à chaque passage, même lorsque les
sorties sont désactivées ou que l'acquisition analogique est suspendue.
`ProcessControl::updateMeasurementsAndRegulators()` les actualise également
avant de calculer les régulateurs. Aucun appel supplémentaire n'est requis
dans une installation utilisant la boucle OPC habituelle.

Les régulateurs existants gardent leur cadence liée aux nouvelles mesures ADC.
Une condition placée dans leur `update()` est donc évaluée à cette cadence.
Pour réagir indépendamment de l'ADC, la logique doit être exécutée dans le cycle
rapide avant le pilotage des sorties, par exemple dans `update(now)` d'un
actionneur personnalisé. Il faut maintenir cette condition à chaque cycle ;
une écriture ponctuelle sur un relais serait remplacée par l'actionneur.
L'entrée seule ne désactive automatiquement aucune sortie.

En utilisation sans `OPC`, appeler `process.pollInputs(now)` avant la logique
de contrôle à chaque tour de boucle, ou `entree.poll(now)` pour un objet isolé.
`process.poll(now)` seul pilote les actionneurs et les sorties.

## Polarité et filtrage

Les réglages peuvent être définis après `begin()`, avant la fin de
l'initialisation :

```cpp
niveau.settings.activeHigh = false;
niveau.settings.debounceMs = 50;
```

- `activeHigh = true` : HIGH signifie actif ; `false` : LOW signifie actif.
- `debounceMs = 0` : lecture immédiatement validée à chaque passage.
- `debounceMs > 0` : un nouvel état doit être observé sans changement pendant
  cette durée. Le filtre s'applique aux deux transitions.

Au démarrage, l'entrée reste invalide jusqu'à la première observation stable
pendant la durée configurée. Une fois valide, elle conserve son dernier état
validé pendant les rebonds. Une modification effective de polarité ou de durée
invalide l'état ; le filtre redémarre à la prochaine lecture. Une consultation
du menu ou l'application de réglages identiques ne le redémarre pas.

Les deux paramètres apparaissent dans `Input > <nom de l'entrée>` après
`process.registerParameters(parameterList)` : `Actif à HIGH` et `Filtrage`
(0 à 10 000 ms). Ils utilisent la sauvegarde/restauration habituelle de
l'installation. Les réglages sauvegardés remplacent les valeurs par défaut du
code, et sont pris en compte au prochain `poll()`.

Le filtrage repose sur les observations de la boucle : il ne compte pas les
impulsions et ne détecte pas un changement survenu entièrement entre deux
lectures. La latence dépend de la boucle et augmente lors d'une opération
bloquante, notamment une sauvegarde. Aucun `delay()` ni interruption GPIO
n'est ajouté.

## Afficher l'état sur le second cœur

Dans `printHomeScreen()`, utiliser exclusivement le snapshot fourni, sans lire
directement l'objet `DigitalInput` :

```cpp
#include <Adafruit_GFX.h>
#include <ProcessSnapshot.h>
#include <hmi/HomeScreen.h>

// Dans printHomeScreen(HomeScreenContext& context), après placement du curseur :
const DigitalInputSample* sample = context.snapshot.find(niveau);

if (sample == nullptr || !sample->valid)
    context.display.print("--");
else
    context.display.print(sample->active ? "Actif" : "Inactif");
```

`inputCount()` et `inputAt(index)` permettent aussi de parcourir les entrées.
`find()` renvoie `nullptr` pour un objet absent du snapshot ; `inputAt()` fait
de même pour un index hors limites.

Chaque `DigitalInputSample` contient `active`, `valid` et `sampledAt`, la date
de dernière lecture en millisecondes. Les entrées sont copiées sous le mutex
du processus et actualisées indépendamment des mesures analogiques.
`ProcessSnapshot::capturedAt()` conserve la date de capture de l'ensemble
mesures/sorties ; utiliser `sampledAt` pour dater une entrée numérique.

Le rafraîchissement de l'écran d'accueil conserve son déclenchement actuel
sur les nouvelles mesures ADC et les retours du menu. L'acquisition numérique
et son snapshot continuent entre ces rafraîchissements, mais un changement
numérique seul ne déclenche pas un nouvel affichage.

Pour les diagnostics série, `ProcessControl::print()` inclut les entrées :
`0` ou `1` lorsqu'elles sont valides, une valeur non numérique sinon.

## Vérification

Depuis la racine du projet :

```sh
bash test/host/run_tests.sh
pio run -e opc_v02
```

Les tests hôte couvrent l'initialisation, les deux polarités, les rebonds dans
les deux sens, le débordement de `millis()`, les changements de paramètres,
l'enregistrement et les snapshots. La lecture réelle des deux voies reste à
vérifier sur la carte.
