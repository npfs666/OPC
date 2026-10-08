# Plan : loi d'eau et circuit de chauffage

But : un régulateur de circuit de chauffage hydraulique complet, avec la
vanne 3 points (`ThreePointActuator`) déjà en place.

```
Sonde extérieure ──► HeatingCurve ──► consigne de départ ──► PID ──► Vanne 3 points
                     (loi d'eau)  │                         ▲
                                  │          Sonde départ ──┘
                                  └──► demande de chauffe ──► post-circulation ──► Pompe
```

## Décisions

1. **La loi d'eau est un bloc `Regulator`** (`HeatingCurve`).
   - Sa **commande** est la demande de chauffe : 1 = chauffer (pompe en
     marche), 0 = arrêt commandé (été, hors plage en `Arrêt`). On peut la
     relier directement à la pompe, ou à une `DelayTimer` de
     post-circulation, sans glue.
   - Sa **consigne** (`readSetpoint()`) est la température de départ
     calculée. Faux quand la chauffe est arrêtée.
   - Commande invalide seulement si l'heure est inconnue avec un programme
     (même règle que les autres régulateurs programmés).
2. **Consigne d'exécution** : `PID::followSetpoint(source)` et
   `Thermostat::followSetpoint(source)` remplacent la consigne du menu par
   celle d'un autre régulateur, enregistré avant. Non sauvegardée, recalculée
   à chaque cycle.
   - Source sans consigne et commande valide : arrêt commandé (commande 0
     valide, la vanne se ferme).
   - Source à commande invalide : défaut, état sûr.
   - Les réglages `Consigne` et `Cons. réduite` du régulateur suiveur
     disparaissent du menu. La rampe, le mode manuel, l'autotune et les
     alarmes relatives restent.
3. **Courbe en deux points**, définie pour la consigne d'ambiance confort :
   (T. ext. froid, T. départ froid) et (T. ext. doux, T. départ doux),
   prolongée au-delà et bornée par `Départ mini` / `Départ maxi`.
4. **Consigne d'ambiance** (confort, réglage de conduite, consigne de l'écran
   d'accueil) et programme horaire optionnel (`ScheduledSetpoint` réutilisé :
   réduit ou arrêt hors plage). Un écart d'ambiance ΔTa décale la courbe de
   `(1 + pente) × ΔTa`, la pente étant celle de la courbe (modèle linéaire
   du bâtiment : Tdépart − Ta = pente × (Ta − Text)).
5. **Température extérieure filtrée** (premier ordre, `Inertie bât.` en
   heures, 0 = sans filtre) pour la courbe et l'arrêt été.
6. **Arrêt été** : au-dessus de `Arrêt été` (température extérieure filtrée),
   arrêt commandé, reprise 1 K en dessous.
7. **Hors-gel** : chauffe arrêtée (été, programme en `Arrêt`) et température
   extérieure brute sous `Hors-gel` : demande de chauffe maintenue, départ à
   `Départ mini`.
8. **Défaut de la sonde extérieure** : la courbe utilise `T. ext. secours`
   (0 °C par défaut). C'est une exception voulue à la règle « défaut =
   état sûr » : arrêter un chauffage en hiver sur une sonde extérieure
   coupée est plus dangereux (gel) que de chauffer sur une valeur moyenne.
   La sonde de départ, elle, garde la règle normale (PID en sécurité).
9. **Template `HeatingCircuitInstallation`**, sans glue :
   - sondes : départ (Pt100), extérieure (Pt1000) ;
   - vanne 3 points sur les relais 1 et 2 (Ouvrir / Fermer, état sûr :
     fermeture) ;
   - pompe sur la sortie DC 1, par un relais d'interface (sortie à
     collecteur ouvert), avec post-circulation ;
   - alarme haute du départ (plancher chauffant), à activer au menu ;
   - écran : extérieure, départ mesuré / consigne, vanne %, pompe, état.

## Étapes

1. ✅ `followSetpoint()` dans `PID` et `Thermostat`, avec tests
   (`test_heating_curve.cpp`).
2. ✅ `HeatingCurve` (`src/Regulator/HeatingCurve.{h,cpp}`), avec tests.
3. ✅ Template, avec test `TemplateBench` (`test_heating_circuit.cpp`).
4. ✅ Documentation : README (loi d'eau, consigne d'exécution, template),
   guide des templates, comparatif JUMO, plan de la logique d'installation
   (§ 6).

## Plus tard

- Influence d'une sonde d'ambiance.
- Relance après réduit (boost), optimisation de la mise en route.
- Position de la vanne dans le snapshot (aujourd'hui copiée par le template).
