# Comparatif OPC v0.2 / JUMO diraTRON 116

Bilan des différences matérielles et logicielles entre l'OPC v0.2 et le
régulateur compact industriel JUMO diraTRON 116 (type 702111), et pistes pour
rapprocher l'OPC d'un produit industriel.

- Référence JUMO : fiche technique 702110, V8.00 (2024-10-30)
- État OPC : commit `55ce064` (ajout de la programmation horaire)
- Date : 2026-10-01

## 1. Comparaison

### Matériel

| Domaine | JUMO diraTRON 116 | OPC v0.2 |
| --- | --- | --- |
| Entrées analogiques | 1 entrée universelle : TC (16 types), Pt100 / Pt1000 2 ou 3 fils, KTY, 0(2)–10 V, 0(4)–20 mA, potentiomètre | 3 entrées multiplexées (ADS1120) : Pt100 / Pt1000 2, 3 ou 4 fils, TC B, E, J, K, N, R, S, T. Pas de 0–10 V ni de 4–20 mA |
| Précision, cadence | 0,1 % de l'étendue (Pt100 3 fils), 50 ppm/K, une mesure toutes les 150 ms | Pt100 4 fils suréchantillonnée, probablement meilleure, mais non spécifiée. Environ 0,8 s par voie |
| Filtre d'entrée | Numérique du 2ᵉ ordre, constante de 0 à 100 s | Moyenne sur N échantillons, pas de constante de temps réglable |
| Surveillance du circuit de mesure | Rupture, court-circuit, dépassement haut et bas, polarité, NAMUR NE43 (4–20 mA). Comportement en défaut configurable | Saturation de l'ADC, valeur NaN ou infinie. Pas de seuils de rupture ou de court-circuit. En défaut, l'état sûr est toujours appliqué |
| Entrées numériques | 2 entrées pour contact sec | 2 entrées isolées (ISO1212) |
| Sorties | 2 relais 3 A / 230 V (150 000 manœuvres), 1 sortie logique 0/14 V pour SSR. Options : relais, sortie analogique 0–10 V / 4–20 mA, PhotoMOS | 2 relais, 2 PWM. Calibre des relais et étage de sortie PWM non documentés |
| Communication | RS485 Modbus-RTU esclave, micro-USB (configuration, alimenté par l'USB) | USB série, disque USB avec `config.json` en lecture seule. Modbus non implémenté |
| Logiciel PC | Configuration complète, éditeur de programmes, enregistrement de mise en service (24 h), valeurs en ligne, schéma de câblage | Aucun |
| IHM | 2 afficheurs LCD 18 segments, 4 touches, voyants sorties, rampe, minuterie, mode manuel | TFT couleur 240 × 240 et encodeur rotatif |
| Alimentation | 110–240 V AC ou 20–30 V AC/DC, 4,1 W max | Non documentée |
| Mécanique, normes | Façade 48 × 48 mm, IP65 en face avant, bornes push-in, EN 61010-1, EN 61326-1 classe A, cULus, isolation 3510 V entre entrée, relais et alimentation | Carte nue |

### Logiciel

| Fonction | JUMO | OPC |
| --- | --- | --- |
| Types de régulateur | 2 états, 3 états (chaud et froid), pas à pas (vanne motorisée), continu. Structures P, I, PD, PI, PID | `Thermostat`, `PID` en chauffage ou en refroidissement, `SolarRegulator`, `TimeSchedule` |
| Jeux de paramètres | 2, commutables | 1 |
| Consignes | 4, sélectionnées par 2 signaux binaires | 1, plus une consigne réduite par programme horaire |
| Temps mini d'enclenchement du relais (Tk) | Oui | Non (limite déjà signalée dans [Outputs/README.md](../src/Outputs/README.md)) |
| Point de travail Y0, limites de sortie | Oui | Limites de sortie seulement |
| Autotune | Par oscillation ou par réponse indicielle | Par oscillation (relais), 4 règles de calcul |
| Rampe de consigne | Montée et descente, commandée par signaux (start, pause, stop) | Montée et descente, sans commande externe |
| Programmateur de consigne | 24 segments (rampes et paliers), 4 contacts de commande | Non. Programmation hebdomadaire à la place, que le JUMO n'a pas |
| Mode manuel | Oui | Seulement marche et arrêt forcés sur `TimeSchedule` |
| Surveillance de seuils (alarmes) | 4 fonctions × 8 types : temporisation, impulsion, masquage au démarrage, mémorisation, acquittement | Aucune |
| Minuterie | Démarrage différé ou au franchissement d'une tolérance, signal de fin | Non |
| Compteur de service, heures de fonctionnement | Oui | Non |
| Logique et calcul | 4 signaux AND / OR / XOR, retard, impulsion, front. 4 formules en option | Non : la logique s'écrit en C++ dans l'installation |
| Affectation des signaux | Sélecteurs : n'importe quel signal vers n'importe quelle fonction, sans programmer | Câblage fixé à la compilation dans `Installation::begin()` |
| Linéarisation personnalisée | Table de 40 points ou polynôme d'ordre 4 | Non |
| Niveau utilisateur, verrouillage | 16 paramètres choisis, accès par niveaux | Non |
| Programmation ST (option) | Oui | Équivalent : C++ natif |

### Points forts de l'OPC à conserver

- Mesure RTD 4 fils sur 3 voies (le JUMO s'arrête au 3 fils, sur 1 voie).
- Multi-boucle : jusqu'à 16 régulateurs, par exemple solaire et appoint sur
  une même carte.
- Horloge, programmation hebdomadaire et changement d'heure automatique,
  absents du diraTRON.
- Régulateurs spécialisés : solaire avec mode vacances, psychrométrie,
  pression.
- Écran graphique couleur.
- Autotune avec 4 règles de calcul.
- Watchdog sur les deux cœurs, sauvegarde atomique, état sûr verrouillable.

## 2. Ajouts logiciels recommandés

### Priorité 1 : sécurité et robustesse

1. **Surveillance du circuit de mesure.**
   - Seuils sur la résistance : court-circuit sous environ 15 Ω, rupture
     au-dessus d'environ 400 Ω en Pt100, valeurs × 10 en Pt1000.
   - Thermocouples : sources de courant de détection de rupture intégrées à
     l'ADS1120 (0,2, 1 ou 10 µA).
   - Exposer un état de défaut typé (rupture, court-circuit, hors étendue)
     plutôt qu'un simple booléen `valid`, et l'afficher.
2. **Comportement en défaut configurable** par régulateur :
   - état sûr (comportement actuel) ;
   - sortie de repli fixe, par exemple 20 % pour une protection hors gel ;
   - maintien de la dernière sortie pendant N secondes.
3. **Temps mini de marche et d'arrêt** dans `ActuatorOnOff` et
   `TimeProportionalActuator`. Indispensable pour les compresseurs et pour la
   durée de vie des relais.
4. **Fonctions d'alarme** : une classe `LimitMonitor`, affectable à un relais.
   - Types, sur le modèle JUMO : seuil absolu haut ou bas, écart à la consigne
     haut ou bas, bande autour de la consigne.
   - Pour chaque alarme : hystérésis, temporisation, masquage au démarrage,
     mémorisation avec acquittement.
   - C'est le manque le plus visible face à un régulateur industriel.
5. **Filtre d'entrée du 1er ou 2ᵉ ordre**, avec une constante τ réglable,
   indépendant du suréchantillonnage.

### Priorité 2 : fonctions de régulation

6. **Mode manuel** sur le PID et le thermostat : sortie en %, bascule sans
   à-coup entre manuel et automatique.
7. **PID 3 états** : chaud et froid sur deux sorties, Xp2 et zone morte.
8. **PID pas à pas** : vanne 3 voies avec servomoteur, temps de course TT. Très
   demandé en chauffage hydraulique.
9. **Commutation de consigne ou de jeu de paramètres par entrée numérique.**
   Les `DigitalInput` existent déjà ; il manque un paramètre de menu du type
   « Entrée de commutation : aucune, E1 ou E2 ».
10. **Programmateur de consigne par segments** : rampes et paliers, répétition,
    reprise après coupure. Complète `TimeSchedule` pour les fours, l'étuvage et
    le séchage.
11. **Compteurs** : manœuvres par relais et heures de fonctionnement, avec seuil
    d'alerte de maintenance. Sauvegarder en flash de façon espacée, par exemple
    toutes les heures, pour limiter l'usure.
12. **Minuterie** déclenchée quand la mesure entre dans la tolérance de la
    consigne : temps de maintien, puis signal de fin.

### Priorité 3 : communication et interface

13. **Modbus-RTU esclave sur RS485.**
    - Émetteur-récepteur isolé (ADM2587E ou ISO1410, par exemple) sur un UART
      du RP2350.
    - Table de registres publiée : mesures, consignes, sorties, alarmes,
      paramètres.
    - Le même protocole sur l'USB série peut servir d'interface de
      configuration.
14. **Configuration depuis le PC** : rendre `config.json` importable (avec
    validation) ou fournir un petit outil Python.
15. **Enregistrement de mise en service** : CSV des N dernières heures en flash,
    sur le modèle de la fonction « Startup » du JUMO.
16. **Réglage direct de la consigne depuis l'écran d'accueil** à l'encodeur.
    Aujourd'hui la rotation semble ignorée à l'accueil ; sur le JUMO, les
    touches ▲ / ▼ modifient la consigne. Gain rapide.
17. **Verrouillage par code et niveau opérateur** : consigne et acquittement
    accessibles, le reste protégé.
18. **Indicateurs d'état à l'accueil** : MAN, RAMPE, ALARME, minuterie.

### Point structurant : configuration sans programmer

Le JUMO est entièrement configurable sans programmer grâce à ses
« sélecteurs ». L'OPC, lui, se configure à la compilation.

Proposition : un template `UniversalInstallation` dont tout le câblage se règle
dans le menu :

- type d'entrée par voie ;
- type de régulateur ;
- sortie affectée ;
- alarmes affectées à un relais.

Les autres templates restent la voie « experte ». L'architecture s'y prête :
`ParameterList`, les clés de configuration stables et `ProcessControl`
existent déjà.

## 3. Évolutions matérielles (prochaine révision du PCB)

| Ajout | Intérêt | Piste |
| --- | --- | --- |
| Entrée 0–10 V / 4–20 mA sur au moins une voie | Capteurs de pression, d'humidité, transmetteurs industriels | Shunt de 100 Ω, pont diviseur, protection, ADS1120 configuré en conséquence. Détection NAMUR NE43 en logiciel |
| Sortie analogique 0–10 V / 4–20 mA | Variateurs, vannes proportionnelles, gradateurs | PWM filtré et amplificateur opérationnel, ou XTR111 pour le courant |
| Sortie logique 12–14 V | Pilotage direct d'un relais statique (SSR) | Vérifier si les sorties PWM actuelles le permettent déjà |
| Alimentation 230 V AC ou 24 V AC/DC | Autonomie, installation en armoire | Module encapsulé (type IRM-05 ou HLK) ou convertisseur 24 V |
| Isolation entre la zone 230 V des relais et la zone basse tension | EN 61010-1 (le JUMO garantit 3510 V) | Lignes de fuite d'au moins 6 mm, fentes dans le PCB, protection RC ou varistance sur les contacts |
| Protections CEM | EN 61326-1 | Diodes TVS et filtres RC sur les entrées, protection ESD sur l'USB et l'encodeur |
| Bornier push-in débrochable, boîtier | Montage en tableau ou sur rail DIN | Un écran 1,54" de 240 × 240 tient dans une façade de 48 × 48 mm. Sinon format 96 × 96 ou boîtier rail DIN |

## 4. À ne pas copier

- **Langage ST et module math** : les installations C++ jouent déjà ce rôle.
- **Ethernet** : un Pico 2 W (Wi-Fi) serait plus cohérent pour une interface
  réseau.
- **Thermocouples exotiques** (C, D, A1, GOST), **KTY**, **homologations
  marines** : sans intérêt pour les usages visés.
- **Afficheur 18 segments** : le TFT est un avantage.

## 5. Feuille de route proposée

1. Surveillance de rupture et de court-circuit avec repli configurable, temps
   mini des relais, filtre d'entrée.
2. Alarmes, mode manuel, réglage de la consigne depuis l'accueil, compteurs.
3. Commutation de consigne par entrée numérique, PID 3 états puis pas à pas,
   programmateur par segments.
4. Modbus-RTU, puis le template universel configurable par menu.
5. Révision du PCB : entrée et sortie analogiques, alimentation, isolation,
   boîtier.

## Sources

- [JUMO diraTRON 104/108/116/132 – Fiche technique 702110 (V8.00, 2024)](https://www.jumo.net/attachments/JUMO/attachmentdownload?id=512212)
- [JUMO diraTRON – page produit 702110](https://www.jumo.group/us/en/products/productdetails/702110)
