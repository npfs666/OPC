# Revue du code OPC v0.2

Revue initiale du 28/09/2026. Statuts mis à jour le 29/09/2026.

Légende : ✅ corrigé · ⬜ à faire

---

## 1. Compréhension (résumé)

- **Cible** : Pico 2 (RP2350), avec le cœur Arduino Earle Philhower.
  - **Cœur 0** : acquisition, régulation et stockage (`loop`).
  - **Cœur 1** : écran ST7789, encodeur et menu (`loop1`).
  - Les deux cœurs échangent des messages par la FIFO (`InterCoreMessages.h`) et partagent les données sous le mutex `processDataMutex`.
- **Chaîne de traitement** : `SensorBoard`, puis `Measurement`, `Regulator`, `Actuator` et `Output`, sous l'orchestration de `ProcessControl`. L'écran lit une copie des mesures (`ProcessSnapshot`).
- **Acquisition**
  - L'ISR DRDY de l'ADS1120 (`SensorBoard::adcInterrupt`) accumule N échantillons par entrée.
  - Quand une entrée a tous ses échantillons, l'ISR change le routage par le MCP23017 (I²C Wire1) et relance la conversion.
  - À la fin d'un cycle complet, l'ISR lit la température interne de l'ADC (utilisée comme jonction froide) et positionne `newMeasurement`.
  - En 3 fils, les sources de courant (IDAC) sont inversées à mi-série (*chopping*).
- **Paramètres**
  - Chaque objet s'enregistre dans `ParameterList` par un `ParameterOwner` (catégorie et propriétaire).
  - `MenuBuilder` et `ArduinoMenuUI` génèrent le menu.
  - `ParameterEditor` gère les brouillons : capture, validation et application sur le cœur 0.
  - `Storage` écrit un JSON dans LittleFS et l'exporte en lecture seule par USB.
- **Sécurités**
  - États sûrs des sorties.
  - Timeout des mesures.
  - Watchdog à deux cœurs : le cœur 0 nourrit le watchdog seulement si le cœur 1 a aussi progressé.
  - Mise en sécurité des sorties pendant l'application des réglages.

L'architecture est propre et défensive : validations nombreuses, écriture atomique du fichier par *rename*, handshake FIFO entre cœurs.

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
   - Reste ouvert : les erreurs I²C du MCP pendant l'acquisition, dans l'ISR, ne sont toujours pas vérifiées.
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

### B. Risques de conception (à planifier)

8. ⬜ (partiellement ✅) **ISR DRDY longue** (`SensorBoard::adcInterrupt`).
   - ✅ Les `delay(2)` de `ADS1120::sendCommand` ont été supprimés.
   - ✅ Wire1 passé à 400 kHz (`OPC.cpp`).
   - ✅ La température ADC n'est plus attendue dans l'ISR : elle devient la dernière étape du cycle (`startAdcTemperature`), lue à l'IRQ DRDY suivante.
   - ⬜ Il faudrait sortir le séquencement de l'ISR : l'ISR lit l'échantillon, une machine d'états dans `loop()` gère le routage.
9. ✅ **Précision PT100** : la conversion passe maintenant par Callendar–Van Dusen avec Newton-Raphson (`PT100::getResistanceToTemperatureNewton`).
   - Mesure faite sur l'ancienne table : erreur de 15 mK à 100 °C, 71 mK à 266 °C, et aucune mesure au-delà de 266 °C.
   - ✅ Test hôte ajouté sur les points IEC 60751 (`testPT100CallendarVanDusen`).
10. ⬜ **Deux formules psychrométriques différentes** (`Psychrometrics.cpp`).
    - `relativeHumidity()` utilise Magnus 17,2694/238,3 avec un γ fixe.
    - `getRH()` utilise 17,27/237,3 avec un A calculé. C'est la seule qui sert.
    - `dewPoint()` utilise encore 17,27/237,7.
    - Il manque la formule sur glace quand le bulbe humide passe sous 0 °C, et un coefficient psychrométrique réglable.
    - `constrain(0,100)` masque les incohérences, par exemple un bulbe humide plus chaud que le sec.
11. ⬜ **Limites du PID** (`PID.cpp`).
    - `KD_MAX = 100` fait échouer l'autotune d'un process lent. Exemple : Ku = 1,3 et Tu = 3600 s donnent Kd ≈ 350.
    - `Ki` n'a que 3 décimales : une valeur de 0,0004 s'affiche 0,000, et le pas de 0,001 empêche de la régler.
    - La consigne est limitée à 0–80 °C, alors que le thermostat accepte 0–200 °C.
    - L'intégrale est remise à zéro à chaque application du menu : pas de transfert *bumpless*.
12. ⬜ **Restauration « tout ou rien »** (`Storage::readConfiguration`).
    - Un seul paramètre hors plage, ou une liste d'options modifiée, invalide tout le fichier.
    - Les calibrations (Rref, N0) sont alors perdues en silence.
13. ⬜ **Interruptions coupées pendant toute la sauvegarde** (`Storage::save`, `InterruptGuard`).
    - Cela représente des centaines de ms sans IRQ sur le cœur 0.
14. ⬜ **Thermostat après une reprise** (`Thermostat::update`).
    - Après `resume()`, la commande reste invalide (sortie OFF) tant que la température est dans l'hystérésis, même en refroidissement.
15. ⬜ **`TimeProportionalActuator` sans durée minimale d'impulsion**.
    - Il n'y a pas non plus de temps mini marche/arrêt (anti-court-cycle).
16. ⬜ **Glitch possible au démarrage d'une sortie** (`RelayOutput::begin`).
    - `pinMode(OUTPUT)` force le niveau bas avant `forceSafe()`. Un SSR actif à LOW peut réagir.
    - Avant `beginOutputs()`, les broches sont en haute impédance : il faut une résistance de tirage matérielle.

### C. Mineur / nettoyage

17. ⬜ `FixedBufferPrint` tronque en silence au-delà de 1024 octets.
18. ⬜ `Sensor::isAccumulationDone()` utilise `==` : `>=` serait plus robuste.
19. ⬜ `273.14` au lieu de `273.15` dans `pressureSeaLevel` (BMP580 et BME).
20. ⬜ Code mort :
    - `RTC::addMenuActions` / `executeMenuAction` / `validateParameters` ne sont jamais appelés ;
    - `printCSVPsychro` indexe sans vérifier les bornes ;
    - `readADC_Array` et `readADC_SingleArray` (boucle sans timeout) ne servent pas.
21. ⬜ `ADS1120::begin` : `beginTransaction` n'est jamais fermé, et `begin(true)` (CS matériel) est ensuite écrasé par `pinMode(cs)`.
22. ⬜ Gain 3 fils : `setGain(16)` mais `measurementGain()` renvoie 8. C'est **correct**, car Vref vaut 2·I·Rref avec les deux IDAC, mais il faut le commenter.
23. ⬜ `src/.garbo` gagnerait à sortir de `src/`, car IntelliSense voit des `main.cpp` en double.
24. ⬜ Le README est désynchronisé du code :
    - il cite `src/main.cpp`, alors que le fichier est `src/Templates/main.cpp` ;
    - il décrit l'ancienne TestInstallation ;
    - il dit « PWM prévu », alors que PWMOutput existe ;
    - il dit « PT1000 désactivée », alors qu'elle est convertie ;
    - l'environnement `opc_v01` est cassé.

---

## 3. Améliorations et fonctionnalités proposées

**Robustesse et architecture**
- Acquisition hors ISR, avec une machine d'états dans `loop()`, et I²C à 400 kHz.
- Registre de défauts centralisé, affiché à l'écran : MCP ou ADC absent, BMP en panne, stockage en erreur, sonde coupée ou en court-circuit.
- Détection de sonde coupée ou en court-circuit :
  - une plage plausible en Ω pour les RTD ;
  - les sources *burn-out* de l'ADS1120 (`setBurnoutCurrentSources` existe déjà) pour les thermocouples.
- Restauration tolérante paramètre par paramètre, et fichier de calibration séparé.
- Diagnostic du watchdog : enregistrer l'étape en cours dans ses registres *scratch*.

**Métrologie**
- Coefficients Callendar–Van Dusen (R0, A, B, C) réglables par sonde, depuis un certificat d'étalonnage.
- Étalonnage deux points par sonde : offset + pente (0 °C en glace + un second point).
- Psychromètre :
  - coefficient A réglable ;
  - formule sur glace ;
  - mesures « point de rosée » et « humidité absolue » (les fonctions existent déjà dans `Psychrometer`) ;
  - détection de mèche sèche (humide ≈ sec ⇒ fausse HR à 100 %).
- Filtre configurable par mesure : EMA ou médiane (`addLP` existe mais n'est pas utilisé).

**Régulation**
- PID :
  - paramétrage en Kp/Ti/Td (en secondes) ;
  - filtre sur la dérivée ;
  - transfert *bumpless* ;
  - mode manuel ;
  - autotune moins agressif (Tyreus–Luyben ou SIMC).
- Actionneurs : temps mini ON/OFF, durée mini d'impulsion, anti-court-cycle.
- Verrouillages par les entrées numériques ISO1212 : fluxostat, contact de porte, autorisation externe.
- Programmation horaire des consignes avec le DS3231.
- Alarmes haut/bas sur les mesures.
- Solaire : protection antigel et surchauffe du capteur.

**Données et communication**
- Journal CSV horodaté dans LittleFS, visible par l'USB MSC. Il remplacerait les logs série et les scripts Python.
- Mode de sortie série CSV générique sélectionnable dans le menu.
- Interface en ligne de commande série : paramètres, calibration, export de la config.
- Import de `config.json` depuis le PC, validé puis appliqué.
- Modbus RTU (RS485).

**Interface**
- Accélération de l'encodeur.
- Confirmation avant les actions de calibration.
- Heure affichée sur l'écran d'accueil.
- Mini-courbe de tendance.
- Écran d'erreur au démarrage.

**Qualité**
- Tests hôte à ajouter : PT100/CVD, psychrométrie, PID, `SetpointRamp`, `TimeProportionalActuator`.
  - Ils demandent un compilateur hôte : sous Fedora, `sudo dnf install gcc-c++`.
- CI GitHub Actions : tests hôte + `pio run -e opc_v02`.
