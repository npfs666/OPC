# Revue du code OPC v0.2

Revue initiale du 28/09/2026. Statuts mis à jour le 07/10/2026. Relecture complète du code le 12/10/2026 : résumé mis à jour, points 28 à 35 ajoutés, compléments aux points 13, 16 et 20.

Légende : ✅ corrigé · ⬜ à faire

---

## 1. Compréhension (résumé)

- **Cible** : Pico 2 (RP2350), avec le cœur Arduino Earle Philhower.
  - **Cœur 0** : acquisition, mesures, régulation, sorties et stockage (`setup` / `loop`).
  - **Cœur 1** : port série, écran ST7789, encodeur et menu (`setup1` / `loop1`).
  - Les deux cœurs échangent des messages par la FIFO (`InterCoreMessages.h`) : barrière de démarrage, capture et application des réglages, consigne réglée à l'accueil, horloge. Ils partagent les données sous le mutex `processDataMutex`, et l'écran ne lit que la copie `ProcessSnapshot`.
- **Chaîne de traitement** : `SensorBoard`, puis `Measurement`, `Regulator`, la glue de l'installation (`ProcessLogic`, `LogicCommand`), `Actuator` et `Output`, sous l'orchestration de `ProcessControl`.
- **Acquisition**
  - L'ISR DRDY de l'ADS1120 (`SensorBoard::adcInterrupt`) lit seulement la conversion, si DRDY est encore bas, et la passe à `AcquisitionSequencer`.
  - Le séquencement se fait dans `loop()` (`OPC::acquisitionPoll()`) : routage par le MCP23017 (I²C Wire1), vérifié par relecture avant et après chaque entrée, *chopping* des IDAC en 3 fils, diagnostic d'étendue, puis température de l'ADC (jonction froide) en fin de cycle.
  - Les défauts d'entrée (rupture, court-circuit, hors étendue, routage, ADC muet) sont portés par `MeasurementStatus` et journalisés.
- **Blocs** : thermostat, PID (Kp / Ti / Td, autotune par relais), comparateur, temporisation, programmation horaire, consigne programmée, rampe, programme paliers-rampes, loi d'eau, actionneurs tout-ou-rien, PWM, temporel et vanne 3 points, alarmes (seuil, boucle ouverte, condition). Mode manuel, inhibition par la glue, verrouillage par alarme et repli « Maintien » sur défaut de mesure.
- **Paramètres et persistance**
  - Chaque objet s'enregistre dans `ParameterList` par un `ParameterOwner` (catégorie et propriétaire).
  - `MenuBuilder` et `ArduinoMenuUI` génèrent le menu, et `ParameterEditor` gère les brouillons : capture, validation et application sur le cœur 0.
  - `Storage` écrit `/config.json` (installation) et `/board.json` (carte) de façon atomique, restaure en tolérant les entrées refusées, et exporte la configuration en lecture seule par USB. Compteurs d'entretien dans `/counters.json`, journal dans `/events.csv`.
- **Sécurités**
  - États sûrs des sorties sur toute commande invalide.
  - Timeout des mesures.
  - Watchdog à deux cœurs : le cœur 0 nourrit le watchdog seulement si le cœur 1 a aussi progressé.
  - Mise en sécurité des sorties pendant l'application des réglages, sauf pour les réglages de conduite (`Parameter::live`).
  - Journal des événements en RAM, écrit en flash par lots.
- **Tests** : 281 tests hôte (doctest), dont des templates entiers sur `TemplateBench`. `SensorBoard`, le pilote ADS1120, `OPC`, `Storage` et l'interface ne sont vérifiés que par la compilation du firmware et sur banc.

L'architecture est propre et défensive : validations nombreuses, écriture atomique des fichiers par *rename*, handshake FIFO entre cœurs, acquisition hors ISR avec vérification du routage. La relecture du 12/10/2026 trouve deux défauts réels (points 28 et 29) ; le reste est mineur.

---

## 2. Erreurs et risques relevés (par priorité)

### A. À corriger rapidement (bugs réels ou restes de test)

1. ✅ **Test I²C laissé actif** (`OPC::controlPoll`).
   - Le DS3231 était lu toutes les 10 ms au lieu de 1000 ms.
2. ✅ **`Serial.printf` sous mutex** (`OPC::handleUIMessage`, `PrintDataAvailable`).
   - Le cœur 1 écrivait sur l'USB en tenant `processDataMutex`, ce qui pouvait bloquer le cœur 0.
   - L'écriture passe maintenant par `bufferedOutput`.
3. ✅ **Deux relais pouvaient piloter la même broche**.
   - Le contrôle des doublons est maintenant commun : `Output::pinIsUnique`, appelé par `RelayOutput` et `PWMOutput`.
   - Il s'applique aux réglages faits dans le menu et au `config.json` restauré.
   - Un doublon écrit directement dans le code d'une installation n'est pas contrôlé : c'est une erreur de l'utilisateur.
4. ✅ **Échec d'initialisation du MCP23017 ignoré**.
   - `AnalogMux::begin()` et `SensorBoard::init()` renvoient maintenant `bool`.
   - `OPC::initMeasurements()` refuse de démarrer si le MCP est absent.
   - ✅ Les erreurs I²C du MCP pendant l'acquisition sont traitées : routage relu avant et après chaque acquisition, nouvelle tentative, réinitialisation, entrée `Invalid` et journal (`AnalogMux::apply()` / `confirm()`). Voir [Plan_acquisition_hors_ISR.md](Plan_acquisition_hors_ISR.md), étape 3.
5. ✅ **Démarrage raté = écran noir**.
   - Un échec de démarrage envoie `StartupFailed`, et le cœur UI affiche un écran « ERREUR DEMARRAGE » avec la cause (`StartupStatus.h`).
   - Le message est répété toutes les 5 s sur le port série.
   - Les installations précisent leur cause avec `Installation::fail("...")`, utilisé dans les templates PID, Thermostat et Solaire et dans l'exemple minimal.
   - Les avertissements non bloquants ne sont pas traités, par choix.
6. ✅ **`Resistance` était toujours valide**.
   - La validité suit maintenant la valeur calculée, et la mesure est invalide si `begin()` n'a pas été appelé.
   - `computeResistance` renvoie NaN pour une entrée thermocouple et pour une résistance ≤ 0.
   - Tests hôte ajoutés, et le test PT1000 a été mis à jour pour Callendar–Van Dusen.
7. ✅ **Constante `DATARATE_1000_SPS`** : elle valait 0x05 au lieu de 0x06.
26. ✅ **IRQ DRDY parasites accumulées comme échantillons** (`SensorBoard::adcInterrupt`). Ajouté le 10/10/2026, voir [Plan_acquisition_hors_ISR.md](Plan_acquisition_hors_ISR.md), étape 0.
    - Quand l'ISR est retardée (écriture flash, pile USB : démarrage, lecteur USB monté par le PC), une IRQ peut suivre la précédente sans nouvelle conversion.
    - DRDY est alors déjà haut, et l'ADS1120 renvoie `0xFFFF` (-1), accumulé comme un échantillon : -1/800 sur la moyenne par parasite.
    - Mesuré : jusqu'à 31 parasites par cycle, soit -5 à -6 °C sur deux PT100. L'état restait `Ok`, car la valeur n'est pas saturée.
    - Les mêmes lectures pouvaient fausser la température ADC (jonction froide) et le diagnostic d'étendue, et réduire le temps d'établissement.
    - Correctif : l'ISR ignore l'IRQ si DRDY est déjà haut à l'entrée. La conversion suivante redonne un front.
    - Vérifié sur banc, avec montages et démontages répétés du lecteur USB : jusqu'à 52 IRQ ignorées par cycle, et des mesures restées dans le bruit (0,03 point ADC).

27. ✅ **BMP580 muet affiché comme valide** (`PressureBMP580`). Ajouté le 10/10/2026, trouvé par l'essai SDA à la masse ([Plan_acquisition_hors_ISR.md](Plan_acquisition_hors_ISR.md), étape 4).
    - `Adafruit_BMP5xx::readPressure()` ignore l'échec de `performReading()` et renvoie la dernière pression lue.
    - Bus bloqué ou capteur muet : la pression restait figée et valide, et l'humidité psychrométrique en dépend.
    - Correctif : lecture par `performReading()`, mesure invalide en cas d'échec. `pressureSeaLevel()` fait aussi une seule lecture pour la pression et la température.
    - Vérifié sur banc (SDA à la masse) : `BMP580 : ERREUR` pendant le court-circuit, puis `BMP580 : OK`.
28. ✅ **Calibration N0 proche du délai du watchdog** (`SensorBoard::calibrateZero`). Ajouté le 12/10/2026, vérifié sur banc le 16/10/2026.
    - Depuis la mesure de R2f (commit 69976a4), « Mesurer N0 » fait deux séries de 15 + 3 200 conversions uniques à 1000 SPS : environ 1,07 ms chacune, soit près de 7 s.
    - Tout se passe dans `handleControlMessage()` sur le cœur 0, suivi de la sauvegarde (`/board.json` puis `/config.json`). Le watchdog (8 s) n'est nourri qu'au tour de `loop()` suivant : la marge est de l'ordre de la seconde, ou moins.
    - Un reset pendant la calibration ne casse rien (sorties en sécurité, ancienne calibration conservée), mais la calibration est perdue sans explication.
    - Sur banc le 16/10/2026 : plusieurs calibrations, aucun reset par le watchdog. Le total (calibration et sauvegarde) reste donc sous 8 s ; la marge exacte n'est pas mesurée, et seule la sauvegarde LittleFS peut varier. Laissé en l'état, puisqu'un reset serait sans danger.
    - Si un reset apparaît : raccourcir la série 2 fils (1 point ADC vaut environ 6 mΩ en PT100 : 16 × 50 conversions devraient suffire pour R2f), soit environ 2,5 s de gagnées. Éviter de nourrir le watchdog dans `readCalibrationSamples()`, qui affaiblirait la garantie « les deux cœurs progressent ».
29. ✅ **Démarrage raté : les compteurs et le journal sauvegardés peuvent être écrasés** (`OPC::controlPoll`). Ajouté le 12/10/2026.
    - Après un échec de `initMeasurements()`, `loop()` continue d'appeler `controlPoll()`.
    - Si l'échec vient après `storage.begin()` (seul cas : `StartupError::Outputs`, une sortie PWM qui ne démarre pas), `restoreCounters()` et `restoreEvents()` n'ont pas été appelés.
    - Une heure plus tard, `saveCounters()` réécrit `/counters.json` avec des compteurs de relais à zéro et les heures carte comptées depuis ce démarrage. Après 15 min, une heure inconnue (« Heure perdue ») suffit pour que `saveEvents()` remplace `/events.csv` par ce seul événement.
    - Corrigé le 16/10/2026 : les sauvegardes des compteurs et du journal attendent `controlCycleStarted`. Le reste de `controlPoll()` (entrées, disque USB, horloge) continue de tourner sur l'écran d'erreur.

### B. Risques de conception (à planifier)

8. ✅ **ISR DRDY longue** (`SensorBoard::adcInterrupt`).
   - ✅ Les `delay(2)` de `ADS1120::sendCommand` ont été supprimés.
   - ✅ Wire1 passé à 400 kHz (`OPC.cpp`).
   - ✅ La température ADC n'est plus attendue dans l'ISR : elle devient la dernière étape du cycle (`startAdcTemperature`), lue à l'IRQ DRDY suivante.
   - ✅ Le séquencement est sorti de l'ISR (`AcquisitionSequencer`, testé sur PC) : l'ISR lit l'échantillon, et une machine d'états dans `loop()` gère le routage. `SharedBusGuard` est supprimé. Voir [Plan_acquisition_hors_ISR.md](Plan_acquisition_hors_ISR.md), étape 2.
9. ✅ **Précision PT100** : la conversion passe maintenant par Callendar–Van Dusen avec Newton-Raphson (`PT100::getResistanceToTemperatureNewton`).
   - Mesure faite sur l'ancienne table : erreur de 15 mK à 100 °C, 71 mK à 266 °C, et aucune mesure au-delà de 266 °C.
   - ✅ Test hôte ajouté sur les points IEC 60751 (`testPT100CallendarVanDusen`).
10. ✅ **Deux formules psychrométriques différentes** (`Psychrometrics.cpp`).
    - `relativeHumidity()` utilisait Magnus 17,2694/238,3 avec un γ fixe, `getRH()` 17,27/237,3 avec un A calculé (la seule qui servait), et `dewPoint()` 17,27/237,7.
    - ✅ Une seule formule de Magnus (WMO, coefficients de Sonntag 17,62/243,12 sur l'eau, 22,46/272,62 sur la glace) ; `getRH()` est supprimée. `dewPoint()` en est l'inverse exact.
    - ✅ Formule sur glace quand le bulbe humide passe sous 0 °C (`Bulbe < 0°C`, `Glace` par défaut), et coefficient psychrométrique réglable (`Coef. psychro.`, 6,53·10⁻⁴/°C par défaut, ventilé). Écart avec l'ancien calcul au-dessus de 0 °C : −0,1 à −0,3 % HR.
    - ✅ `constrain(0,100)` ne masque plus les incohérences : bulbe humide plus chaud que le sec de plus de 0,5 K → `Invalid`, humidité sous −2 % → `UnderRange`. Tests dans `test_psychrometrics.cpp`.
11. ✅ **Limites du PID**.
    - Les gains sont passés en forme standard Kp / Ti / Td (secondes), avec des plages Ti ≤ 100 000 s et Td ≤ 10 000 s.
    - La consigne va de −50 à 250 °C par défaut, et chaque installation peut la changer avec `setSetpointLimits()`.
    - Reprise sans à-coup : l'intégrale est conservée à la reprise, sur une mesure invalide et au changement de gains.
    - Ajouts : filtre sur la dérivée (Td/10), et anti-windup corrigé (la sortie atteint bien la saturation).
    - Autotune : choix de la règle de calcul, Tyreus-Luyben par défaut.
12. ✅ **Restauration « tout ou rien »** (`Storage::restore`, `ParameterJson`).
    - ✅ Un paramètre hors plage revient seul à sa valeur par défaut ; un propriétaire refusé par la validation croisée revient entièrement par défaut ; le reste du fichier est gardé (`ParameterEditor::keepValidDrafts()`).
    - ✅ Chaque groupe remis par défaut est inscrit au journal (`Par défaut : <groupe>`), comme un fichier non repris (`Config. non reprise : défauts`).
    - ✅ Une entrée inconnue, en double ou illisible (type changé) n'invalide plus le fichier : elle est ignorée, ou son réglage revient seul par défaut. Une liste d'options complétée garde la valeur.
    - ✅ Les réglages de la carte (calibrations Rref, N0, C.J., fuseau horaire ; `ParameterOwner::board`) sont dans `/board.json`, indépendant de l'installation. Migration automatique depuis `config.json`.
13. ⬜ **Interruptions coupées pendant toute la sauvegarde** (`Storage::save`, `InterruptGuard`).
    - Cela représente des centaines de ms sans IRQ sur le cœur 0.
    - Le garde empêche la pile USB (IRQ du cœur 0, `tud_task()`) de lire LittleFS pour le lecteur `SingleFileDrive` pendant une écriture. Correction possible : tenir `USB.mutex` à la place, ce qui laisse passer l'IRQ DRDY et les timers.
    - Impact jugé faible le 12/10/2026 : l'ISR DRDY retardée est déjà traitée (point 26), `millis()` est matériel, la PWM aussi, et les sauvegardes sont rares. Laissé en l'état pour l'instant.
    - Relu le 12/10/2026 : `InterruptGuard` utilise `noInterrupts()` / `interrupts()` et n'est pas imbriquable. Dans `restore()`, la migration appelle `saveBoard()`, dont les gardes réactivent les IRQ avant la fin du garde de `restore()`. Sans effet aujourd'hui (plus d'accès au fichier ensuite), mais `save_and_disable_interrupts()` / `restore_interrupts()` éviteraient le piège.
14. ✅ **Thermostat après une reprise** (`Thermostat::update`).
    - Après `resume()`, la commande restait invalide (sortie OFF) tant que la température était dans l'hystérésis, même en refroidissement.
    - Sans état précédent (démarrage, retour du menu, levée d'une inhibition), le thermostat part maintenant à l'arrêt dans la bande.
15. ✅ **`TimeProportionalActuator` sans durée minimale d'impulsion**.
    - `Impulsion mini` sur l'actionneur temporel, `Marche mini` et `Arrêt mini` (anti-court-cycle) sur chaque relais.
16. ✅ **Glitch possible au démarrage d'une sortie** (`RelayOutput::begin`).
    - `pinMode(OUTPUT)` activait la sortie avec la valeur du reset (bas) avant `forceSafe()`. Un SSR actif à LOW, ou une sortie d'état sûr ON, pouvait réagir.
    - Corrigé le 16/10/2026 : le niveau sûr est écrit (`digitalWrite`) avant `pinMode(OUTPUT)`. Dans le cœur Arduino-Pico, `digitalWrite` sur une entrée fixe la valeur de sortie du GPIO, et `pinMode(OUTPUT)` ne la remet pas à 0 (pas de `gpio_init`). Test `relais : niveau sur avant la sortie`. `PWMOutput` n'était pas concerné.
    - Reste matériel : avant `beginOutputs()`, les broches sont en haute impédance, il faut une résistance de tirage (bilan, § 4, point 2 : erratum E9).
25. ✅ **Tempo des alarmes après 49 jours** (`LimitAlarm`, `LoopBreakAlarm`). Ajouté après la revue initiale (renuméroté : il portait le n° 17, déjà pris).
    - La tempo était recalculée à chaque cycle (`now - début >= délai`). Une alarme non mémorisée en cours depuis plus de 49,7 jours retombait pendant sa tempo au rebouclage de `millis()`.
    - La tempo écoulée est maintenant acquise tant que la condition dure, comme dans `ConditionAlarm` et `DelayTimer`. Tests de non-régression dans `test_limit_alarm.cpp` et `test_loop_break.cpp`.

### C. Mineur / nettoyage

17. ✅ `FixedBufferPrint` tronquait en silence au-delà de 1024 octets. Corrigé le 18/10/2026 : tampon de 2048 octets (un gros template dépassait 1 Ko), et une sortie qui déborde quand même se termine par `[sortie série tronquée]`.
18. ✅ `Sensor::isAccumulationDone()` utilisait `==` : un échantillon de trop empêchait la fin de la série. Corrigé en `>=` le 18/10/2026 (le test de mi-série du *chopping* reste en `==`).
19. ✅ `273.14` au lieu de `273.15` dans `pressureSeaLevel` (BMP580 et BME).
20. ✅ Code mort (traité le 18/10/2026) :
    - ✅ `RTC::addMenuActions` / `handlesMenuAction` / `executeMenuAction` (action « Mettre à jour », n° 71) supprimés : l'heure s'écrit à la validation du menu (`applyMenuParameters()`). `RTC::validateParameters` reste (interface `Configurable`, testée) ;
    - ✅ `printCSVPsychro` (indexait sans vérifier les bornes) supprimé ;
    - ✅ `readADC_Array` et `readADC_SingleArray` (boucle sans timeout) supprimés, ainsi que `readADC_Single()` et `readInternalTemp()` sans argument, inutilisés, qui attendaient sans limite ;
    - `Storage::erase()` n'est jamais appelé : gardé volontairement pour la future réinitialisation usine.
21. ✅ `ADS1120::begin` : `beginTransaction` n'est jamais fermé, et `begin(true)` (CS matériel) est ensuite écrasé par `pinMode(cs)`.
    - Relu le 18/10/2026 dans le cœur Arduino-Pico : moins grave que noté. `SPI.begin()` rappelle `beginTransaction()` puis `endTransaction()` avec les réglages déjà donnés, et aucune interruption n'est masquée (pas de `usingInterrupt()`). Le CS matériel était bien annulé par `pinMode()`.
    - Réécrit pour la lisibilité : `begin(false)` (CS logiciel), puis `beginTransaction()` / `endTransaction()`, sans `setCS()`. Comportement inchangé ; contrôle sur banc (mesures identiques) à faire.
22. ✅ Gain 3 fils : `setGain(16)` mais `measurementGain()` renvoie 8. C'est **correct**, car Vref vaut 2·I·Rref avec les deux IDAC : `Sensor::adcGain()` le commente.
23. ✅ `src/.garbo` gagnerait à sortir de `src/`, car IntelliSense voit des `main.cpp` en double (dossier supprimé).
24. ✅ Le README est désynchronisé du code :
    - ✅ chemin de `main.cpp` : le fichier est `src/main.cpp` ; le README et l'exemple minimal citaient `src/Templates/main.cpp` (corrigé) ;
    - ✅ il décrit l'ancienne TestInstallation (le template psychromètre `TestInstallation` figure maintenant dans le tableau des templates) ;
    - ✅ il dit « PWM prévu », alors que PWMOutput existe ;
    - ✅ il dit « PT1000 désactivée », alors qu'elle est convertie ;
    - ✅ l'environnement `opc_v01` est cassé (seul `opc_v02` reste).

Points 30 à 35 ajoutés le 12/10/2026.

30. ✅ **Journal : regroupement avec la session précédente** (`EventLog::add`).
    - Les événements relus de `/events.csv` gardent l'`uptimeMs` de leur session. Un événement identique de la nouvelle session, daté à moins de 60 s du même instant après le démarrage, était fusionné avec l'ancien (« x2 »).
    - Cas typique : une sonde en rupture au démarrage, ou une boucle de redémarrages. Le défaut récent n'apparaissait pas comme une nouvelle ligne : il gardait la date de l'ancienne et restait placé avant l'événement de redémarrage.
    - Corrigé le 16/10/2026 : `restore()` marque les événements relus (`EventEntry::fromPreviousSession`, non sauvegardé), et le regroupement s'arrête au premier. Test `journal : pas de regroupement avec la session precedente`.
31. ⬜ **Capacité des actions du menu** (`MenuBuilder::MAX_ACTIONS` = 16).
    - Calibration (6), Journal, Acquitter, une « RAZ compteurs » par relais et les actions de l'installation. Au-delà de 16, `initMenu()` échoue : le menu reste inaccessible et la cause n'est écrite que sur le port série.
    - Sans conséquence sur la carte v0.2 : elle n'a que deux broches de relais, donc au plus 13 actions avec les templates actuels. À revoir si `MAX_REGISTERED_OUTPUTS` (16) devient atteignable, ou à rendre visible à l'écran.
32. ✅ Le libellé « Manœuvres » (`RelayOutput`, compteurs) s'affichait « Man?uvres » : le « œ » n'existe pas en CP437 (`DisplayTextCodec`). Il s'écrit maintenant « Manoeuvres ».
33. ⬜ L'unité de `Kp` est toujours « 1/°C » dans le menu du PID, même si la mesure régulée est une humidité ou une pression. Elle pourrait être construite à partir de l'unité de la mesure, comme celle de la consigne.
34. ⬜ Les classes BME280 (`TemperatureBME`, `HumidityBME`, `PressureBME`) ne détectent pas un capteur muet, comme le BMP580 avant le point 27 : `Adafruit_BME280` ne signale pas une lecture I²C ratée. Aucun template ne les utilise.
35. ⬜ **Allocations dynamiques** : la règle « pas d'allocation dynamique » de `CLAUDE.md` a deux exceptions, à documenter.
    - `ArduinoMenuUI` crée les éléments du menu par `new`, une seule fois au démarrage : sans risque de fragmentation.
    - Les `JsonDocument` d'ArduinoJson 7 (`Storage`, `OPC::saveCounters()`) allouent sur le tas à chaque sauvegarde, puis libèrent. Volumes faibles et sauvegardes rares, donc risque faible, mais c'est le seul usage répété du tas.

---

## 3. Améliorations et fonctionnalités proposées

Les propositions réalisées depuis sont marquées ✅.

**Robustesse et architecture**
- ✅ Acquisition hors ISR, avec une machine d'états dans `loop()`, et I²C à 400 kHz (voir le point 8).
- Registre de défauts centralisé, affiché à l'écran : MCP ou ADC absent, BMP en panne, stockage en erreur, sonde coupée ou en court-circuit (en partie couvert par le journal des événements, consultable dans `Divers > Journal`).
- ✅ Détection de sonde coupée ou en court-circuit (`MeasurementStatus` : rupture, court-circuit, hors étendue) :
  - ✅ une plage plausible en Ω pour les RTD ;
  - les sources *burn-out* de l'ADS1120 (`setBurnoutCurrentSources` existe déjà) pour les thermocouples (un thermocouple ouvert est détecté par la polarisation 1 MΩ).
- ✅ Restauration tolérante paramètre par paramètre, et fichier de calibration séparé (voir le point 12).
- Diagnostic du watchdog : enregistrer l'étape en cours dans ses registres *scratch*.

**Métrologie**
- Coefficients Callendar–Van Dusen (R0, A, B, C) réglables par sonde, depuis un certificat d'étalonnage.
- Étalonnage deux points par sonde : offset + pente (0 °C en glace + un second point).
- Psychromètre :
  - ✅ coefficient A réglable (point 10) ;
  - ✅ formule sur glace (point 10) ;
  - mesures « point de rosée » et « humidité absolue » (les fonctions existent déjà dans `Psychrometer`) ;
  - détection de mèche sèche (humide ≈ sec ⇒ fausse HR à 100 %).
- ✅ Filtre configurable par mesure : filtre du 2e ordre réglable par entrée (`Filtre`). L'ancien `addLP` a disparu.

**Régulation**
- ✅ PID :
  - ✅ paramétrage en Kp/Ti/Td (en secondes) ;
  - ✅ filtre sur la dérivée ;
  - ✅ transfert *bumpless* ;
  - ✅ mode manuel ;
  - ✅ autotune moins agressif (Tyreus–Luyben).
- ✅ Actionneurs : temps mini ON/OFF, durée mini d'impulsion, anti-court-cycle.
- ✅ Verrouillages par les entrées numériques ISO1212 : fluxostat, contact de porte, autorisation externe (glue et `inhibit()`, `ConditionAlarm`, `dependsOn()`).
- ✅ Programmation horaire des consignes avec le DS3231.
- ✅ Alarmes haut/bas sur les mesures (`LimitAlarm`).
- Solaire : protection antigel et surchauffe du capteur (le template a un maximum de ballon, pas de protection du capteur).
- Four : reprise d'un programme après une coupure de courant (aujourd'hui, la cuisson est arrêtée au redémarrage).

**Données et communication**
- Journal CSV horodaté dans LittleFS, visible par l'USB MSC. Il remplacerait les logs série et les scripts Python (✅ journal des événements dans `/events.csv` ; ni mesures, ni accès par l'USB).
- Mode de sortie série CSV générique sélectionnable dans le menu.
- Interface en ligne de commande série : paramètres, calibration, export de la config.
- Import de `config.json` depuis le PC, validé puis appliqué.
- Modbus RTU (RS485).

**Interface**
- Accélération de l'encodeur (✅ pour la consigne réglée à l'accueil).
- Confirmation avant les actions de calibration.
- ✅ Heure affichée sur l'écran d'accueil (templates thermostat, solaire, programmation horaire, chambre froide).
- Mini-courbe de tendance.
- ✅ Écran d'erreur au démarrage.

**Qualité**
- ✅ Tests hôte à ajouter : PT100/CVD, psychrométrie, PID, `SetpointRamp`, `TimeProportionalActuator`.
  - Ils demandent un compilateur hôte : sous Fedora, `sudo dnf install gcc-c++`.
- CI GitHub Actions : tests hôte + `pio run -e opc_v02`.
- Tests hôte de `Storage` et de la séquence `OPC` (démarrage, application des réglages), avec un faux LittleFS : le point 29 y aurait été visible.
