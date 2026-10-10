# Plan d'action : acquisition hors ISR et routage MCP23017 vérifié

Plan établi le 10/10/2026, état du code : commit `dc06a03` (bloc et template
four).

Il traite deux points ouverts, qui ont une cause commune :

- [revue](Revue_code_OPC_v0.2.md), point 8 : sortir le séquencement de l'ISR
  DRDY ;
- revue, point 4 (reste ouvert), et [bilan](bilan.md), P0 n°1 : routage
  analogique non vérifié pendant l'acquisition.

Il couvre aussi l'essentiel du bilan P0 n°7 (récupération du bus I²C) et une
partie du P2 n°12 (ADS1120 redémarré).

Légende : ✅ fait · ⬜ à faire

---

## 1. Constat

Le routage I²C est fait dans l'ISR DRDY. Dans une ISR, on ne peut ni attendre
un timeout raisonnable, ni réessayer, ni réinitialiser le MCP, ni récupérer le
bus. Sortir le séquencement est la condition pour sécuriser le routage.

- **L'ISR fait tout le séquencement** (`SensorBoard::adcInterrupt`). En plus de
  la lecture de l'échantillon :
  - l'inversion des sources de courant en 3 fils ;
  - le diagnostic d'étendue ;
  - `compute()` ;
  - le routage de la voie suivante ;
  - la température de l'ADC.
- **Un changement de voie dure 2,7 ms dans l'ISR** (mesuré, voir l'étape 0) :
  11 à 14 `mcp.digitalWrite()` à environ 0,2 ms chacun, soit 22 à 28
  transactions I²C à 400 kHz. Cela arrive à chaque voie et à chaque début de
  cycle.
- **Une lecture ratée met tout un port à 1.**
  - `digitalWrite()` de la bibliothèque Adafruit lit `GPIO` (pas `OLAT`),
    modifie un bit et réécrit le port.
  - Si la lecture échoue, `Adafruit_BusIO_Register::read()` renvoie `-1`,
    soit `0xFF`. Le port est réécrit avec toutes ses broches à 1.
  - Port A : commutateurs 2, 3 et 4 fils, les deux Rref, `TC_GND` et `TC_VCC`
    fermés ensemble. Port B : les trois entrées en parallèle.
  - Il suffit d'une lecture ratée, sans reset du MCP. Aucune erreur ne remonte.
- **Le délai de `Wire1` n'est pas réglé** : c'est celui de `Stream`, 1 s par
  transaction. Un bus bloqué fige l'ISR, donc tout le cœur 0, jusqu'au
  watchdog. Les sorties restent dans leur dernier état pendant ce temps.
- **`SharedBusGuard` masque DRDY** pendant les lectures du DS3231 et pendant
  tout `updateMeasurementsAndRegulators()`, pas seulement pendant la lecture
  du BMP580. Le relevé de l'étape 0 montre que l'effet est faible
  aujourd'hui : aucun trou de plus de 1,5 ms ne lui est attribuable. Il n'est
  là que parce que l'ISR fait de l'I²C, et disparaît avec elle.
- **Le cœur Arduino-Pico sait récupérer le bus** avec
  `Wire1.setTimeout(ms, true)` : 9 impulsions SCL puis un STOP. Il fait pour
  cela `end()`/`begin()` et des `sleep_us()`, ce qui exclut l'ISR.

---

## 2. Décisions prises

| # | Sujet | Décision | Raison |
|---|---|---|---|
| 1 | Contenu de l'ISR | lecture et accumulation seulement ; l'inversion 3 fils passe aussi dans `loop()` | un seul propriétaire de la configuration de l'ADC |
| 2 | Écriture du routage | 2 transactions : nouvelle config avec aucune voie, puis la voie | garde le passage par « tout coupé » d'aujourd'hui, sans chevauchement des commutateurs |
| 3 | Contrôle `GPIO = OLAT` | inscrit au journal seulement, puis bloquant après validation sur la carte | une broche chargée pourrait donner un faux défaut permanent |
| 4 | État affiché | `Invalid`, et la cause dans le journal | rien à changer en aval (libellés, alarmes, snapshot) |
| 5 | Lecture de fin impossible | la mesure passe en `Invalid` | le routage n'est pas prouvé, donc la mesure non plus |
| 6 | Périmètre | inclut le délai et la récupération du bus, le timeout d'étape de l'ADC, et des compteurs de diagnostic | même logique, peu de code |
| 7 | Tests | séquenceur et encodage du routage dans des fichiers testables sur PC | le vrai `SensorBoard` n'est compilé que pour la carte |

---

## 3. Architecture cible

### ISR

- Si elle n'est pas armée, elle sort.
- Si DRDY est déjà haut à l'entrée, elle sort aussi : c'est une IRQ sans
  nouvelle donnée, qui lirait `-1` (voir l'étape 0, IRQ DRDY parasites).
- Sinon elle lit l'échantillon (SPI), puis l'ignore pendant l'établissement ou
  l'accumule dans le `Sensor`.
- À mi-série en 3 fils, en fin de série, ou après une conversion unique
  (diagnostic, température), elle se désarme et signale l'événement.
- Elle ne fait plus d'I²C et ne configure plus l'ADC. Durée visée : celle d'un
  échantillon aujourd'hui (environ 33 µs), pour toutes les IRQ, au lieu de
  pointes à 2,7 ms.

### Séquenceur

Machine d'états non bloquante, appelée en tête de `loop()` sur le cœur 0. Il
ne voit le MCP et l'ADC qu'à travers deux petites interfaces : `SensorBoard`
les implémente sur la carte, des faux les implémentent dans les tests.

| État | Action dans `loop()` | Suite |
|---|---|---|
| Routage(n) | écrit et vérifie le routage, configure l'ADC, arme l'ISR | Acquisition |
| Acquisition(n) | attend l'ISR, avec un timeout d'étape | Inversion, Diagnostic ou Fin de voie |
| Inversion (3 fils) | inverse les IDAC, réarme (15 conversions ignorées, comme aujourd'hui) | Acquisition |
| Diagnostic d'étendue | passe au gain large, réarme pour 1 conversion | Fin de voie |
| Fin de voie(n) | revérifie le routage, puis `compute()` ou rejet ; coupe la voie | Routage(n+1) ou Temp. ADC |
| Temp. ADC | lance la conversion ; l'ISR la lit | fin de cycle, `newMeasurement` |
| Arrêt | menu, calibration | aucune |

- Le menu remplace `pause()` / `resetAcquisition()` / `startContinuous()` par
  l'arrêt et le départ du séquenceur.
- La calibration (`readCalibrationSamples`) reste en lectures interrogées,
  séquenceur arrêté. Elle utilise la nouvelle écriture vérifiée du routage, et
  une vérification ratée refuse la calibration.
- **Une voie en défaut garde sa durée normale.** Sinon un MCP muet ferait
  tourner le cycle en quelques ms : la régulation s'emballerait et le bus serait
  martelé.

### Partage du SPI

L'ISR et `loop()` utilisent tous deux le SPI0 de l'ADC.

- L'ISR ne lit que si elle est armée.
- `loop()` ne touche l'ADC que si l'ISR est désarmée.
- On désarme avant toute configuration. On arme en dernier : `startSync`,
  acquittement du front en attente, puis armement, comme `restart()`
  aujourd'hui.
- L'ISR écrit les sommes du `Sensor`, `loop()` les lit : drapeaux `volatile`
  et barrière du compilateur (même cœur).

---

## 4. Routage MCP23017

### Pilote par registres

Il remplace la bibliothèque Adafruit MCP23017.

- Copie de `OLATA` / `OLATB` en RAM, écriture de ports entiers, plus aucune
  lecture-modification-écriture.
- Initialisation : `IOCON` (`BANK` = 0, adresses séquentielles), puis `OLAT` =
  0, puis `IODIR`. Les broches passent en sortie déjà à 0. Utile après un
  redémarrage par le watchdog : le MCP n'a pas de /RESET câblé et garde son
  ancien état.

### Encodage du routage

Fonction pure, testable sur PC : réglages + voie → `{portA, portB}`. Elle doit
reproduire exactement le routage actuel.

| Entrée | Port A |
|---|---|
| PT100 2 fils | `0x15` (SW_2, SW_2_3, RREF1k65) |
| PT1000 2 fils | `0x91` (SW_2, SW_2_3, RREF16k5) |
| Thermocouple | `0x59` (SW_2, SW_2_3, TC_GND, TC_VCC) |
| PT100 3 fils | `0x25` (SW_2_3, SW_3, RREF1k65) |
| PT1000 3 fils | `0xA1` (SW_2_3, SW_3, RREF16k5) |
| PT100 4 fils | `0x06` (SW_4, RREF1k65) |
| PT1000 4 fils | `0x82` (SW_4, RREF16k5) |

- Port B : voie n → bit n (`GPB0` à `GPB2`).
- Arrêt : port A et port B à 0.

### Écriture

1. `OLATA` = nouvelle configuration et `OLATB` = 0, en une transaction.
2. `OLATB` = la voie.

### Vérification

Lecture en rafale de `IODIR` à `OLAT` (22 octets, environ 0,6 ms) :

- `IODIR` : les 11 broches utilisées sont en sortie ;
- `IOCON.BANK` = 0 ;
- `OLAT` = la copie en RAM ;
- `GPIO` = `OLAT` sur les broches utilisées : inscrit au journal seulement
  (décision 3). Le passage en bloquant est une constante du code, changée après
  validation sur la carte.

### Encadrement de chaque acquisition

On vérifie avant d'armer l'ISR, puis à la fin, avant `compute()`. Après un
reset, le MCP repart avec `IODIR` = 0xFF et `OLAT` = 0, et seul le firmware le
reconfigure. Une configuration correcte à la fin prouve donc que le routage a
tenu pendant toute l'acquisition. Une relecture périodique séparée est
inutile.

Coût : environ 1,5 ms d'I²C par voie et par cycle, hors ISR.

---

## 5. Réaction aux défauts

- **Avant la mesure** (écriture ou vérification ratée) :
  1. réécriture et nouvelle vérification ;
  2. puis réinitialisation complète du MCP et nouvel essai ;
  3. puis la voie est `Invalid` pour ce cycle.

  Une erreur isolée ne coûte donc que du temps.
- **En fin d'acquisition** :
  - configuration fausse : mesure rejetée (`Invalid`), et réinitialisation du
    MCP avant la voie suivante ;
  - lecture impossible : une relecture, puis `Invalid` (décision 5).
- **Remontée** : `Sensor` passe en `Invalid`. `Resistance`, `TemperatureTC` et
  le psychromètre en héritent. Les sorties passent en état sûr, ou en
  `Maintien` si ce repli est réglé. Le cycle se termine quand même : l'état sûr
  est immédiat, au lieu d'attendre le timeout de mesure (30 s).
- **Journal** : « Défaut multiplexeur » à l'apparition, « Multiplexeur
  rétabli » après un cycle complet sans défaut. Écrits par
  `OPC::newMeasurement()` sous le mutex, comme « Heure perdue » / « Heure
  retrouvée ».
- **Timeout d'étape** : une acquisition plus longue que prévu (DRDY muet, ADC
  redémarré) passe en `Invalid`, et l'ADC est réinitialisé. Détection en
  quelques secondes au lieu de 30 s. Proposition : durée prévue × 2 + 1 s, pour
  couvrir une sauvegarde flash (IRQ coupées pendant des centaines de ms).
- **Compteurs de diagnostic** : erreurs I²C du MCP et réinitialisations depuis
  le démarrage. En lecture seule dans le menu Calibration, non sauvegardés.

---

## 6. Bus I²C

- `Wire1.setTimeout(10, true)` dans `OPC::initI2C()` : délai court et
  récupération du bus par le cœur. Il s'applique aussi au DS3231 et au BMP580.
- `SharedBusGuard` est supprimé : `Wire1` n'est plus utilisé que dans `loop()`
  du cœur 0, donc les accès sont sérialisés. À vérifier avant : aucun accès à
  `Wire1` depuis le cœur 1.

---

## 7. Risques et parades

| Risque | Parade |
|---|---|
| Réécriture d'une partie de mesure validée sur banc : établissement, inversion 3 fils, diagnostic d'étendue, thermocouples à 20 SPS | Tests sur PC du séquenceur ; mesures avant / après sur banc avec les mêmes étalons |
| Partage du SPI : une erreur corrompt les lectures sans aucun symptôme | Règle armée / désarmée enfermée dans le séquenceur et vérifiée par les tests |
| Cycle un peu plus long et moins régulier (latence de `loop()`) | Environ 0,15 ms par tour mesuré, donc moins de 1 ms par étape ; seule une sauvegarde flash retarde davantage, et elle bloque déjà l'ISR aujourd'hui |
| Un `Serial.print()` sur le cœur 0 bloque jusqu'à 1 s si l'hôte ne lit pas (`SerialUSB::write`) ; aujourd'hui il ne retarde que la régulation, demain aussi l'acquisition | Aucune écriture série dans le séquenceur : les défauts passent par le journal. Les autres messages du cœur 0 restent rares (démarrage, calibration, échec de sauvegarde). Le timeout d'étape garde sa marge |
| Délai de 10 ms commun au DS3231 et au BMP580 | Large marge sur la plus longue transaction (environ 0,6 ms) ; à vérifier sur banc |
| Récupération du bus par `end()`/`begin()` de `Wire1` | Seulement depuis `loop()` ; testée en mettant SDA à la masse |
| Faux défaut du contrôle `GPIO = OLAT` | Journal seulement tant qu'il n'est pas validé (décision 3) |
| Un défaut de routage met toutes les entrées en défaut | Voulu : le MCP est commun à toutes les voies. `Maintien` couvre les défauts brefs |
| Pilote MCP maison à maintenir | Petit : écrire et lire des registres consécutifs |
| Un parasite bref sur une broche, sans reset du MCP, reste invisible | Limite acceptée ; les contrôles de plausibilité (rupture, court-circuit, étendue) restent |

---

## 8. Étapes

Chaque étape compile et laisse une carte qui fonctionne.

0. ⬜ **Référence sur banc** (à faire sur la carte, avant tout changement).
   - ✅ Durée de l'ISR, relevée le 10/10/2026 avec `PRINT_CONVERSION_TIME`
     (`ThermostatInstallation`, une PT100 4 fils, commit `dc06a03`) :

     | IRQ DRDY | Durée |
     |---|---|
     | échantillon accumulé | 33 µs typique (20 à 47 µs) |
     | fin de série : `compute()`, voie coupée, départ de la temp. ADC | 416 µs |
     | temp. ADC lue, routage de la voie 0, relance | 2 688 µs |

     Ces durées partent du début de `adcInterrupt()`. Elles ne comptent ni
     la répartition de l'IRQ par `attachInterrupt`, ni le `Serial.println()`
     de la mesure elle-même.
   - ✅ Durée du cycle, même montage (16 échantillons, soit 15 conversions
     ignorées + 800 accumulées), relevée de 102 à 158 s après le démarrage
     avec un compteur des fronts DRDY et des trous de plus de 1,5 ms :
     - **836,3 ms**, à ±1 ms près, sur 65 cycles. 816 IRQ par cycle.
     - Un trou de 3,6 ms par cycle : le routage dans l'ISR. Un second trou,
       plus court, environ tous les 4 cycles : non identifié, sans effet
       notable.
     - Aucun trou attribuable à `refreshClock()` : le masquage par
       `SharedBusGuard` coûte peu aujourd'hui (moins de 1,5 ms à chaque fois).
     - La théorie donne environ 820 ms. Comme il n'y a pas d'autre trou,
       l'écart vient de l'ADC lui-même : environ 1,021 ms par conversion,
       soit environ 979 SPS au lieu de 1000. C'est probablement la tolérance
       de son oscillateur interne, à confirmer dans la fiche technique.
   - ✅ Écriture flash : le premier `saveEvents()` (123 s, juste après les
     2 min d'attente) dure 52 ms avec les IRQ coupées. Cela donne un trou de
     52,8 ms et un cycle de 888 ms. Une écriture flash allonge donc le cycle
     de sa durée et laisse un trou dans la fenêtre d'intégration. Le timeout
     d'étape prévu couvre largement ce cas.
   - ✅ Démarrage, relevé de 4 à 38 s avec `TestInstallation` (deux
     résistances fixes en PT100 4 fils, environ 100,06 et 120,02 Ω) :
     - en régime établi : 1 671,3 ms par cycle (2 × 835,7 ms), 1 631 IRQ,
       2 trous de 3,7 ms au plus (les deux routages faits dans l'ISR) ;
     - de 9,6 à 13 s : deux cycles perturbés, de +12 et +14 ms, avec 9 à
       10 trous de 8 ms au plus. `refreshClock()` y dure 3 à 4 ms au lieu de
       moins de 1 ms ;
     - `storage.poll()` rafraîchit 4 fois l'export USB (2,2 à 2,7 ms, IRQ
       coupées), après des branchements / débranchements du lecteur vus par
       le PC ;
     - cause probable des trous : le PC monte le lecteur USB. La pile USB
       (`tud_task()`, lectures MSC comprises) tourne dans une IRQ du cœur 0
       (`USB.cpp` du cœur Arduino-Pico), qui retarde DRDY et ralentit
       `loop()`.
   - Cycles de 1 187 et 971 ms relevés avec le thermostat 15 à 17 s après
     le démarrage : probablement la même cause (activité USB et flash au
     démarrage), non revérifié.
   - ✅ **IRQ DRDY parasites : défaut de mesure silencieux.** Relevé du
     10/10/2026, `TestInstallation`, lecteur USB monté et démonté depuis le
     PC :
     - pendant l'activité USB et flash, des IRQ DRDY arrivent 45 à 52 µs
       après la précédente, c'est-à-dire juste après la fin de l'ISR
       précédente ;
     - à ce moment, DRDY est déjà haut : il n'y a pas de nouvelle donnée.
       L'ADS1120 renvoie alors `0xFFFF`, soit `-1`, et l'ISR l'accumule comme
       un échantillon ;
     - chaque échantillon parasite fausse la moyenne de -1/800. Jusqu'à 31
       parasites par cycle ont été vus, soit jusqu'à -1,5 % : RTD1 lit
       -5,3 °C au lieu de 0,15 °C, RTD2 45,0 °C au lieu de 51,6 °C ;
     - le compte est exact : 10 parasites sur E1 et 12 sur E2 expliquent
       98,808 et 118,224 Ω ;
     - l'état reste `Ok` : la valeur n'est pas saturée, donc rien ne la
       détecte ;
     - un parasite pendant les conversions ignorées réduit aussi le temps
       d'établissement ;
     - en régime calme, aucun parasite.

     Correctif : l'ISR ne lit que si DRDY est encore bas à l'entrée.
     Sinon, elle ignore l'IRQ, et la conversion suivante redonnera un front.
     Appliqué à l'ISR actuelle le 10/10/2026 (revue, point 26). La règle est
     reprise dans la nouvelle ISR (§ 3). Vérifié sur banc le 10/10/2026 :
     - démarrage avec montage USB par le PC : 2 IRQ ignorées ;
     - montages et démontages répétés du lecteur pendant 40 s : jusqu'à 52
       IRQ ignorées par cycle, et `storage.poll()` jusqu'à 104 ms ;
     - aucun échantillon aberrant. Les moyennes des cycles perturbés restent
       dans le bruit : E1 de 15 891,88 à 15 892,03 points, RTD1 à 100,058 ou
       100,059 Ω, RTD2 à 120,024 Ω. Avant le correctif, la même charge
       donnait jusqu'à -6 °C.
   - ✅ Bruit de référence, résistances fixes en PT100 4 fils (environ
     100,06 et 120,02 Ω), 20 cycles calmes : écart type de 0,03 point ADC sur
     la moyenne de chaque entrée. Cela fait environ 0,2 mΩ, soit environ
     0,5 mK.
   - Curiosité, hors plan : `storage.poll()` dure jusqu'à 104 ms, souvent
     sans trou de DRDY correspondant (4,1 ms au plus dans ce cycle). Une seule
     fois, un trou de 31 ms correspond à un `storage.poll()` de 31 à 33 ms. Les
     IRQ ne restent donc pas toujours coupées pendant toute la durée de
     `InterruptGuard`. À éclaircir avec le point 13 de la revue.
   - Constat annexe, hors de ce plan : à environ 979 SPS, les 800
     échantillons durent environ 817 ms, soit 40,85 périodes de 50 Hz au lieu
     de 40. La moyenne manque le zéro de réjection à 50 Hz, mais garde au
     moins environ 42 dB (enveloppe de la moyenne glissante). Sur ce banc, la
     mesure reste à ±1 mΩ.
   - ✅ Rythme de `loop()` : environ 0,15 ms par tour, `Serial.println()`
     compris, tant que l'USB suit. Ensuite, le `println()` à chaque tour sature
     le terminal : 3 à 4 ms par tour, et des trous jusqu'à 124 ms. Ces trous
     viennent de la mesure elle-même (voir § 7).
   - ⬜ Moyenne et écart type sur des étalons en PT100 2, 3 et 4 fils, PT1000
     et thermocouple.
1. ✅ **Pilote MCP par registres et encodage du routage.**
   - `AnalogRoute` (`src/Hardware/AnalogRoute.h`) : réglages + voie →
     `{portA, portB}`, sans dépendance au bus.
   - `AnalogMux` : initialisation (IOCON, sorties à 0, puis directions) et
     `write(route)` en une transaction (`OLATA`, `OLATB`). Plus de
     lecture-modification-écriture.
   - `SensorBoard` : montage sans voie, configuration de l'ADC, puis voie.
     En fin de voie, la voie est coupée et le montage gardé. Arrêt et
     calibration : tout coupé (`AnalogRoute{}`).
   - Tests sur PC (`test_analog_mux.cpp`) : octets du § 4 pour chaque
     combinaison, séquence d'initialisation, écriture en une transaction,
     MCP absent ou NACK. Le simulateur `Wire` couvre maintenant les
     registres du MCP et garde le journal des écritures.
   - La bibliothèque Adafruit MCP23017 est retirée de `platformio.ini`
     (flash : 196 → 188 ko).
   - Le pilote est encore appelé depuis l'ISR à ce stade, avec 3 transactions
     par changement de voie au lieu de 22 à 28.
   - ✅ Sur banc le 10/10/2026, `TestInstallation`, deux PT100 4 fils :
     - mesures identiques à la référence : RTD1 100,059 Ω, RTD2 120,024 Ω ;
     - avec `PRINT_CONVERSION_TIME`, la fin de série passe de 416 à 244 µs, et
       la lecture de la température ADC suivie du routage de la voie 0 et de la
       relance de 2 688 à 538 µs.
   - ⬜ Sur banc : les autres valeurs du port A (2 et 3 fils, PT1000,
     thermocouple) ne sont vérifiées que par les tests sur PC.
2. ⬜ **Séquenceur dans `loop()`.**
   - ISR réduite, règle de partage du SPI.
   - Menu et calibration adaptés.
   - Suppression de `SharedBusGuard`.
   - Tests sur PC : ordre nominal, inversion 3 fils, diagnostic d'étendue,
     aucune configuration de l'ADC pendant que l'ISR est armée, arrêt et départ
     par le menu.
3. ⬜ **Vérification et réaction aux défauts.**
   - Encadrement, nouvelles tentatives, réinitialisation, `Invalid`, journal,
     timeout d'étape, compteurs.
   - Tests sur PC avec un faux MCP : NACK isolé rattrapé ; reset pendant
     l'acquisition rejeté puis rattrapé ; MCP muet (toutes les voies
     `Invalid`, cycle à durée normale, un seul événement) ; retour à la
     normale ; `GPIO ≠ OLAT` inscrit sans invalider ; DRDY muet.
4. ⬜ **Délai et récupération du bus `Wire1`.**
5. ⬜ **Validation sur banc.**
   - Comparaison avec l'étape 0.
   - SDA à la masse quelques secondes : défaut, état sûr, retour sans
     redémarrage.
   - Drapeau de compilation de test qui force `IODIR` = 0xFF : voie rejetée,
     réinitialisation, journal.
   - `GPIO = OLAT` relevé dans toutes les configurations, puis rendu bloquant.
6. ⬜ **Documentation** : revue (points 4 et 8), bilan (P0 n°1 et n°7), README
   (défauts de mesure), CLAUDE.md (description de l'ISR).

---

## 9. Hors périmètre

- Interruptions coupées pendant la sauvegarde (revue, point 13).
- Boucle de redémarrages (bilan, P1 n°6).
- Registre de défauts centralisé.
- Révision de carte : câbler /RESET du MCP sur un GPIO permettrait un vrai reset
  matériel.
