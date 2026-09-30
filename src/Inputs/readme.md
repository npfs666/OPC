# Entrées numériques

`DigitalInput` lit une entrée logique, typiquement une des deux voies isolées
ISO1212 de la carte. Elle gère la polarité et un filtrage anti-rebond, sans
bloquer le programme.

| Constante | GPIO | Voie |
| --- | --- | --- |
| `Board::Rp2040::DIGITAL_INPUT_1` | 8 | ISO1212 voie 1 |
| `Board::Rp2040::DIGITAL_INPUT_2` | 9 | ISO1212 voie 2 |

Les constantes sont dans `<Hardware/pinout.h>`. Au plus `MAX_DIGITAL_INPUTS`
entrées (2 par défaut) peuvent être enregistrées.

## Ajouter une entrée à une installation

Déclarer l'entrée comme **membre** de l'installation (le framework conserve
son adresse) :

```cpp
#include <Inputs/DigitalInput.h>

class MonInstallation final : public Installation
{
    // ...
private:
    DigitalInput autorisation;
};
```

Puis l'initialiser et l'enregistrer dans `begin()` :

```cpp
autorisation.begin(
    "autorisation",                  // clé de configuration (stable)
    "Autorisation",                  // nom affiché
    Board::Rp2040::DIGITAL_INPUT_1,
    true,                            // actif quand le GPIO est HIGH
    20);                             // filtrage : 20 ms

if (!process.add(autorisation))
    return fail("Entrée autorisation non enregistrée");

// Une seule fois, après avoir enregistré tous les composants :
process.registerParameters(parameterList);
```

Les deux derniers arguments sont optionnels (par défaut : actif à HIGH, sans
filtrage). La surcharge `begin("Nom", pin)` utilise le même texte comme clé et
comme nom.

Une fois enregistrée, l'entrée est lue automatiquement à chaque tour de la
boucle de contrôle, même quand les sorties sont arrêtées. Aucun appel
supplémentaire n'est nécessaire.

## Réglages

Deux paramètres apparaissent dans le menu `Input > <nom de l'entrée>` et sont
sauvegardés avec le reste de la configuration :

| Paramètre | Champ | Rôle |
| --- | --- | --- |
| Actif à HIGH | `settings.activeHigh` | `true` : HIGH = actif ; `false` : LOW = actif |
| Filtrage | `settings.debounceMs` | 0 à 10 000 ms. Durée pendant laquelle un nouvel état doit rester stable avant d'être validé (0 = immédiat) |

Les valeurs passées à `begin()` sont les valeurs par défaut ; une configuration
sauvegardée les remplace.

## Lire l'état (cœur de contrôle)

```cpp
if (autorisation.isActive())
{
    // ...
}
```

- `isActive()` : état filtré, `false` tant que l'entrée n'est pas valide ;
- `isValid()` : `false` au démarrage tant que le premier état n'est pas stable,
  et juste après un changement de réglage ;
- `sampledAt()` : date de la dernière lecture (`millis()`).

`isValid()` indique seulement que la lecture est établie. Ce n'est pas un
diagnostic de câblage : un fil coupé est lu comme un état normal.

### Exemple : bloquer un régulateur

Dans l'`update()` d'un régulateur personnalisé qui reçoit une référence vers
l'entrée :

```cpp
if (!autorisation.isActive())
{
    invalidateCommand();   // les sorties passent en état sûr
    return;
}

// calcul habituel de la commande...
```

Une entrée seule ne coupe **aucune** sortie : c'est au régulateur ou à
l'actionneur d'utiliser son état.

Un régulateur n'est mis à jour qu'à l'arrivée d'une nouvelle mesure ADC : sa
réaction à l'entrée suit donc ce rythme. Pour réagir plus vite, placer la
logique dans l'`update(now)` d'un actionneur personnalisé, exécuté à chaque
tour de boucle. La condition doit alors être réappliquée à chaque tour : une
écriture ponctuelle sur un relais serait écrasée par l'actionneur.

## Afficher l'état (écran d'accueil)

L'écran tourne sur l'autre cœur : dans `printHomeScreen()`, lire l'entrée via
le snapshot, jamais directement l'objet `DigitalInput`.

```cpp
const DigitalInputSample* s = context.snapshot.find(autorisation);

if (s == nullptr || !s->valid)
    context.display.print("--");
else
    context.display.print(s->active ? "Actif" : "Inactif");
```

`DigitalInputSample` contient `active`, `valid` et `sampledAt`. Pour parcourir
toutes les entrées : `inputCount()` et `inputAt(index)`.

L'écran d'accueil n'est redessiné qu'à l'arrivée d'une nouvelle mesure ADC ou au
retour du menu. Un changement d'entrée seul ne provoque pas de rafraîchissement
immédiat.

## Limites

- Le filtrage repose sur les lectures successives de la boucle : une impulsion
  plus courte qu'un tour de boucle peut être manquée, et les impulsions ne sont
  pas comptées.
- La latence augmente pendant les opérations bloquantes (sauvegarde de la
  configuration, par exemple).
- Ne pas affecter le même GPIO à plusieurs composants.
