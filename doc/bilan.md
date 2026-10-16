# Bilan OPC v0.2 : ce qui reste avant un usage réel et une publication

Bilan du 09/10/2026, sur le commit `dc06a03` (bloc et template four). Mis à jour le 16/10/2026, sur le commit `3af7140` : routage analogique vérifié et bus I²C récupéré ([Plan_acquisition_hors_ISR.md](Plan_acquisition_hors_ISR.md)), ADC muet détecté, psychrométrie corrigée (sauf la mèche sèche), points 28 à 35 de la revue repris.

Il complète deux documents sans les répéter :

- [Revue_code_OPC_v0.2.md](Revue_code_OPC_v0.2.md) : défauts du code, avec leur état ;
- [Comparatif_JUMO_diraTRON116.md](Comparatif_JUMO_diraTRON116.md) : fonctions manquantes face à un régulateur industriel.

Le matériel est pris dans le [README](../README.md#matériel-carte-v02) (borniers, isolation, alimentation 24 V) et dans [Pinout_v0.2.h](../src/Hardware/Boards/Pinout_v0.2.h). Le dossier [hardware/](../hardware/) ne contient que la licence : les schémas n'ont pas pu être relus.

Légende des priorités :

- **P0** : à faire avant de laisser une installation réelle sans surveillance ;
- **P1** : à faire avant une publication (v1.0) ;
- **P2** : confort, et fonctions qui élargissent les usages.

Les points réglés depuis le bilan initial sont marqués ✅.

---

## 1. Verdict

| Domaine | État |
| --- | --- |
| Architecture logicielle | Solide : deux cœurs séparés, snapshot, états sûrs, watchdog à deux cœurs, sauvegarde atomique, restauration tolérante |
| Tests | 281 tests hôte, tous OK (16/10/2026). Templates testés complets sur PC. Firmware compilé (RAM 23 %, flash 6 %) |
| Sécurité de la régulation | Bonne base : défauts de sonde, timeout, LBA, alarmes, temps mini des relais, verrous d'état sûr |
| Robustesse matérielle (bus, démarrage, alimentation) | Acquisition hors ISR, routage MCP vérifié, bus I²C débloqué, ADC muet détecté. **Points ouverts** : sorties en haute impédance avant leur initialisation (erratum E9, pull-down matériels), boucle de redémarrages, 24 V non surveillé |
| Templates | 7 templates applicatifs + 2 de test. Il manque des applications phares (humidité, couveuse, chaud / froid) |
| Distribution | **Pas prête** : pas de version affichée, pas de release, un firmware à compiler par installation, matériel non publié |
| Documentation utilisateur | README très complet, mais écrit pour un développeur. Pas de guide d'installateur ni de schémas de câblage |

En résumé :

- l'OPC est **utilisable aujourd'hui** pour un usage personnel, sur une installation surveillée, avec un limiteur de sécurité matériel ;
- il n'est **pas encore publiable** comme régulateur que d'autres câbleront sans relire le code ;
- l'écart tient surtout à la robustesse matérielle et à la distribution, et peu aux fonctions de régulation.

---

## 2. Sécurité de fonctionnement (logiciel)

### P0

1. ✅ **Routage analogique non vérifié pendant l'acquisition**. Revue, points 4 et 8. Voir [Plan_acquisition_hors_ISR.md](Plan_acquisition_hors_ISR.md), étapes 1 à 3.
   - Constat initial : l'ISR DRDY commutait les entrées par le MCP23017, broche par broche, sans contrôle d'erreur. Un MCP redémarré (ESD, chute de tension, parasite sur le bus d'extension) donnait une mesure fausse mais plausible, restée `Ok`.
   - Fait : séquencement dans `loop()`, routage écrit en une transaction (`OLATA`, `OLATB`) et relu avant et après chaque entrée. Sur incohérence, l'entrée passe en `Invalid`, le MCP est réinitialisé, et le journal note `Défaut multiplexeur`. Vérifié sur banc en PT100 2, 3 et 4 fils.
   - Reste : la validation sur banc de la PT1000 et du thermocouple (étape 5 du plan).

2. ✅ **Four : rien n'arrête un chauffage à 100 % si le thermocouple sort du four**. Voir [KilnInstallation.cpp](../src/Templates/KilnInstallation.cpp).
   - Cas : le thermocouple tombe ou sort du four, et lit l'ambiance.
   - L'écart maxi fige le programme (attente) et le PID reste à 100 %.
   - L'alarme de boucle `Boucle four` était **désactivée par défaut**, et `Écart haut` ne voit qu'un four trop chaud.
   - Fait le 16/10/2026, sur le modèle des contrôleurs du commerce (Skutt `E-1`, Bartlett `FTH` / `FTL`, Orton `FtL`) :
     - `Boucle four` active par défaut (2 °C en 2 × Ti = 20 min), en montée comme en descente : elle voit aussi un relais statique collé pendant un refroidissement contrôlé ;
     - la boucle ouverte arrête la cuisson, et `Démarrer` est refusé jusqu'à l'acquittement ;
     - `SetpointProgram` : `Attente max` (bloqué par l'écart maxi, 120 min dans le four) et `Durée max` (20 h dans le four), qui arrêtent le programme. Alarme `Durée dépassée` ;
     - tests `test_kiln.cpp` : thermocouple hors du four, relais statique collé en descente, attente max, durée max.
   - Une carte déjà réglée garde ses valeurs sauvegardées : la boucle reste à activer au menu.

3. **Template PID : relais mécanique en proportionnel temporisé, période 10 s**. Voir [PIDInstallation.cpp](../src/Templates/PIDInstallation.cpp), sortie `OUTPUT_1`.
   - Avec une période de 10 s, le relais fait jusqu'à 8 640 manœuvres par jour.
   - Les 150 000 manœuvres d'un relais standard sont atteintes en 2 à 3 semaines.
   - Le template est le point de départ le plus copié : il doit être sûr par défaut.
   - À faire : passer le template sur la sortie DC 1 (relais statique), ou allonger la période à 60–120 s avec un avertissement dans le menu.

4. **Chauffage arrêté après une longue coupure de courant**. Voir [HeatingCurve.cpp](../src/Regulator/HeatingCurve.cpp), état `ClockInvalid`.
   - Une coupure de plusieurs jours peut décharger le supercondensateur du DS3231, et l'heure est alors perdue.
   - Sans heure, la loi d'eau passe en état sûr : vanne fermée, donc plus de chauffage jusqu'au réglage de l'horloge.
   - L'hiver, dans une maison inoccupée, c'est un risque de gel.
   - À faire :
     - un repli sur heure inconnue, choisi au menu : `Arrêt`, `Confort` ou `Réduit`, avec `Réduit` par défaut pour le chauffage ;
     - le même choix pour `ScheduledSetpoint` du thermostat et du PID ;
     - **mesurer l'autonomie réelle du supercondensateur**, et la publier.

### P1

5. **Mode manuel sans limite de durée**.
   - En manuel, la sortie ignore la mesure, les défauts de sonde et l'inhibition.
   - Une résistance laissée à 100 % en manuel le reste tant qu'on n'y touche pas, ou jusqu'au prochain redémarrage.
   - À faire : une durée maximale optionnelle (`Manuel max`, 0 = illimité), après laquelle le mode repasse en Auto. Elle serait à régler par défaut sur les templates de chauffage (four, PID).

6. **Boucle de redémarrages non détectée**.
   - Un blocage reproductible (bus I²C coincé, par exemple) provoque un redémarrage par le watchdog toutes les 8 s environ, sans fin.
   - Les sorties sans `Arrêt mini` peuvent alors battre à chaque démarrage.
   - Le compresseur reste protégé : `Arrêt mini` part du démarrage.
   - À faire :
     - compter les redémarrages watchdog consécutifs dans les registres *scratch* du watchdog ;
     - au-delà de N, rester en état sûr et afficher un écran de défaut ;
     - garder dans ces registres l'étape en cours, pour le diagnostic (proposition de la revue).

7. ✅ **Aucune récupération du bus I²C**. Voir [Plan_acquisition_hors_ISR.md](Plan_acquisition_hors_ISR.md), étape 4.
   - Le bus `Wire1` sert au MCP23017, au DS3231 et au BMP581. Il est aussi sorti sur le connecteur d'extension.
   - Fait : délai de 10 ms par transaction, puis déblocage (9 impulsions SCL, STOP) par le cœur Arduino-Pico ; déblocage aussi au démarrage, avant la première transaction. Le séquencement, qui fait les accès I²C, est sorti de l'ISR (revue, point 8).
   - Vérifié sur banc, SDA à la masse quelques secondes : entrées en défaut, « Heure perdue », puis retour sans redémarrage. L'essai a aussi trouvé le BMP580 muet affiché comme valide (revue, point 27, corrigé).

8. **Démarrage progressif** (proposé, non fait).
   - Usages : résistances blindées à sécher, limitation des appels de courant.
   - C'est un réglage par régulateur : sortie limitée pendant X min, ou sous un seuil de mesure.

9. **Psychrométrie**. Revue, point 10 : corrigé, sauf la mèche sèche.
   - ✅ Une seule formule de Magnus, sur l'eau et sur la glace, et un coefficient psychrométrique réglable.
   - ✅ Bulbe humide plus chaud que le sec de plus de 0,5 K : `Invalid`.
   - ⬜ Il manque la détection d'une mèche sèche : bulbe humide ≈ bulbe sec, ce qui donne une fausse HR à 100 %. C'est bloquant pour tout template qui régule l'humidité (voir § 5).

### P2

10. Revue, point 13 : interruptions coupées pendant toute une sauvegarde (des centaines de ms sur le cœur 0). Impact jugé faible le 12/10/2026 (DRDY retardée déjà traitée, `millis()` et PWM matériels) : laissé en l'état.
11. Revue, points 17, 18, 20 et 21 : nettoyage. `FixedBufferPrint` tronque en silence, `isAccumulationDone()` compare avec `==`, code mort, et `ADS1120::begin` laisse une transaction ouverte.
12. ✅ ADC redémarré ou muet : une étape sans conversion dans deux fois sa durée normale passe en `Invalid`, réinitialise l'ADC et note `ADC muet` au journal. Détection en quelques secondes, au lieu des 30 s du timeout des mesures.
13. Revue, points 28 à 35 (relecture du 12/10/2026). Les trois premiers sont des corrections courtes, faits le 16/10/2026 :
    - ✅ 28 : « Mesurer N0 » dure près de 7 s, proche du watchdog de 8 s. Plusieurs calibrations sur banc sans reset : laissé en l'état ;
    - ✅ 29 : après un démarrage raté, `/counters.json` et `/events.csv` pouvaient être écrasés ;
    - ✅ 30 : un défaut de la nouvelle session pouvait être fusionné avec un événement relu de la précédente ;
    - 31 à 35 : capacité des actions du menu, « Manœuvres » mal affiché (✅ 32), unité de `Kp`, BME280 muet non détecté, allocations dynamiques à documenter.

---

## 3. Failles et usages imprévus

| Faille | Conséquence | Proposition | Prio. |
| --- | --- | --- | --- |
| **Reset « 1200 bauds » du cœur Arduino-Pico** : un logiciel PC qui ouvre le port série à 1200 bauds (téléverseur, moniteur mal réglé) redémarre la carte en mode BOOTSEL | Plus de firmware, donc plus de régulation et plus de watchdog, jusqu'à une coupure de courant. Les sorties sont coupées, mais une chambre froide se réchauffe et un chauffage s'arrête | Désactiver ce reset dans le firmware de production (option de compilation, ou surcharge du callback de `SerialUSB` : à vérifier dans le cœur). Garder le bouton BOOTSEL pour la mise à jour | P1 |
| **Reprogrammation libre par l'USB** | Toute personne qui a accès au port USB peut remplacer le firmware | Acceptable pour un projet ouvert. À documenter : ne pas laisser l'USB accessible en façade. Le *secure boot* du RP2350 est hors sujet | P2 |
| **USB seul, 24 V absent** : la partie logique tourne, sans les sorties (README, § Isolation) | Le firmware croit piloter : écran, journal et compteurs sont faux. Le même cas se produit si le 24 V disparaît pendant que l'USB est branché | Matériel : détecter la présence du 24 V. Logiciel : défaut `24 V absent`, journalisé, avec les sorties notées hors service | P1 (carte) |
| **Disque USB : déconnexion et reconnexion à chaque sauvegarde** ([Storage.cpp](../src/Storage.cpp)) | Fenêtres « disque retiré » sous Windows, et gêne pour un logiciel de supervision sur le port série | Ne rafraîchir le disque qu'à la demande (action de menu), ou hors connexion | P2 |
| **Bus I²C d'extension sur le même bus que le multiplexeur** | Un câble d'extension long, en court-circuit ou soumis à une ESD arrête toute l'acquisition et l'horloge tant qu'il perturbe le bus. Depuis le § 2, points 1 et 7, c'est détecté (mesures en défaut, état sûr) et rattrapé sans redémarrage, mais la régulation reste arrêtée | Prochaine carte : bus d'extension séparé (`Wire` ≠ `Wire1`) ou tamponné (P82B715, ISO1540) | P1 (carte) |
| **Menu ouvert à tous** | Choix assumé (➖ dans le comparatif) | Garder ce choix, mais proposer un **verrou de façade** optionnel (appui long), contre les manipulations par erreur ou par un enfant en usage domestique | P2 |

---

## 4. Matériel, sécurité électrique et normes

Certains points sont tirés du README et du brochage, sans les schémas. Ils sont à confirmer sur le PCB.

### P0 : avant de câbler une installation réelle

1. **Thermocouple de four et isolation de la partie mesure**.
   - Les entrées analogiques et l'USB n'ont qu'une isolation **fonctionnelle** avec le 24 V.
   - Dans un four, un thermocouple peut toucher une résistance à 230 V (gaine fendue, résistance affaissée). Le secteur arrive alors sur la partie logique, et sur le PC branché en USB.
   - À faire, dans le README et dans le template four :
     - imposer un thermocouple à soudure isolée, gaine reliée à la terre ;
     - ne pas laisser l'USB branché pendant une cuisson.
   - Sur une prochaine carte, envisager une isolation renforcée de la partie mesure.

2. **Erratum RP2350-E9 et glitch des sorties au démarrage**. Revue, point 16 : partie logicielle corrigée.
   - Avant `RelayOutput::begin()`, les broches de sortie sont des entrées. Le pull-down interne du RP2350 (puce A2) peut ne pas tenir le niveau bas (erratum E9).
   - Un relais statique, ou une commande active à l'état bas, peut réagir.
   - ✅ Logiciel (16/10/2026) : le niveau sûr est écrit avant de passer la broche en sortie. `pinMode(OUTPUT)` ne passe plus par le niveau bas du reset.
   - À faire :
     - matériel : des pull-down externes de valeur faible (≤ 10 kΩ) sur les quatre commandes de sortie ;
     - vérifier la révision de puce montée.

3. **Charges inductives et puissance**.
   - Les relais sont des contacts travail (NO) de 5 A.
   - Le template chambre froide pilote le compresseur **directement sur le relais 1**, alors que le courant de démarrage d'un compresseur dépasse largement 5 A.
   - À faire : imposer dans la documentation un **contacteur** pour les compresseurs, les moteurs et les résistances de plus de ~1 kW. Prévoir un RC ou une varistance sur les contacts, et un fusible par circuit.

4. **Limiteur de sécurité matériel**.
   - Il est mentionné pour le four, le plancher chauffant et le PID.
   - À faire : en faire une règle écrite dans **chaque** template qui chauffe, avec un schéma de câblage. Le contacteur de puissance est commandé à travers le limiteur, jamais par l'OPC seul.

### P1 : prochaine révision de la carte

Le comparatif JUMO (§ 3) liste déjà : entrée 0–10 V / 4–20 mA, sortie analogique, RS485, alimentation 230 V, protections CEM, boîtier rail DIN. Ce bilan y ajoute :

| Ajout | Raison |
| --- | --- |
| Détection de la présence du 24 V (troisième voie ISO1212, par exemple) | Voir § 3 : sorties hors service sans le savoir |
| Bus I²C d'extension séparé ou tamponné | Voir § 3 |
| Pull-down externes sur les commandes de sortie | Voir l'erratum E9 ci-dessus |
| Contact inverseur (NO + NF) sur au moins un relais | Câblage d'une alarme à sécurité positive, d'une vanne tout-ou-rien sans relais d'interface, ou d'une vanne 3 points en Marche / Sens (verrouillage matériel des deux sens) |
| Retour d'état des sorties (lecture du contact, ou courant de la charge) | LBA plus rapide, détection d'un relais collé |
| Rétroéclairage de l'écran pilotable (aucune broche dans le brochage) | Mise en veille, durée de vie, usage de nuit (chambre) |
| Protection des entrées analogiques (TVS, résistances série) | Câbles de sonde longs, contact accidentel avec le 24 V |

### P1 : conformité, pour une diffusion au-delà de l'usage personnel

- Une carte vendue ou fournie montée, qui commute du 230 V, relève du marquage **CE** :
  - basse tension 2014/35/UE ;
  - CEM 2014/30/UE ;
  - RoHS ;
  - DEEE.
- Normes produit à viser :
  - EN 60730-1 (commandes automatiques à usage domestique) ou EN 61010-1 (laboratoire, process) ;
  - EN 61326-1 ou EN 60730 pour la CEM.
- Points de conception à vérifier en premier :
  - lignes de fuite et distances d'isolement entre les contacts des relais (230 V) et la zone 24 V : isolation **renforcée** si le 24 V est une TBTS ;
  - fentes dans le PCB ;
  - isolation bobine / contact des relais.
- Un kit open source à monter soi-même, sans vente de produit fini, reste sous la responsabilité de celui qui l'installe. À écrire clairement : la licence exclut la garantie, mais la documentation doit le rappeler en tête.

---

## 5. Templates

### Existants

Thermostat, PID, programmation horaire, solaire, chambre froide positive, circuit de chauffage, four céramique. S'y ajoutent `TestIO` et `testInstallation` (psychromètre de développement).

### Corrections

| Template | À faire | Prio. |
| --- | --- | --- |
| `PIDInstallation` | Sortie DC 1 (relais statique) ou période longue (§ 2, point 3) | P0 |
| `KilnInstallation` | ✅ LBA active par défaut, durée maximale de cuisson (§ 2, point 2). Reste : consignes d'isolation du thermocouple (§ 4, point 1) | P0 |
| `HeatingCircuitInstallation` | Repli sur heure inconnue (§ 2, point 4) | P0 |
| `ColdRoomInstallation` | Documenter le contacteur du compresseur. Ajouter une alarme `Temp. basse` (gel des produits, thermostat collé) | P1 |
| `SolarInstallation` | Protection du capteur : stagnation (arrêt de la pompe au-dessus de ~120 °C, pour protéger la pompe et le glycol) et antigel (installations sans glycol). Proposé dans la revue, non fait | P1 |
| `testInstallation` | En faire un template propre « **Indicateur / enregistreur** » : 3 voies, alarmes sur les relais, sans régulation. Très demandé en usage réel (surveillance d'une chambre, d'une cuve, d'un congélateur) | P1 |

### Manquants, par intérêt

| Template | Contenu | Bloc à créer avant |
| --- | --- | --- |
| **Cave d'affinage, séchoir, chambre de maturation** | Température (froid + chauffage) et humidité (humidificateur, déshumidification par le froid). Il valorise le travail de métrologie fait sur le psychromètre | Régulation sur l'HR, détection de mèche sèche (psychrométrie déjà corrigée) |
| **Couveuse** | Température PID, humidité, retournement des œufs (temporisation cyclique), arrêt du retournement avant l'éclosion (J-3), alarmes haute et basse | Temporisation cyclique (impulsion toutes les N min) |
| **Chambre de fermentation, bain thermostaté, chambre climatique** | Chauffage et froid sur la même consigne | **PID chaud / froid avec zone morte** (3 états) |
| **Ballon tampon et chaudière bois, charge d'ECS** | Pompe de charge sur différentiel, protection anti-condensation du retour, priorité ECS | Aucun : comparateurs et glue |
| **Serre** | Chauffage, ouvrant par vanne 3 points ou moteur, éclairage programmé, humidité | `ThreePointActuator` existe déjà |
| **Chambre froide négative** | Dégivrage par l'évaporateur, retard des ventilateurs, alarme basse | Variante de la chambre froide positive |
| **Étuve, sous-vide, bain-marie** | PID et minuterie lancée par l'opérateur (fin → arrêt et alarme) | **Minuterie opérateur** |
| **Template universel** | Type d'entrée, de régulateur, affectation des sorties et des alarmes, choisis au menu (comparatif, § 4) | Gros chantier ; à garder pour plus tard |

---

## 6. Options et fonctions manquantes

Reprises du comparatif quand elles y figurent déjà ; la colonne « Pourquoi » donne leur poids pour un usage réel.

| Fonction | Pourquoi | Prio. |
| --- | --- | --- |
| **Version du firmware, template et commit dans un menu `À propos`**, et au démarrage sur le port série | Support : savoir ce qui tourne sur une carte. `main.cpp` annonce 0.3, le port série affiche « v0.2 » | P1 |
| **Réinitialisation usine** (réglages de l'installation, réglages de la carte à part) | Aujourd'hui, il faut effacer la flash au PC | P1 |
| **Export du journal et des compteurs sur le disque USB** | `/events.csv` existe, mais seul `config.json` est visible par l'USB | P1 |
| **Enregistrement des courbes en CSV** (mesures, consigne, sortie), visible par l'USB | Mise en service et diagnostic. Remplacerait les scripts `serialLog.py` | P1 |
| **Import de la configuration par l'USB** (validée puis appliquée) | Installer plusieurs cartes identiques, sauvegarder avant une intervention | P1 |
| **Réglage de l'heure depuis le PC** (commande série) | Il n'y a pas de NTP | P2 |
| **Fonctions des entrées TOR au menu** : marche / arrêt à distance, seconde consigne, acquittement, défaut externe | Câbler un contact sans recompiler | P1 |
| **PID chaud / froid** avec zone morte | Fermentation, bains, chambres climatiques | P1 |
| **Minuterie opérateur**, temporisation cyclique | Étuve, couveuse | P2 |
| **Démarrage progressif** | Voir § 2, point 8 | P1 |
| **Modbus RTU**, d'abord sur l'USB série | Supervision (Home Assistant, automate) | P2 |
| Étalonnage deux points, coefficients CVD par sonde (certificat) | Mesures de précision. Le travail de calibration de `doc/V0.2 Measurements` le justifie | P2 |
| Point de rosée, humidité absolue en mesures | Les fonctions existent dans `Psychrometer` | P2 |
| Consignes commutables, deux jeux de paramètres | Process non linéaires | P2 |

---

## 7. Ergonomie

| Point | Proposition | Prio. |
| --- | --- | --- |
| **Test des sorties à la mise en service** : aujourd'hui, il faut flasher `TestIO` | Un menu `Divers > Test sorties` dans toutes les installations : forcer un relais pendant 10 s, avec confirmation et retour en Auto | P1 |
| **Diagnostic du câblage** | Afficher en lecture seule la résistance brute (Ω), ou la tension (µV), de chaque entrée, pour vérifier une sonde sans PC | P1 |
| **Confirmation des actions** (calibration, RAZ des compteurs, RAZ des N0, `Arrêter` la cuisson) | Un clic de trop efface une calibration. Proposé dans la revue | P1 |
| **Mini-courbe de tendance** à l'accueil | Voir si la température monte ou baisse, sans PC | P2 |
| **Aide en ligne courte** dans le menu (unité, plage, rôle) | Beaucoup de réglages techniques (Ti, Td, `Variation min.`) | P2 |
| **Langue** : tout est en français | Une version anglaise (au moins le README et les menus) élargit fortement la diffusion. C'est un gros chantier : à décider, pas à imposer | P2 |
| **Veille de l'écran** | Demande un rétroéclairage pilotable (§ 4) | P2 |

---

## 8. Distribution, documentation et qualité

### P1 : nécessaire pour une v1.0 publique

1. **Firmware prêt à l'emploi**.
   - Un `.uf2` par template, publié dans une *release* GitHub avec un changelog. C'est la phase 4 du plan de la logique d'installation.
   - Aujourd'hui, changer d'installation demande de modifier `main.cpp` et de recompiler.
2. **Version unique et visible**. Des tags git (`v0.3.0`…), la même chaîne dans le firmware, le port série, le menu `À propos` et le nom des `.uf2`.
3. **Compilation reproductible**.
   - [platformio.ini](../platformio.ini) pointe sur la plateforme `maxgerhardt/platform-raspberrypi.git` sans révision fixée, et les bibliothèques sont en `^x.y.z`.
   - À faire : figer le commit de la plateforme et les versions exactes des bibliothèques.
4. **Intégration continue**. Une GitHub Action qui lance les tests hôte et `pio run`, et publie les `.uf2`. C'est proposé dans la revue ; il n'y a pas de dossier `.github/`.
5. **Fichiers matériels**.
   - [hardware/](../hardware/) ne contient que la licence : ni schémas, ni PCB, ni nomenclature, ni Gerber.
   - La licence CERN-OHL-S demande de publier les sources du matériel distribué, et un utilisateur en a besoin pour câbler et réparer.
6. **Documentation utilisateur**, à séparer du README développeur :
   - un **guide de l'installateur** par template : schéma de câblage (bornes, contacteur, limiteur), réglages de mise en service, que faire sur chaque alarme ;
   - une **fiche technique** : précision mesurée (les essais de `doc/V0.2 Measurements` permettent de chiffrer la PT100 4 fils), cadence, consommation, plages de fonctionnement ;
   - des **avertissements de sécurité** en tête : pas de limiteur homologué, contacteur obligatoire pour les fortes charges, isolation des thermocouples, USB en exploitation ;
   - une FAQ et un dépannage, à partir des messages de l'écran et du journal.
7. **Nettoyage du dépôt avant publication** :
   - ✅ `.continue/` (configuration d'un outil local) : retiré du dépôt et ignoré ;
   - le fichier de verrou `.~lock.mesures_…csv#` dans `doc/V0.2 Measurements/4 - …` ;
   - les journaux bruts de `doc/Random measurements/` ;
   - `doc/Old code/` (archive morte) : à sortir du dépôt publié, ou à mettre dans une branche.

### P2

- Une **procédure de recette matérielle** écrite : checklist sur une carte neuve (entrées, sorties, horloge, calibration, watchdog). C'est aussi la seule couverture de `SensorBoard`, `OPC.cpp`, `Storage` et de l'IHM, que les tests hôte ne voient pas.
- Un **essai d'endurance** : plusieurs semaines sur une vraie installation, avec coupures de courant, débranchements de sonde et parasites (contacteur à proximité), journal relu à la fin.

---

## 9. Feuille de route proposée

**v0.3 « terrain »** : les P0.

- ✅ Vérification du MCP23017 et récupération du bus I²C. Reste la validation sur banc de la PT1000 et du thermocouple.
- ✅ Revue, points 28 à 30 : calibration N0 et watchdog (vérifiée sur banc), compteurs écrasés après un démarrage raté, regroupement du journal.
- Pull-down des sorties (✅ ordre d'initialisation corrigé).
- Repli sur heure inconnue pour le chauffage, et autonomie du supercondensateur mesurée.
- ✅ Four : LBA par défaut et durée maximale de cuisson.
- Template PID sur relais statique.
- Documentation : contacteur, limiteur, isolation des thermocouples.

**v1.0 « publique »** : les P1.

- Version affichée, réinitialisation usine, test des sorties, diagnostic du câblage, confirmations.
- Reset 1200 bauds désactivé, boucle de redémarrages détectée.
- Export du journal et des courbes par l'USB, import de la configuration.
- PID chaud / froid, démarrage progressif, fonctions des entrées TOR, détection de mèche sèche.
- Templates : indicateur / enregistreur, cave d'affinage ou couveuse, protections du solaire.
- CI, releases `.uf2`, compilation reproductible, fichiers matériels, guide de l'installateur.

**Ensuite** : révision de la carte (§ 4), Modbus, template universel, version anglaise.
