# Comparatif OPC v0.2 / JUMO diraTRON 116 et régulateurs industriels

Bilan des différences entre l'OPC v0.2, le régulateur compact JUMO
diraTRON 116 (type 702111) et, plus largement, les régulateurs de process
industriels. Il sert de feuille de route pour rapprocher l'OPC d'un produit
industriel.

- Référence JUMO : fiche technique 702110, V8.00 (2024-10-30)
- État OPC : commit `6af35df` (compteurs d'usure des relais)
- Première version : 2026-10-01 (commit `55ce064`)
- Mise à jour : 2026-10-03

Légende : ✅ fait · 🟡 partiel · ⬜ à faire · ➖ volontairement non retenu.

## 1. Comparaison avec le JUMO diraTRON 116

### Matériel

| Domaine | JUMO diraTRON 116 | OPC v0.2 | État |
| --- | --- | --- | --- |
| Entrées analogiques | 1 entrée universelle : TC (16 types), Pt100 / Pt1000 2 ou 3 fils, KTY, 0(2)–10 V, 0(4)–20 mA, potentiomètre | 3 entrées (ADS1120) : Pt100 / Pt1000 2, 3 ou 4 fils, TC B, E, J, K, N, R, S, T | 🟡 pas de 0–10 V ni de 4–20 mA |
| Étendue RTD | −200 à 850 °C | Plage précise −200 à environ 280 °C (gain 8) ou étendue −200 à 850 °C (gain 4), au choix dans le menu | ✅ |
| Précision, cadence | 0,1 % de l'étendue, 50 ppm/K, une mesure toutes les 150 ms | Pt100 4 fils suréchantillonnée, non spécifiée. Environ 0,8 s par voie | 🟡 précision à caractériser et publier |
| Filtre d'entrée | Numérique du 2ᵉ ordre, 0 à 100 s | Identique, appliqué à la température calculée | ✅ |
| Surveillance de la sonde | Rupture, court-circuit, hors étendue, polarité, NAMUR NE43 | Rupture, court-circuit, hors étendue (haut et bas), câblage inversé. Thermocouple ouvert détecté par la polarisation 1 MΩ | ✅ (NE43 avec l'entrée 4–20 mA) |
| Entrées numériques | 2 pour contact sec | 2 isolées (ISO1212) | ✅ |
| Sorties | 2 relais 3 A / 230 V, sortie logique 0/14 V pour SSR. Options : relais, analogique 0–10 V / 4–20 mA, PhotoMOS | 2 relais, 2 PWM | 🟡 pas de sortie analogique ni de sortie SSR caractérisée |
| Communication | RS485 Modbus-RTU, micro-USB de configuration | USB série, disque USB `config.json` en lecture seule | ⬜ Modbus |
| Logiciel PC | Configuration, éditeur de programmes, enregistrement de mise en service | Aucun | ⬜ |
| IHM | 2 LCD 18 segments, 4 touches | TFT couleur 240 × 240, encodeur | ✅ plus riche |
| Alimentation | 110–240 V AC ou 20–30 V AC/DC | Non documentée | ⬜ |
| Mécanique, normes | Façade 48 × 48 mm IP65, push-in, EN 61010-1, EN 61326-1, cULus, isolation 3510 V | Carte nue | ⬜ |

### Logiciel

| Fonction | JUMO | OPC | État |
| --- | --- | --- | --- |
| Types de régulateur | 2 états, 3 états (chaud / froid), pas à pas, continu | Thermostat, PID chauffage ou refroidissement, solaire, programmation horaire | 🟡 pas de 3 états ni de pas à pas |
| Comportement sur défaut de sonde | Configurable, dont sortie forcée | `Sécurité` (défaut) ou `Maintien` limité dans le temps. Pas de sortie forcée : sans mesure, l'état sûr reste la règle | ✅ choix volontaire |
| Temps mini du relais (Tk) | Oui | `Marche mini`, `Arrêt mini` par relais, `Impulsion mini` de l'actionneur temporel | ✅ |
| Autotune | Oscillation ou réponse indicielle | Oscillation, 4 règles de calcul | 🟡 pas de réponse indicielle |
| Rampe de consigne | Montée et descente, commandée par signaux | Montée et descente | 🟡 pas de commande externe |
| Mode manuel | Oui | PID (sortie en %) et thermostat (Marche / Arrêt), sans à-coup dans les deux sens, non conservé au redémarrage | ✅ |
| Réglage de la consigne en façade | ▲ / ▼ | Encodeur à l'accueil, accélération, validé par un clic | ✅ |
| Surveillance de seuils (alarmes) | 4 fonctions × 8 types | 8 alarmes × 5 types (Max, Min, Écart haut, Écart bas, Hors bande), temporisation, masquage au démarrage, mémorisation, acquittement par menu ou entrée | ✅ |
| Compteur de service | Manœuvres ou durée, heures de l'appareil | Manœuvres et heures en marche par relais, heures de la carte, seuil d'entretien | ✅ |
| Jeux de paramètres | 2 | 1 | ⬜ |
| Consignes commutables | 4, par entrées numériques | 1, plus consigne réduite par programme horaire | ⬜ |
| Programmateur de consigne | 24 segments, 4 contacts | Non (programmation hebdomadaire à la place) | ⬜ |
| Minuterie | Oui | Non | ⬜ |
| Logique et calcul | 4 signaux AND / OR / XOR, retard, impulsion, front ; 4 formules | Non (en C++ dans l'installation) | ⬜ |
| Affectation des signaux sans programmer | Sélecteurs | Câblage dans `Installation::begin()` | ⬜ voir § 4 |
| Linéarisation personnalisée | 40 points ou polynôme d'ordre 4 | Non | ⬜ |
| Niveau utilisateur, verrouillage | Oui | Non | ⬜ |
| Programmation ST | Option | C++ natif | ➖ |

### Points forts de l'OPC

- Mesure RTD 4 fils sur 3 voies (le JUMO s'arrête au 3 fils, sur 1 voie).
- Plusieurs boucles de régulation sur une même carte.
- Horloge, programmation hebdomadaire, changement d'heure automatique.
- Régulateurs spécialisés : solaire avec mode vacances, psychrométrie,
  pression.
- Écran graphique couleur, états en couleur, bandeau d'alarme.
- Journal série des changements d'état (mesures, alarmes, entretien).
- Heures en marche par relais.
- Réglages de conduite (consignes, mode manuel) appliqués sans arrêter la
  régulation.
- Watchdog sur les deux cœurs, sauvegarde atomique de la configuration.
- Open source, extensible en C++.

## 2. Au-delà du JUMO : les régulateurs industriels

Fonctions courantes sur les régulateurs de process (Eurotherm, Omron, Watlow,
West…), classées selon l'intérêt pour l'OPC.

### Recommandé

Les quatre premières sont du logiciel seul, réalisable sur la carte actuelle.

| Fonction | Intérêt | Effort |
| --- | --- | --- |
| ✅ **Alarme de boucle ouverte** (loop break, LBA) : sortie en butée depuis T s sans variation de la mesure | Très forte valeur de sécurité : résistance grillée, contacteur collé ou ouvert, sonde sortie du process. Couvre les pannes que la surveillance de sonde ne voit pas | Fait : `LoopBreakAlarm`, mise en sécurité optionnelle |
| **Événements horodatés et conservés** (les N derniers en flash, heure réelle) | Diagnostic après coup. L'horloge et le journal existent déjà | Faible |
| **Démarrage progressif** (sortie limitée pendant X min ou sous un seuil de mesure) | Résistances chauffantes à sécher, limitation des appels de courant | Faible |
| **Verrouillage par code, niveau opérateur** | Indispensable dès qu'un tiers a accès à l'appareil | Faible à moyen |
| **Enregistrement des courbes** (mesure, consigne, sortie en CSV, récupéré par la clé USB) | Un vrai avantage possible de l'OPC : le stockage USB existe | Moyen |
| **Modbus RTU** : d'abord sur l'USB série (sans matériel), puis sur RS485 | Supervision, automate. Le standard attendu | Moyen |
| **Programmateur de consigne avec attente de maintien** (holdback : le palier commence quand la mesure a rejoint la consigne) | Fours, étuvage, séchage | Moyen |
| **PID chaud / froid** avec zone morte | Chambres climatiques, bains | Moyen |
| **Import et sauvegarde de la configuration par USB** | Mise en service en série, sauvegarde avant intervention | Moyen |
| **Fonctions des entrées numériques choisies dans le menu** : marche / arrêt à distance, deuxième consigne, manuel, acquittement, défaut externe (verrouillage) | Câbler un thermostat d'ambiance, un contact de porte, un défaut de pompe… sans programmer | Moyen |
| **Vanne 3 points** (pas à pas, deux relais, temps de course) | Chauffage hydraulique | Moyen |

### Plus tard

- Deux jeux de paramètres ou réglages selon la consigne (process très non
  linéaire).
- Autotune automatique au changement de consigne : complexe, l'autotune
  manuel suffit souvent.
- Cascade, régulation de rapport, anticipation : faisable en C++ dans une
  installation dédiée le jour où le besoin existe.
- Consigne déportée 4–20 mA, recopie de la mesure en sortie analogique,
  alarme de rupture de résistance par transformateur de courant : attendent
  les entrées et sorties analogiques.
- Mise à jour du firmware depuis le menu (déjà possible par USB avec
  picotool).

### Volontairement non retenu

- **Limiteur de sécurité homologué** (STB, EN 14597) : exige une conception
  matérielle indépendante et certifiée. L'OPC reste un régulateur ; la
  sécurité thermique passe par un limiteur externe.
- **Sortie forcée sur défaut de sonde** : sans mesure, l'état sûr reste la
  règle (une marche forcée peut faire bouillir de l'eau).
- **Bus de terrain lourds** (Profinet, EtherCAT) : disproportionné, Modbus
  suffit.
- **Logique floue** et arguments commerciaux du même ordre : aucun gain réel
  face à un PID bien réglé.
- **Programmation ST, homologations marines, thermocouples exotiques (C, D,
  A1, GOST), KTY** : hors cible.
- **Ethernet** : un Pico 2 W (Wi-Fi) serait plus cohérent pour une interface
  réseau.
- **Afficheur 18 segments** : le TFT est un avantage.

## 3. Évolutions matérielles (prochaine révision du PCB)

| Ajout | Intérêt | Piste |
| --- | --- | --- |
| Entrée 0–10 V / 4–20 mA sur au moins une voie | Capteurs de pression, d'humidité, transmetteurs | Shunt de 100 Ω, pont diviseur, protection ; NAMUR NE43 en logiciel |
| Sortie analogique 0–10 V / 4–20 mA | Variateurs, vannes proportionnelles, recopie | PWM filtré et amplificateur opérationnel, ou XTR111 pour le courant |
| Sortie logique 12–14 V | Relais statique (SSR) | Vérifier si les sorties PWM actuelles conviennent |
| RS485 isolé | Modbus RTU | ADM2587E ou ISO1410 sur un UART du RP2350 |
| Alimentation 230 V AC ou 24 V AC/DC | Installation en armoire | Module encapsulé (IRM-05, HLK) ou convertisseur 24 V |
| Isolation zone 230 V / basse tension | EN 61010-1 | Lignes de fuite ≥ 6 mm, fentes, RC ou varistance sur les contacts |
| Protections CEM | EN 61326-1 | TVS et filtres RC sur les entrées, ESD sur l'USB et l'encodeur |
| Bornier push-in débrochable, boîtier | Tableau ou rail DIN | Écran 1,54" dans une façade 48 × 48 mm, ou format 96 × 96 / rail DIN |
| Transformateur de courant | Alarme de rupture de résistance | Une entrée analogique dédiée |

## 4. Le principal écart restant

Un régulateur industriel se **configure entièrement depuis sa façade**, sans
programmer. L'OPC demande encore d'écrire une installation en C++ et de la
compiler.

L'étape qui rapprocherait le plus l'OPC d'un produit est un **template
universel** configurable par le menu : type d'entrée, type de régulateur,
affectation des sorties, alarmes, fonctions des entrées numériques. Les
briques nécessaires existent maintenant (diagnostic des sondes, alarmes, mode
manuel, réglages de conduite, compteurs) ; il en regrouperait plusieurs
points du § 2.

## 5. Feuille de route

### Réalisé

**Point 1 — sécurité et robustesse**

- Diagnostic des sondes (`MeasurementStatus`) : rupture, court-circuit, hors
  étendue, affichés en couleur et journalisés.
- Plage RTD précise ou étendue, diagnostic rupture / dépassement par une
  conversion au gain large.
- Temps mini des relais, impulsion mini de l'actionneur temporel.
- Filtre d'entrée du 2ᵉ ordre.
- Repli sur défaut de sonde : sécurité ou maintien limité.
- Thermostat : départ à l'arrêt dans la bande d'hystérésis (plus de commande
  invalide après le menu).

**Point 2 — fonctions de régulation et d'exploitation**

- Consigne réglée depuis l'accueil, à l'encodeur.
- Alarmes de seuil (`LimitAlarm`), bandeau et journal.
- Mode manuel du PID et du thermostat.
- Réglages de conduite appliqués sans état sûr (`Parameter::live`).
- Compteurs d'entretien, `/counters.json`.

**Écrans d'accueil** refaits (thermostat, PID, solaire) : champs à largeur
fixe, mesure en grand, jauge des seuils du thermostat.

**Point 3 — sécurité de la boucle**

- Alarme de boucle ouverte (`LoopBreakAlarm`) : butées haute et basse, chaud
  et froid, temps de détection automatique (2 × Ti), mise en sécurité du
  régulateur jusqu'à l'acquittement. Base commune `Alarm` pour les alarmes.

### Proposé

1. Événements horodatés conservés, démarrage progressif.
2. Verrouillage et niveau opérateur, fonctions des entrées numériques dans le
   menu.
3. Enregistrement des courbes, Modbus sur l'USB série.
4. Template universel configurable par le menu.
5. Selon les applications : programmateur de consigne, PID chaud / froid,
   vanne 3 points.
6. Révision matérielle (§ 3).

## Sources

- [JUMO diraTRON 104/108/116/132 – Fiche technique 702110 (V8.00, 2024)](https://www.jumo.net/attachments/JUMO/attachmentdownload?id=512212)
- [JUMO diraTRON – page produit 702110](https://www.jumo.group/us/en/products/productdetails/702110)
