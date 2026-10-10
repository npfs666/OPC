# Plan d'action : console de banc et essais automatisés

Plan établi le 10/10/2026, état du code : commit `807633b` (BMP580, lecture
ratée détectée).

Légende : ✅ fait · ⬜ à faire

---

## 1. Objectif

Faire des essais complets et rejouables de la carte, pendant le
développement et avant chaque livraison :

- un **firmware de banc** accepte des commandes sur le port série USB et y
  répond dans un format lisible par un programme ;
- des **scripts sur PC** déroulent des scénarios, vérifient les réponses et
  produisent un rapport ;
- **l'opérateur** ne fait que le montage physique (sondes ou résistances,
  câblage, sorties) et les manipulations qu'aucun logiciel ne peut faire.

Ce n'est pas une fonction du produit : le firmware livré ne contient pas la
console.

### Situation actuelle

- Le firmware ne lit jamais le port série. Le cœur 1 y écrit l'état du
  procédé à chaque cycle et le journal, pour un humain.
- Les essais de cette semaine ont tous été faits à la main : un `#define` à
  décommenter, un téléversement, une sortie série copiée-collée, une
  analyse.
- Ils ont pourtant trouvé de vrais défauts :
  - IRQ DRDY parasites (jusqu'à -6 °C) ;
  - BMP580 muet affiché comme valide ;
  - faux défaut de câblage en 3 fils, dû en fait à un contact.

  Rejouer ces essais à chaque version est exactement ce qu'il faut
  automatiser.

---

## 2. Architecture proposée

### Firmware de banc

- Un second environnement PlatformIO, `opc_v02_banc`, compile la console
  avec un drapeau (`-D OPC_BENCH_CONSOLE`).
- `default_envs = opc_v02` garde `pio run` sur le firmware livré.
- Le code de la console est dans des fichiers à part (`src/Bench/`),
  compilés seulement avec ce drapeau. Le firmware livré reste identique, à
  part quelques points d'accroche sous `#ifdef`.
- Les deux environnements sont compilés à chaque vérification, pour que le
  banc ne prenne pas de retard sur le produit.

### Répartition entre les cœurs

- **Cœur 1** : il possède déjà le port série. Il lit les lignes, analyse
  les commandes et écrit les réponses. Les lectures d'état passent par le
  `ProcessSnapshot`, comme l'écran.
- **Cœur 0** : il exécute les commandes qui touchent au contrôle
  (réglages, actions, injection de défauts), reçues par la FIFO comme celles
  du menu. Il ne fait aucune écriture série : une écriture bloquée
  retarderait l'acquisition (voir `Plan_acquisition_hors_ISR.md`, § 7).
- **Changement de réglages** : par le même chemin que le menu (capture des
  brouillons, validation, application sur le cœur 0, sauvegarde). Le banc
  vérifie ainsi le vrai chemin, pas un raccourci.

### Protocole

- Une commande par ligne, avec un identifiant :
  `#12 set input1.wiring three_wire`.
- Chaque réponse porte le même identifiant et commence par un marqueur
  (`@`), suivi d'un objet JSON sur une ligne :
  `@{"id":12,"ok":true}`.
- **Notifications** sans demande, sur le même modèle : fin de cycle,
  événement du journal, changement d'état d'une mesure. Un script peut ainsi
  attendre une condition au lieu d'interroger la carte.
- La sortie lisible actuelle reste, sans marqueur : un humain peut suivre
  en même temps, et un script ne lit que les lignes `@`.

### Côté PC

- Python, pyserial et pytest, dans `test/bench/` :
  - un client (connexion, envoi, attente d'une réponse ou d'une
    notification, reconnexion après un redémarrage) ;
  - des scénarios sous forme de tests pytest ;
  - un rapport par campagne : version, montage, résultats, mesures.
- Le port n'est jamais ouvert à 1 200 bauds : cela ferait passer la carte en
  BOOTSEL.
- Le téléversement libère le port, puis la campagne reprend.

---

## 3. Commandes envisagées

| Groupe | Commandes | Effet |
|---|---|---|
| Identité | `version`, `help` | Commit, date de compilation, installation, liste des commandes |
| État | `status`, `measure <clé>`, `outputs`, `events [n]`, `diag` | Mesures et états, sorties, journal, compteurs de diagnostic |
| Réglages | `get <clé>`, `set <clé> <valeur>`, `apply`, `defaults` | Comme le menu, avec la même validation |
| Actions | `action <nom>` | Acquitter, RAZ compteurs, calibrations, etc. |
| Horloge | `time <date heure>`, `time` | Régler et lire l'heure (passages heure d'été et d'hiver) |
| Injection | `inject mcp-reset`, `inject adc-mute <ms>`, `inject hang` | Reset du MCP, ADC muet, blocage du cœur 0 (watchdog) |
| Simulation | `simulate <mesure> <valeur>`, `simulate <mesure> <état>`, `simulate off` | Remplace une mesure, pour essayer la régulation sans four ni chambre froide |
| Capture | `capture <entrée>` | Conversions brutes d'une acquisition (comme pour le 3 fils) |
| Système | `reboot`, `save` | Redémarrage, sauvegarde immédiate |

Les clés sont celles de `config.json` (propriétaire, puis paramètre) : elles
sont déjà stables et uniques.

---

## 4. Scénarios d'une campagne

1. **Acquisition** : chaque montage (2, 3 et 4 fils, PT100, PT1000,
   thermocouple) sur des résistances connues. Valeur dans la tolérance,
   bruit (écart type), durée du cycle.
2. **Défauts d'acquisition** :
   - par injection : reset du MCP, ADC muet ;
   - avec l'opérateur : SDA à la masse, sonde débranchée, lecteur USB monté
     et démonté.

   Pour chacun, on vérifie l'état des mesures, les événements, le retour à
   la normale et l'absence de redémarrage.
3. **Réglages** :
   - chaque réglage modifié par la console ;
   - persistance après un redémarrage ;
   - restauration tolérante (fichier corrompu, réglage hors plage).
4. **Régulation**, avec des mesures simulées :
   - hystérésis du thermostat, PID, alarmes, LBA ;
   - état sûr sur mesure invalide, repli `Maintien` ;
   - temps mini des relais.
5. **Horloge** : programmations horaires, passages d'heure d'été et
   d'hiver, heure inconnue.
6. **Robustesse** :
   - blocage du cœur 0 : redémarrage par le watchdog, cause au journal ;
   - compteurs et journal conservés après un redémarrage.
7. **Endurance** : 24 h de fonctionnement, statistiques des mesures et des
   défauts.

### Vérifier les sorties

L'état des sorties se lit dans le snapshot, mais cela ne prouve pas qu'un
relais a vraiment commuté. Sur le banc, un câblage de rebouclage relierait
une sortie à une des deux entrées TOR : le script commanderait la sortie et
lirait l'entrée.

---

## 5. Décisions à prendre

| # | Sujet | Recommandation | Autre choix |
|---|---|---|---|
| 1 | Variante | Environnement `opc_v02_banc`, compilé avec un drapeau ; console absente du firmware livré | Console présente partout, activée au menu |
| 2 | Installation essayée | D'abord une installation de banc qui utilise tous les blocs, puis chaque template avec la console (installation choisie par un drapeau de compilation) | Seulement les templates, un par un |
| 3 | Format des réponses | JSON sur une ligne, après un marqueur `@`, avec un identifiant de commande | Texte libre, plus simple à lire, plus fragile à analyser |
| 4 | Simulation des mesures | Au niveau de la mesure (valeur et état), pour la régulation ; les injections matérielles servent aux essais d'acquisition | Au niveau de l'ADC (points bruts), plus réaliste, mais plus lourd |
| 5 | Preuve des sorties | Rebouclage d'une sortie sur une entrée TOR | Snapshot seulement |
| 6 | Outils PC | Python, pyserial et pytest | Lanceur maison |
| 7 | Rapport | Markdown dans `test/bench/reports/`, avec la version et le montage | Page HTML partagée |

---

## 6. Étapes

1. ⬜ **Console minimale, en lecture seule.**
   - Environnement de banc, protocole, commandes `version`, `help`,
     `status`, `get`, `events`, `diag`.
   - Client Python et un premier scénario : acquisition 4 fils.
2. ⬜ **Réglages et actions** : `set`, `apply`, `action`, `time`, `reboot`.
3. ⬜ **Injection de défauts et capture** : `inject`, `capture`. Les
   `#define` d'essai (`OPC_TEST_MCP_FAULT`, `PRINT_CONVERSION_TIME`)
   disparaissent au profit de la console.
4. ⬜ **Simulation des mesures et scénarios de régulation.**
5. ⬜ **Campagne complète et rapport.** C'est l'étape 5 de
   `Plan_acquisition_hors_ISR.md`, faite avec la console.

---

## 7. Risques et parades

| Risque | Parade |
|---|---|
| Le firmware de banc diverge du firmware livré | Code de la console à part, deux environnements compilés à chaque vérification, aucun changement de comportement hors `#ifdef` |
| La console ralentit l'écran ou l'acquisition | Lecture non bloquante sur le cœur 1, exécution par la FIFO, aucune écriture série sur le cœur 0 |
| Une commande contourne une sécurité du produit | Réglages par le chemin du menu ; la simulation est explicite, et elle s'arrête au redémarrage |
| Lignes humaines et lignes `@` mélangées | Une ligne `@` est écrite d'un bloc, avec `bufferedOutput` |
| Port pris par un autre programme (moniteur de VS Code) | Le client signale le port occupé au lieu de lire des lignes incomplètes |
