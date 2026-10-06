# Plan d'action : logique d'installation

Plan établi le 06/10/2026, état du code : commit `06009b4` (journalisation).

Légende : ✅ fait · ⬜ à faire

---

## 1. Objectif

OPC fournit des **blocs sécurisés, réglables au menu** : mesures, régulateurs,
alarmes, programmes horaires, sorties. Le template les **lie dans `begin()`**.
Seulement si c'est nécessaire, il ajoute de la **glue** dans `processLogic()`
pour les combinaisons que les liaisons ne savent pas exprimer.

Le template est la glue entre OPC et l'application réelle. Il doit être
lisible, modifiable et partageable (source + `.uf2`).

### Situation actuelle

- `begin()` décrit déjà une logique par **liaisons** : mesure → régulateur →
  sortie, programme → consigne, alarme → verrouillage. Pour un thermostat ou un
  PID, elle suffit.
- Ce qui manque :
  - aucun point d'appel pendant le cycle ;
  - aucune sortie pilotable par le template ;
  - aucun levier sur un bloc existant.
- Le seul contournement est un `Regulator` imbriqué dans l'installation
  (`TestIO::InputCommand`). Il oblige à réécrire les contrôles de validité des
  mesures et n'a ni mode manuel, ni réglages au menu.

### Rôles

- **Utilisateur** : il conduit l'installation, surtout par la consigne. Il a
  accès à tout le menu, sous sa responsabilité.
- **Installateur / auteur de template** : il adapte OPC à l'application,
  écrit ou modifie le template, puis reflashe. Il fait ses réglages fins sur
  place, au menu.
- **Distribution** : la source du template et un `.uf2` compilé. Celui qui
  flashe un `.uf2` ne modifie que les réglages du menu.

---

## 2. Principes

- **Un bloc** est une fonction générique, sécurisée, réglable au menu et
  réutilisable d'une application à l'autre : PID, thermostat, comparateur,
  temporisation, alarme, programme horaire.
- **La glue** est ce qui n'a de sens que pour l'application. Exemple : « la
  pompe tourne si le capteur est plus chaud ET que le ballon n'est pas
  plein ».
- **D'abord les blocs et les liaisons dans `begin()`**, puis de la glue
  seulement pour ce qui manque. La plupart des templates simples n'en ont pas.
- **Cycle de vie** : un auteur peut écrire dans sa glue un algorithme propre à
  son application quand aucun bloc ne le fait. Quand ce morceau sert à
  plusieurs templates, il devient un bloc d'OPC.
- **Aucune constante de réglage dans la glue.** Tout ce qui varie d'un site à
  l'autre vient d'un bloc ou d'un paramètre. C'est indispensable pour un
  `.uf2`, dont l'utilisateur ne peut modifier que le menu.
- **Sûr par défaut** :
  - la glue ne touche jamais aux `Output` ;
  - une sortie dont la glue ne donne pas l'état passe en état sûr ;
  - une alarme dont la glue ne donne pas la condition est signalée.

---

## 3. Décisions prises

| # | Sujet | Décision | Raison |
|---|---|---|---|
| 1 | `SolarRegulator` | supprimé, découpé en blocs génériques + glue | c'est une application déguisée en bloc ; la logique solaire doit se lire dans le template |
| 2 | Commande de la glue | à écrire à chaque cycle, sinon état sûr, avec un événement dans le journal | un oubli dans une branche se voit dès la mise en service, au lieu de laisser une sortie en marche |
| 3 | Mode manuel des sorties de la glue | présent par défaut, retirable par le template | uniforme, comme `lockFaultAction()` ; retiré pour les sorties sensibles (résistance chauffante) |
| 4 | Accès au menu | ouvert à tous | l'utilisateur est responsable, et l'installateur règle finement sur place |
| 5 | Clés de configuration | pas de compatibilité à garder | OPC est un prototype, rien n'est déployé |
| 6 | Distribution | source + `.uf2` par template | un `.uf2` s'installe par simple glisser-déposer |
| 7 | Écran d'accueil | écrit librement par l'auteur, un seul fichier par template | des blocs d'affichage seraient trop rigides ; on pourra séparer les fichiers plus tard |
| 8 | Retrait du mode manuel du Thermostat et du PID | reporté | le mode manuel est en place, on le laisse tel quel |
| 9 | Dégivrage | par intervalle, sans horloge | un groupe froid n'a pas besoin de l'heure |
| 10 | Témoin ou voyant | libre au template, rien dans OPC ni dans les démonstrateurs | rarement utile en pratique ; le témoin le plus courant est une présence tension, sans logique |
| 11 | Masquage d'alarme | inhibition par la glue | évite les fausses alarmes de dégivrage sans allonger le retard des vraies |
| 12 | Phase 1 | en trois étapes (1a, 1b, 1c), chacune testée et commitée | l'inhibition touche chaque régulateur et chaque alarme ; son API doit être validée par un utilisateur réel |

---

## 4. Phases

### Phase 1 : le noyau de la glue

La phase 1 se fait en trois étapes. Chacune se termine par les tests sur
l'hôte, par `pio run` (1.1 touche `OPC.cpp`, que seul le build firmware
vérifie) et par un commit.

| Étape | Contenu | Premier utilisateur |
|---|---|---|
| **1a** | 1.1, 1.2 et 1.6, avec leurs tests et le README | le template `TestIO` (1.6) |
| **1b** | 1.3, l'inhibition des régulateurs | le démonstrateur solaire ou le brasseur |
| **1c** | 1.3, l'inhibition des alarmes ; elle peut attendre la veille de 3.2 | la chambre froide |

1.1 ✅ **Point d'appel** : `processLogic(now)` et `resumeLogic(now)` dans
`Installation`, vides par défaut.
- `ProcessControl` les appelle après les régulateurs et les alarmes, et avant
  les actionneurs. Il passe par une petite interface, pour rester testable sur
  l'hôte.
- Comme le reste du cycle, elles ne sont pas appelées pendant une pause menu ou
  un timeout de mesure.
- `resumeLogic()` est appelée à la reprise, pour réarmer l'état de la glue.

1.2 ✅ **`LogicCommand`**, la sortie pilotée par la glue. C'est un `Regulator`,
qu'on relie à un actionneur comme les autres.
- `setOn()`, `set()` (0..1), `invalidate()`.
- Elle doit être écrite à chaque cycle. Sinon, elle passe en état sûr, avec
  un événement dans le journal. Elle est aussi invalide au démarrage et après
  une reprise, jusqu'à sa première écriture.
- `dependsOn(mesures, entrées TOR ou blocs…)` : si une dépendance n'est pas
  `Ok` (ou valide, pour une entrée TOR), la sortie
  passe en état sûr, ou en maintien selon `faultSettings`
  (`handleMeasurementFault`). L'action sur défaut reste verrouillable par
  `lockFaultAction()`.
- Le verrouillage par alarme (`setInterlock()`) fonctionne comme sur les
  autres régulateurs.
- Le réglage `Commande` Auto / Marche / Arrêt a la même sémantique que sur le
  Thermostat : en manuel, la mesure est ignorée. Il est marqué live et le
  template peut le retirer.

1.3 ⬜ **L'inhibition**, un levier non persistant de la glue. Il est distinct
du réglage `Activée` des alarmes : il faudra un autre nom que `setEnabled()`,
car `Alarm::isEnabled()` existe déjà.
- **Un régulateur inhibé** a sa sortie arrêtée sur ordre (ce n'est pas un
  défaut). Il n'a pas non plus de consigne active (`readSetpoint()` est faux),
  ce qui suspend les alarmes relatives à sa consigne.
- **Une alarme inhibée** ne peut pas se déclencher, et son retard repart de
  zéro à la levée de l'inhibition. Une alarme déjà mémorisée le reste jusqu'à
  l'acquittement.
- **Un défaut de sonde n'est jamais masqué**, comme c'est déjà la règle dans
  `LimitAlarm`.
- **Seules les alarmes déclarées inhibables** dans `begin()` peuvent l'être.
  Sinon, une glue boguée pourrait faire taire une alarme de verrouillage
  (`LoopBreakAlarm`).
- **Chaque régulateur doit être revu (étape 1b)**, car une ligne dans la
  classe de base ne suffit pas :
  - **PID** : l'intégrale ne doit pas dériver pendant l'inhibition, et la
    reprise doit se faire sans à-coup, comme au retour du mode manuel. Il faut
    aussi décider du sort d'un autotune en cours et de la rampe de consigne ;
  - **Thermostat** : plus de consigne active (`readSetpoint()` faux), et un
    départ propre à la levée de l'inhibition ;
  - **`LoopBreakAlarm`** : un régulateur inhibé ne doit pas être pris pour une
    boucle ouverte. `isAutomatic()` doit probablement renvoyer faux pendant
    l'inhibition ;
  - **programme horaire** et **comparateur** : définir l'état de sortie
    pendant l'inhibition et à sa levée.

1.4 ⬜ **Tests sur l'hôte** (faits pour l'étape 1a, `test/host/test_logic.cpp` ; reste l'inhibition) : ordre d'appel, commande non écrite, `dependsOn`,
reprise, mode manuel et son retrait, verrouillage par alarme, inhibition des
régulateurs et des alarmes.

1.5 ⬜ **README** (fait pour l'étape 1a : « Créer une installation > 3. Ajouter
de la glue » et « Mode manuel » ; reste l'inhibition).

1.6 ✅ **Premier utilisateur de la glue (étape 1a) : le template `TestIO`.**
C'est un banc d'essai : on peut lui ajouter des fonctions fictives pour
éprouver l'API sur la carte.
- **Remplacer le contournement.** `TestIO::InputCommand` (un `Regulator`
  imbriqué qui recopie une entrée TOR) disparaît, au profit de deux
  `LogicCommand` et d'une glue :
  *commande N = 50 % si l'entrée N est active, sinon 0*. Le relais N et la
  sortie PWM N suivent toujours l'entrée N, comme aujourd'hui.
- **`dependsOn` sur la carte** :
  - chaque commande dépend de son entrée TOR ;
  - la commande 2 dépend aussi de la Pt100. Une sonde débranchée met le
    relais 2 et la PWM 2 en état sûr, même entrée 2 active.
- **Mode manuel, dans les deux sens** :
  - la commande 1 garde son réglage `Commande` Auto / Marche / Arrêt, qu'on
    exclut de la mise en lecture seule de `TestIO`. On peut alors forcer le
    relais 1 depuis le menu, sans entrée ;
  - la commande 2 a son mode manuel retiré, et le réglage doit être absent du
    menu.
- **Commande non écrite (décision 2)** : une action de menu fictive, « Simuler
  oubli glue », fait sauter l'écriture de la commande 1 pendant 5 s. Le
  relais 1 doit passer en état sûr, et un événement doit apparaître dans le
  journal.
- **Écran** : il affiche en plus l'état de la Pt100 et le mode de la
  commande 1, et indique « Relais 2 : entrée 2 + PT100 Ok ».
- Ce qui reste testé seulement sur l'hôte à cette étape : l'ordre d'appel,
  `resumeLogic()` et le verrouillage par alarme.

### Phase 2 : les nouveaux blocs

2.1 ⬜ **Comparateur** :
- un **seuil** (une mesure) ou un **différentiel** (deux mesures) ;
- seuils de marche et d'arrêt réglables au menu ;
- invalide si une de ses mesures est en défaut ;
- c'est un `Regulator` : on peut le relier directement à une sortie dans
  `begin()` (un hors-gel sans glue), ou le lire depuis la glue.

2.2 ⬜ **Temporisation** :
- retard à la montée ou à la descente, réglable au menu ;
- durées de quelques secondes à plusieurs heures (intervalle de dégivrage) ;
- reprise propre après une pause, et calcul de durée sûr au débordement de
  `millis()`.

2.3 ⬜ **Alarme sur condition** :
- réglages `Activée`, `Retard` et `Mémorisation` ;
- source : une **entrée TOR liée dans `begin()`** (une porte, sans glue) ou une
  **condition écrite par la glue** ;
- elle se déclenche si l'entrée TOR est invalide, ou si la glue n'écrit pas la
  condition pendant un cycle ;
- elle hérite de tout le reste de la classe `Alarm` : mémorisation,
  acquittement, bandeau d'accueil, journal, action « Acquitter », pilotage d'un
  relais. Elle compte dans `MAX_ALARMS`.

2.4 ⬜ **Tests sur l'hôte et README** pour les trois blocs.

### Phase 3 : les démonstrateurs

3.1 ⬜ **Le solaire réorganisé.** `SolarRegulator` est supprimé.
- **Blocs** :
  - comparateur différentiel « charge » (capteur − bas du ballon, marche 8 K,
    arrêt 4 K) ;
  - comparateur « ballon max » (haut du ballon) et comparateur « capteur min » ;
  - pour la décharge vacances : comparateur « bas du ballon chaud » et
    comparateur différentiel « décharge » (bas du ballon − capteur) ;
  - programme « Décharge nuit » ;
  - paramètre « Mode vacances » ;
  - `LogicCommand` pompe, qui dépend des trois sondes.
- **Glue** :

  ```
  charge   = différentiel charge ET NON ballon max ET capteur min
  décharge = NON charge ET mode vacances ET plage de nuit
             ET bas du ballon chaud ET différentiel décharge
  pompe    = charge OU décharge
  ```

- L'appoint électrique (Thermostat + programme « Heures creuses ») ne change
  pas.
- On garde **la même régulation qu'avant, prouvée par les tests**. On porte
  les tests de décharge de `test_schedule.cpp`. La pompe gagne le mode manuel.
  La décharge a maintenant ses propres seuils, avec les mêmes valeurs par
  défaut que la charge.
- **Point d'attention.** Dans `SolarRegulator`, une limite (ballon max,
  capteur min) remet la charge à l'arrêt : il faut ensuite de nouveau le delta
  de démarrage pour repartir. En blocs, le comparateur différentiel garde son
  propre état, et la pompe pourrait repartir dès que la limite disparaît, sur
  un simple delta d'arrêt. Il faut soit l'accepter, soit inhiber le
  différentiel pendant une limite. Les tests portés trancheront.

3.2 ⬜ **La chambre froide positive** (fromagerie, 2 à 4 °C), démonstrateur
principal.
- **Matériel** :
  - Pt100 d'ambiance (régulation) et Pt100 d'évaporateur (fin de dégivrage) ;
  - contact de porte sur l'entrée TOR 1 ;
  - relais 1 : compresseur ; relais 2 : ventilateurs de l'évaporateur ;
  - PWM 1 : résistance de dégivrage, par un relais statique.
  - Variante sans résistance : dégivrage par simple arrêt du compresseur, avec
    les 2 relais seulement.
- **Liaisons dans `begin()`** :
  - Thermostat en mode froid sur l'ambiance → compresseur. Les temps mini du
    relais assurent l'anti-court-cycle.
  - Comparateur seuil « Fin dégivrage » sur l'évaporateur (8 °C).
  - Temporisations : intervalle de dégivrage (6 h), durée max du dégivrage,
    égouttage (2 min), retard des ventilateurs (3 min), masquage de l'alarme
    après le dégivrage (30 min).
  - `LogicCommand` ventilateurs et résistance. La résistance dépend de
    l'évaporateur, son état sûr est verrouillé et son mode manuel est retiré.
  - Alarme de porte sur l'entrée TOR, avec un retard (5 min) : sans glue.
  - `LimitAlarm` d'écart haut sur l'ambiance, référencée au thermostat et
    déclarée inhibable.
- **Glue** :

  ```
  Froid      : temporisation « intervalle » écoulée   → Dégivrage
  Dégivrage  : « Fin dégivrage » OU durée max          → Égouttage
  Égouttage  : temporisation d'égouttage écoulée       → Reprise
  Reprise    : temporisation des ventilateurs écoulée  → Froid

  thermostat inhibé     = Dégivrage ou Égouttage
  résistance            = Dégivrage
  ventilateurs          = Froid ET porte fermée
  alarme haute inhibée  = pas en Froid, ou masquage après dégivrage non écoulé
  ```

- **Ce qu'il met à l'épreuve** : `LogicCommand` (`dependsOn`, retrait du mode
  manuel), l'inhibition d'un régulateur et d'une alarme, le comparateur, la
  temporisation, l'alarme sur entrée TOR, et une glue propre à l'équipement.
- En cas de défaut de la sonde d'ambiance, le compresseur passe en état sûr
  (arrêt) et l'alarme de défaut de sonde se déclenche : c'est à l'utilisateur
  d'intervenir.

3.3 ⬜ **Des templates compilables dans les tests sur l'hôte**, pour tester la
glue des deux démonstrateurs. Il faudra probablement un fake `Adafruit_GFX`
pour `printHomeScreen()`.

### Phase 4 : distribution et partage

4.1 ⬜ Un environnement PlatformIO par template, donc un `.uf2` par template,
et une sélection de template sans éditer `src/main.cpp`.

4.2 ⬜ L'identité du firmware dans le menu : template, version, auteur.

4.3 ⬜ **Un guide d'auteur de template**, court et normatif, utile autant aux
humains qu'à une IA :
- les principes du § 2 ;
- les règles : code non bloquant (pas de `delay()`, d'écriture de fichier ni
  de longues sorties série), aucun accès direct aux `Output`, `dependsOn`,
  clés de configuration stables, `fail()`, écran dessiné depuis le snapshot
  seulement ;
- un exemple court : la **cuve d'eau chaude du brasseur**. Le thermostat de la
  cuve est inhibé hors heures creuses (contact du compteur sur une entrée TOR)
  ou si la chauffe de nuit n'est pas armée. Le thermostat de la cuve 2 est
  inhibé tant que la cuve 1 chauffe (délestage). Le tout tient en quelques
  lignes de glue.

4.4 ⬜ Une version d'API (`OPC_API_VERSION` + `static_assert` dans le
template). Priorité faible.

4.5 ⬜ **Corriger la documentation** :
- le fichier est `src/main.cpp`, mais `README.md` (lien du § choix de
  l'installation et arborescence) et `examples/MinimalInstallation/README.md`
  citent `src/Templates/main.cpp` ;
- dans la revue de code, l'item 24 décrit l'inverse et doit être corrigé.

---

## 5. Points techniques à vérifier

- ⬜ Une sortie PWM peut-elle être pilotée en tout ou rien (résistance de
  dégivrage par relais statique) ?
- ⬜ Un changement de template par `.uf2` ne doit pas charger les réglages de
  l'ancien. `Storage::restore()` reçoit `configurationKey()`, donc c'est
  probablement déjà le cas.
- ⬜ Ce qu'il manque pour compiler un template complet dans les tests sur
  l'hôte.

---

## 6. Plus tard, si un besoin apparaît

- **Consigne d'exécution** non persistante sur les régulateurs (loi d'eau,
  réduit sur contact). Aucun démonstrateur ne l'utilise : elle sera ajoutée
  quand une application la demandera.
- **Retrait du mode manuel** du Thermostat et du PID (décision 8).
- **Templates séparés en deux fichiers** (logique / écran).

---

## 7. Hors périmètre

| Sujet | Raison |
|---|---|
| Séquences de procédé (paliers, recettes) | l'opérateur décide lui-même des étapes |
| Assistance opérateur (consigne atteinte, consignes prédéfinies, minuterie) | écartée pour l'instant |
| Programmation de la logique depuis le menu | beaucoup de code lourd, et l'IHM n'est pas faite pour |
| Graphe de signaux entre blocs | inutile, la glue en C++ suffit |
| Niveaux d'accès utilisateur / installateur | le menu reste ouvert (décision 4) |
| Blocs d'affichage | trop rigides, chaque auteur écrit son écran |
| Dégivrage à heure fixe | un groupe froid n'a pas besoin de l'heure (décision 9) |

---

## 8. Liens avec les autres documents

- `Comparatif_JUMO_diraTRON116.md` :
  - la ligne « Logique et calcul » (⬜) : ce plan y répond par la glue en C++
    dans le template, et non par des fonctions logiques configurées au menu ;
  - le § 4 et le point 4 de la feuille de route, « Template universel
    configurable par le menu » : c'est un sujet distinct, que ce plan ne
    couvre pas ;
  - le point 2 de la feuille de route, « Verrouillage et niveau opérateur » :
    il contredit la décision 4.
- `Revue_code_OPC_v0.2.md`, item 24 : voir 4.5.
