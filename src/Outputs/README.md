# Sorties PWM

`PWMOutput` est une sortie matérielle, au même titre que `RelayOutput`.
`ActuatorPWM` transmet directement la commande normalisée (0 à 1) d'un
régulateur, par exemple un PID. Une commande invalide déclenche `forceSafe()`.

Exemple à intégrer dans une installation, avec des objets membres dont la
durée de vie couvre celle de `ProcessControl` :

```cpp
ActuatorPWM actuator;
PWMOutput output;

// Après l'initialisation et l'enregistrement du régulateur :
actuator.begin("pwm_actuator", "Actionneur PWM", pid);
output.begin("pwm_output", "Sortie PWM", Board::Rp2040::OUTPUT_3);

if (!process.add(actuator) || !process.connect(actuator, output))
    return false;

process.registerParameters(parameterList);
```

Inclure `Outputs/ActuatorPWM.h` et `Outputs/PWMOutput.h`. Le framework
initialise les sorties, applique leurs réglages et gère leur repli.
Aucune installation n'instancie ces nouvelles classes par défaut.

Le menu **Sorties** expose pour chaque instance :

- **Broche** : PWM 1 (`OUTPUT_3`, GPIO 26) ou PWM 2 (`OUTPUT_4`, GPIO 27) ;
- **Actif à HIGH** : polarité de la commande ;
- **Commande de sécurité** : rapport cyclique logique de repli, de 0 à 1
  (0 par défaut).

La rubrique **PWM commun** expose une seule **Fréquence**, commune aux deux
canaux du même compteur matériel. Elle est réglable de 1 à 100 kHz, avec
1 kHz par défaut, via `PWMOutput::sharedSettings.frequency` ou le menu.
La fréquence réelle est quantifiée par le diviseur matériel (pas de 1/16).
La résolution du rapport cyclique est de 0,1 %. Les commandes 0 et 1
produisent des niveaux constants ; la polarité s'applique aussi au repli.

Les paramètres sont persistants par le mécanisme habituel. La validation
interdit d'affecter la même broche à deux sorties dans le menu. Lors d'une
construction entièrement en code, utiliser une broche distincte par sortie.
