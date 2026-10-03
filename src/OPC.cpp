#include "OPC.h"

#include <Installation.h>
#include <SPI.h>
#include <Wire.h>
#include <Hardware/pinout.h>
#include <hardware/sync.h>

#include <Outputs/Output.h>
#include <hmi/DisplayTextCodec.h>
#include <hmi/HomeScreen.h>

#include <cstring>

namespace
{
    constexpr int16_t STARTUP_ERROR_MARGIN = 8;
    constexpr int16_t STARTUP_ERROR_LINE_HEIGHT = 20;
    // Taille de texte 2 : 12 px par caractère sur 240 px de large.
    constexpr size_t STARTUP_ERROR_LINE_LENGTH = 18;

    /**
     * Affiche un texte UTF-8 sur plusieurs lignes, en coupant de préférence
     * aux espaces. Retourne l'ordonnée de la ligne suivante.
     */
    int16_t printWrapped(
        Adafruit_GFX& display,
        const char* text,
        int16_t y,
        uint8_t maximumLines)
    {
        char encoded[96] = {};

        DisplayTextCodec::utf8ToCp437(
            text != nullptr ? text : "",
            encoded,
            sizeof(encoded));

        const char* cursor = encoded;

        for (uint8_t line = 0;
             line < maximumLines && *cursor != '\0';
             line++)
        {
            while (*cursor == ' ')
                cursor++;

            size_t length = std::strlen(cursor);

            if (length > STARTUP_ERROR_LINE_LENGTH)
            {
                length = STARTUP_ERROR_LINE_LENGTH;

                for (size_t i = STARTUP_ERROR_LINE_LENGTH; i > 0; i--)
                {
                    if (cursor[i] == ' ')
                    {
                        length = i;
                        break;
                    }
                }
            }

            display.setCursor(STARTUP_ERROR_MARGIN, y);

            for (size_t i = 0; i < length; i++)
                display.write(cursor[i]);

            cursor += length;
            y += STARTUP_ERROR_LINE_HEIGHT;
        }

        return y;
    }

    class FixedBufferPrint final : public Stream
    {
    public:
        FixedBufferPrint(
            uint8_t* storage,
            size_t capacity)
            : storage(storage),
              capacity(capacity)
        {
        }

        size_t write(uint8_t value) override
        {
            if (length < capacity)
                storage[length++] = value;

            return 1;
        }

        size_t write(
            const uint8_t* data,
            size_t size) override
        {
            for (size_t i = 0; i < size; i++)
                write(data[i]);

            return size;
        }

        size_t size() const
        {
            return length;
        }

        int available() override
        {
            return 0;
        }

        int read() override
        {
            return -1;
        }

        int peek() override
        {
            return -1;
        }

        void flush() override
        {
        }

    private:
        uint8_t* storage = nullptr;
        size_t capacity = 0;
        size_t length = 0;
    };
}

OPC::OPC(Installation& installation)
    : tft(Board::Rp2040::LCD_SPI, Board::Rp2040::LCD_CS, Board::Rp2040::LCD_DC, -1),
      userInstall(installation)
{
    mutex_init(&processDataMutex);
}



void OPC::initSerial()
{
    Serial.begin(115200);
    Serial.println("Open Process Controller v0.2");
}



void OPC::initDisplay()
{
    SPI1.setSCK(Board::Rp2040::LCD_SCK);
    SPI1.setTX(Board::Rp2040::LCD_MOSI);

    tft.init(240, 240);
    tft.setRotation(1);
    tft.setSPISpeed(48000000);
    tft.setTextWrap(false);
    tft.cp437(true);
    tft.fillScreen(ST77XX_BLACK);
}

void OPC::initI2C() {

    Wire1.setSDA(Board::Rp2040::I2C_SDA);
    Wire1.setSCL(Board::Rp2040::I2C_SCL);
    Wire1.setClock(400000);
}

void OPC::initBMP580()
{
    bmp580Initialized =
        bmp580.begin(Board::BMP::ADDRESS, &Wire1);

    if (!bmp580Initialized)
    {
        Serial.println("BMP580 initialization failed");
        return;
    }

    bmp580.setTemperatureOversampling(BMP5XX_OVERSAMPLING_16X);
    bmp580.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
    bmp580.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    bmp580.setPowerMode(BMP5XX_POWERMODE_NORMAL);
}

void OPC::initSensorBoard()
{
    // Set pin DC_DC_PWM HIGH to switch the pico DC-DC converter to PWM (improved ripple)
	// Improves a lot measurement stability
    pinMode(Board::Rp2040::DC_DC_PWM,OUTPUT);
    digitalWrite(Board::Rp2040::DC_DC_PWM,HIGH);

    sensorBoardInitialized = input.init();
}

void OPC::initRTC() {
    clock.begin(Board::Rp2040::I2C_SDA, Board::Rp2040::I2C_SCL, Wire1);
}


void OPC::initRotenc()
{
    encoder.begin();
}

void OPC::handleISRRotenc()
{
    encoder.onRotationISR();
}

void OPC::handleISRButton()
{
    encoder.onButtonISR();
}

bool OPC::newMeasurement()
{
    // Sans entrée analogique, l'ADC ne cadence rien : cycle fixe.
    const bool sensorlessCycle =
        controlCycleStarted &&
        input.sensorCount() == 0 &&
        !acquisitionPausedForMenu &&
        millis() - lastMeasurementTime >= SENSORLESS_CYCLE_MS;

    if (!input.newMeasurement && !sensorlessCycle)
        return false;

    input.newMeasurement = false;

    const uint32_t times = millis();

    mutex_enter_blocking(
        &processDataMutex);

    if (!controlOutputsEnabled)
        controller.resume(times);

    {
        // Lecture BMP580 sur Wire1, partagé avec l'ISR ADC.
        SensorBoard::SharedBusGuard busGuard;
        controller.updateMeasurementsAndRegulators(
            times);
    }

    controller.captureSnapshot(
        sharedProcessSnapshot,
        times);

    const bool configurationSaveRequested =
        userInstall.takeConfigurationSaveRequest();

    lastMeasurementTime = times;

    controlOutputsEnabled =
        controller.outputsHealthy();

    if (!controlOutputsEnabled)
        controller.forceSafeOutputs();

    if (configurationSaveRequested)
    {
        controller.forceSafeOutputs();
        controlOutputsEnabled = false;
    }

    mutex_exit(&processDataMutex);

    if (configurationSaveRequested)
        requestConfigurationSave();

    rp2040.fifo.push_nb(
        interCoreMessageValue(
            InterCoreMessage::PrintDataAvailable));

    return true;
}

void OPC::refreshClock()
{
    lastClockRefresh = millis();

    RTC::DateTime dateTime;
    bool readOk;
    bool oscillatorRunning = false;
    {
        SensorBoard::SharedBusGuard busGuard;
        readOk = clock.readDateTime(dateTime);

        if (readOk && !clock.isTimeValid(oscillatorRunning))
            readOk = false;
    }

    // Une erreur I2C isolée ne doit pas arrêter les programmations : la
    // dernière heure lue reste utilisée quelques secondes.
    if (readOk)
        clockReadFailures = 0;
    else if (clockReadFailures < CLOCK_READ_FAILURES_TOLERATED)
    {
        clockReadFailures++;
        return;
    }

    mutex_enter_blocking(&processDataMutex);
    sharedClockDateTime = dateTime;
    sharedClockValid = readOk;
    // Heure perdue (OSF, pile vide) : les programmations horaires s'arrêtent.
    controller.updateClock(
        ClockSample{dateTime, readOk && oscillatorRunning});
    mutex_exit(&processDataMutex);
}

void OPC::controlPoll()
{
    // Les entrées restent accessibles même lorsque les sorties sont arrêtées.
    mutex_enter_blocking(&processDataMutex);
    controller.pollInputs(millis());
    controller.captureInputSnapshot(sharedProcessSnapshot);
    controller.updateOperatingTime(millis());
    mutex_exit(&processDataMutex);

    if (controller.takeCountersChanged() ||
        millis() - lastCountersSave >= COUNTERS_SAVE_PERIOD_MS)
    {
        saveCounters();
    }

    storage.poll();

    if (millis() - lastClockRefresh >= 1000)
        refreshClock();

    if (configurationSavePending &&
        !acquisitionPausedForMenu &&
        static_cast<int32_t>(millis() - configurationSaveDueAt) >= 0)
    {
        configurationSavePending = false;

        if (!storage.save(
                userInstall.configurationKey(),
                userInstall.getParameters()))
        {
            Serial.println(
                "Automatic configuration save failed");
        }

        return;
    }

    if (!controlOutputsEnabled ||
        acquisitionPausedForMenu)
    {
        return;
    }

    const uint32_t now = millis();

    mutex_enter_blocking(
        &processDataMutex);

    const bool measurementTimedOut =
        (now - lastMeasurementTime) >=
        userInstall.measurementTimeoutMs();

    if (measurementTimedOut ||
        !controller.outputsHealthy())
    {
        controller.forceSafeOutputs();
        controlOutputsEnabled = false;
    }
    else
    {
        controller.poll(now);
    }

    mutex_exit(&processDataMutex);
}

bool OPC::initMeasurements()
{
    // Sans le MCP23017, le routage analogique est inconnu : mesures fausses.
    if (!sensorBoardInitialized)
        return failStartup(StartupError::SensorBoard);

    if (userInstall.requiresBMP580() &&
        !bmp580Initialized)
    {
        return failStartup(StartupError::BMP580);
    }

    if (!userInstall.prepareParameterRegistration())
        return failStartup(StartupError::ParameterStorage);

    if (!userInstall.begin(
            input,
            bmp580,
            controller))
    {
        return failStartup(StartupError::Installation);
    }

    clock.registerParameters(userInstall.getParameters());

    if (!userInstall.completeParameterRegistration() ||
        userInstall.getParameters().hasError())
    {
        return failStartup(StartupError::ParameterRegistration);
    }

    const bool storageReady =
        storage.begin();

    const Storage::RestoreResult restoreResult =
        storage.restore(
            userInstall.configurationKey(),
            userInstall.getParameters(),
            parameterEditor,
            *this);

    switch (restoreResult)
    {
    case Storage::RestoreResult::Restored:
        Serial.println(
            "Configuration restored");
        break;

    case Storage::RestoreResult::NoFile:
        Serial.println(
            "No saved configuration; using defaults");
        break;

    case Storage::RestoreResult::InvalidFile:
        Serial.println(
            "Invalid saved configuration; using defaults");
        break;

    case Storage::RestoreResult::StorageUnavailable:
        Serial.println(
            storageReady
                ? "Configuration unavailable; using defaults"
                : "Storage initialization failed; using defaults");
        break;
    }

    if (!controller.beginOutputs())
        return failStartup(StartupError::Outputs);

    restoreCounters();

    controlOutputsEnabled = false;
    lastMeasurementTime = millis();

    refreshClock();

    input.startContinuous();
    controlCycleStarted = true;

    return true;
}

bool OPC::failStartup(StartupError error)
{
    startupError = error;
    printStartupError(Serial);
    return false;
}

const char* OPC::startupErrorDetail() const
{
    // Une installation peut préciser sa cause avec Installation::fail().
    if (startupError == StartupError::Installation &&
        userInstall.failureReason() != nullptr)
    {
        return userInstall.failureReason();
    }

    return startupErrorText(startupError).detail;
}

void OPC::printStartupError(Print& output) const
{
    output.print("Erreur de demarrage : ");
    output.print(startupErrorText(startupError).title);
    output.print(" - ");
    output.println(startupErrorDetail());
}

void OPC::showStartupError()
{
    constexpr uint16_t COLOR_GREY = 0x8410;

    tft.cp437(true);
    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.fillScreen(ST77XX_BLACK);

    tft.fillRect(0, 0, tft.width(), 32, ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
    tft.setCursor(STARTUP_ERROR_MARGIN, 9);
    tft.print("ERREUR DEMARRAGE");

    int16_t y = 50;

    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    y = printWrapped(tft, startupErrorText(startupError).title, y, 2);

    y += 6;

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    printWrapped(tft, startupErrorDetail(), y, 4);

    tft.setTextColor(COLOR_GREY, ST77XX_BLACK);
    printWrapped(tft, "Sorties inactives", 190, 1);
    printWrapped(tft, "Détails : série", 212, 1);
}

void OPC::initMenu()
{
    if (menu.isInitialized())
        return;

    if (!userInstall.buildMenu(
            menuDefinition) ||
        !input.addMenuActions(
            menuDefinition) ||
        !controller.addMenuActions(
            menuDefinition))
    {
        Serial.println("Menu generation failed");
        return;
    }

    if (!menu.begin(
            tft,
            parameterEditor,
            menuDefinition))
    {
        Serial.println("Menu initialization failed");
    }
}

void OPC::copyProcessSnapshot()
{
    mutex_enter_blocking(
        &processDataMutex);

    displayProcessSnapshot =
        sharedProcessSnapshot;

    userInstall.captureHomeScreenState();

    mutex_exit(&processDataMutex);
}

void OPC::showClockAlert()
{
    constexpr uint16_t COLOR_ORANGE = 0xFD20;
    constexpr uint16_t COLOR_GREY = 0x8410;

    tft.cp437(true);
    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.fillScreen(ST77XX_BLACK);

    tft.fillRect(0, 0, tft.width(), 32, COLOR_ORANGE);
    tft.setTextColor(ST77XX_BLACK, COLOR_ORANGE);
    tft.setCursor(STARTUP_ERROR_MARGIN, 9);
    tft.print("ALERTE HORLOGE");

    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    int16_t y = printWrapped(tft, "Heure inconnue", 50, 1);

    y += 6;

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    y = printWrapped(
        tft, "Régulations programmées à l'arrêt, leurs sorties en sécurité.", y, 4);

    y += 6;

    tft.setTextColor(COLOR_GREY, ST77XX_BLACK);
    printWrapped(tft, "Pile du DS3231 à vérifier.", y, 2);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    printWrapped(tft, "Clic : Divers > Horloge", 212, 1);
}

void OPC::showHomeScreen(
    bool fullRefresh)
{
    const bool clockAlert =
        displayProcessSnapshot.clockRequired() &&
        !displayProcessSnapshot.clock().valid;

    if (clockAlert)
    {
        if (!clockAlertShown || fullRefresh)
            showClockAlert();

        if (!clockAlertShown)
        {
            Serial.println(
                "Alerte horloge : heure inconnue, programmations arretees");
        }

        clockAlertShown = true;
        return;
    }

    // Retour à l'accueil normal après réglage de l'heure.
    if (clockAlertShown)
    {
        clockAlertShown = false;
        fullRefresh = true;
    }

    HomeScreenContext context{
        tft,
        displayProcessSnapshot,
        millis(),
        fullRefresh
    };

    context.editingSetpoint = setpointEditor.isActive();
    context.editedSetpoint = setpointEditor.value();

    userInstall.printHomeScreen(context);
}

void OPC::requestMenu()
{
    if (uiState != UIState::Home ||
        !menu.isInitialized())
    {
        return;
    }

    pendingMenuAction =
        MenuBuilder::NO_ACTION;

    uiState = UIState::CaptureRequested;

    rp2040.fifo.push(
        interCoreMessageValue(
            InterCoreMessage::CaptureMenuParameters));
}

void OPC::requestParameterApply(
    MenuBuilder::ActionId actionId)
{
    if (uiState != UIState::Menu)
        return;

    if (actionId != MenuBuilder::NO_ACTION &&
        menuDefinition.findAction(actionId) == nullptr)
    {
        return;
    }

    menu.close();

    if (clockMenuOpen)
    {
        // Le timeout abandonne les champs de l'horloge non validés.
        parameterEditor.capture(RTC::MENU_OWNER_KEY);
        clockMenuOpen = false;
    }

    pendingMenuAction = actionId;

    uiState = UIState::ApplyRequested;

    /*
     * Les brouillons ont été écrits par le coeur 1.
     * La barrière garantit leur visibilité avant
     * l'envoi de la commande au coeur 0.
     */
    __dmb();

    rp2040.fifo.push(
        interCoreMessageValue(
            InterCoreMessage::ApplyMenuParameters));
}

void OPC::requestClockApply()
{
    uiState = UIState::ClockApplyRequested;
    // Les brouillons restent figés jusqu'à la réponse du cœur contrôle.
    __dmb();
    rp2040.fifo.push(interCoreMessageValue(
        InterCoreMessage::ApplyClockParameters));
}

void OPC::restoreCounters()
{
    JsonDocument document;

    lastCountersSave = millis();

    if (!storage.loadCounters(document) ||
        document["schema"] != COUNTERS_SCHEMA_VERSION)
    {
        Serial.println("No saved counters");
        return;
    }

    controller.restoreOperatingSeconds(
        document["operating_s"] | 0.0);

    JsonObjectConst outputs = document["outputs"];

    for (size_t i = 0; i < controller.registeredOutputCount(); i++)
    {
        Output* output = controller.registeredOutput(i);

        if (output == nullptr || output->counters() == nullptr)
            continue;

        JsonObjectConst saved = outputs[output->configurationKey()];

        if (saved.isNull())
            continue;

        output->restoreCounters(
            saved["switches"] | 0UL,
            saved["on_s"] | 0.0);
    }

    Serial.println("Counters restored");
}

void OPC::saveCounters()
{
    lastCountersSave = millis();

    JsonDocument document;
    document["schema"] = COUNTERS_SCHEMA_VERSION;

    mutex_enter_blocking(&processDataMutex);

    document["operating_s"] = controller.operatingSeconds();

    JsonObject outputs = document["outputs"].to<JsonObject>();

    for (size_t i = 0; i < controller.registeredOutputCount(); i++)
    {
        const Output* output = controller.registeredOutput(i);

        if (output == nullptr || output->counters() == nullptr)
            continue;

        JsonObject saved = outputs[output->configurationKey()].to<JsonObject>();
        saved["switches"] = output->counters()->switches;
        saved["on_s"] = output->onSeconds();
    }

    mutex_exit(&processDataMutex);

    if (!storage.saveCounters(document))
        Serial.println("Counters save failed");
}

void OPC::requestConfigurationSave(uint32_t delayMs)
{
    const uint32_t now = millis();
    const uint32_t due = now + delayMs;

    // Une demande immédiate passe devant ; une demande différée repousse
    // l'échéance (réglages successifs à l'encodeur).
    if (!configurationSavePending ||
        delayMs == 0 ||
        static_cast<int32_t>(due - configurationSaveDueAt) > 0)
    {
        configurationSaveDueAt = delayMs == 0 ? now : due;
    }

    configurationSavePending = true;
}

void OPC::homePoll(int32_t movement, bool clicked)
{
    const uint32_t now = millis();
    const Parameter* setpoint = userInstall.homeSetpoint();

    // Rotation : réglage de la consigne (pas sous l'alerte horloge).
    if (movement != 0 && setpoint != nullptr && !clockAlertShown)
    {
        if (!setpointEditor.isActive())
        {
            mutex_enter_blocking(&processDataMutex);
            const double_t current = *setpoint->value.number;
            mutex_exit(&processDataMutex);

            setpointEditor.begin(
                current,
                setpoint->data.number.minimum,
                setpoint->data.number.maximum,
                setpoint->data.number.step,
                now);
        }

        // Même sens que l'édition d'une valeur dans le menu, où un cran
        // positif (vers le bas de la liste) diminue la valeur.
        setpointEditor.rotate(-movement, now);
        showHomeScreen(false);
        return;
    }

    // Seul un clic valide. Sans action pendant le délai du menu, le réglage
    // est abandonné et la consigne en place est conservée.
    if (setpointEditor.isActive())
    {
        if (clicked)
        {
            commitHomeSetpoint();
        }
        else if (setpointEditor.inactiveFor(now, userInstall.menuTimeoutMs()))
        {
            setpointEditor.end();
            showHomeScreen(false);
        }

        return;
    }

    if (clicked)
        requestMenu();
}

void OPC::commitHomeSetpoint()
{
    if (!setpointEditor.hasChanged())
    {
        setpointEditor.end();
        showHomeScreen(false);
        return;
    }

    mutex_enter_blocking(&processDataMutex);
    pendingHomeSetpoint = setpointEditor.value();
    mutex_exit(&processDataMutex);

    // La valeur reste affichée jusqu'à l'acquittement du cœur contrôle.
    uiState = UIState::HomeSetpointApplyRequested;
    __dmb();
    rp2040.fifo.push(interCoreMessageValue(
        InterCoreMessage::ApplyHomeSetpoint));
}

void OPC::uiPoll()
{
    int32_t movement =
        encoder.takeRotation();

    const bool clicked =
        encoder.takeClick();

    if (uiState == UIState::StartupFailed)
    {
        // Rappel périodique : le moniteur série peut être ouvert après le boot.
        if (millis() - lastStartupErrorPrint >= STARTUP_ERROR_REPEAT_MS)
        {
            lastStartupErrorPrint = millis();
            printStartupError(Serial);
        }

        return;
    }

    if (uiState == UIState::Home)
    {
        homePoll(movement, clicked);
        return;
    }

    if (uiState != UIState::Menu)
        return;

    const bool hadActivity =
        movement != 0 ||
        clicked;

    while (movement > 0)
    {
        menu.move(1);
        movement--;
    }

    while (movement < 0)
    {
        menu.move(-1);
        movement++;
    }

    ArduinoMenuUI::EnterResult enterResult;

    if (clicked)
        enterResult = menu.enter();

    if (enterResult.type == ArduinoMenuUI::EnterResult::Type::ClockOpened)
    {
        clockMenuOpen = true;
        uiState = UIState::ClockCaptureRequested;
        __dmb();
        rp2040.fifo.push(interCoreMessageValue(
            InterCoreMessage::CaptureClockParameters));
        return;
    }

    if (enterResult.type == ArduinoMenuUI::EnterResult::Type::ClockValidate)
    {
        requestClockApply();
        return;
    }

    if (enterResult.type == ArduinoMenuUI::EnterResult::Type::ClockClosed)
    {
        // Aucun accès au DS3231 : restaurer les derniers champs capturés.
        parameterEditor.capture(RTC::MENU_OWNER_KEY);
        clockMenuOpen = false;
    }

    if (clockMenuOpen)
    {
        mutex_enter_blocking(&processDataMutex);
        const RTC::DateTime dateTime = sharedClockDateTime;
        const bool valid = sharedClockValid;
        mutex_exit(&processDataMutex);
        menu.updateClockDisplay(dateTime, valid);
    }

    menu.poll();

    const uint32_t now = millis();

    if (hadActivity)
        lastMenuActivity = now;

    const uint32_t timeout =
        userInstall.menuTimeoutMs();

    const bool timedOut =
        (now - lastMenuActivity) >= timeout;

    if (enterResult.type ==
        ArduinoMenuUI::EnterResult::Type::Action)
    {
        requestParameterApply(
            enterResult.actionId);
    }
    else if (enterResult.type ==
                 ArduinoMenuUI::EnterResult::Type::Exit ||
             timedOut)
    {
        requestParameterApply();
    }
}

void OPC::handleControlMessage(
    InterCoreMessage message)
{
    switch (message)
    {
    case InterCoreMessage::CaptureClockParameters:
        __dmb();
        if (menuSessionOpen)
        {
            {
                SensorBoard::SharedBusGuard busGuard;
                clock.onMenuOpened();
            }
            parameterEditor.capture(RTC::MENU_OWNER_KEY);
        }
        __dmb();
        rp2040.fifo.push(interCoreMessageValue(
            InterCoreMessage::ClockParametersCaptured));
        break;

    case InterCoreMessage::ApplyClockParameters:
    {
        __dmb();
        bool saved = false;
        if (menuSessionOpen)
        {
            SensorBoard::SharedBusGuard busGuard;
            saved = clock.applyMenuParameters(parameterEditor);
        }
        __dmb();
        rp2040.fifo.push(interCoreMessageValue(saved
            ? InterCoreMessage::ClockParametersApplied
            : InterCoreMessage::ClockParametersRejected));
        break;
    }

    case InterCoreMessage::ApplyHomeSetpoint:
    {
        bool applied = false;

        // Jamais pendant une session de menu : les brouillons l'écraseraient.
        if (!menuSessionOpen)
        {
            mutex_enter_blocking(&processDataMutex);
            applied = userInstall.applyHomeSetpoint(pendingHomeSetpoint);
            mutex_exit(&processDataMutex);
        }

        // Ni pause de l'acquisition ni état sûr : seule la consigne change.
        if (applied)
            requestConfigurationSave(HOME_SETPOINT_SAVE_DELAY_MS);

        __dmb();
        rp2040.fifo.push(interCoreMessageValue(
            InterCoreMessage::HomeSetpointApplied));
        break;
    }

    case InterCoreMessage::CaptureMenuParameters:
        if (menuSessionOpen)
            break;

        mutex_enter_blocking(&processDataMutex);
        userInstall.onMenuOpened();
        {
            SensorBoard::SharedBusGuard busGuard;
            clock.onMenuOpened();
        }
        parameterEditor.capture();
        menuSessionOpen = true;
        mutex_exit(&processDataMutex);

        // Le cœur UI possède ensuite les brouillons jusqu'à la fermeture.
        __dmb();
        rp2040.fifo.push(interCoreMessageValue(
            InterCoreMessage::MenuParametersCaptured));
        break;

    case InterCoreMessage::ApplyMenuParameters:
    {
        /*
         * La commande FIFO vient du coeur 1 :
         * les brouillons ne seront plus modifiés tant
         * que la réponse n'aura pas été reçue.
         */
        __dmb();

        const MenuBuilder::ActionId actionId =
            pendingMenuAction;

        pendingMenuAction =
            MenuBuilder::NO_ACTION;

        if (!menuSessionOpen)
        {
            rp2040.fifo.push(
                interCoreMessageValue(
                    InterCoreMessage::MenuParametersRejected));
            break;
        }

        /*
         * Acquittement seul : il ne modifie aucun réglage, donc ni pause de
         * l'acquisition, ni état sûr, ni sauvegarde.
         */
        if (!parameterEditor.hasChanges() &&
            controller.handlesMenuAction(actionId) &&
            !acquisitionPausedForMenu)
        {
            mutex_enter_blocking(&processDataMutex);
            controller.executeMenuAction(actionId);
            mutex_exit(&processDataMutex);

            menuSessionOpen = false;
            __dmb();
            rp2040.fifo.push(interCoreMessageValue(
                InterCoreMessage::MenuParametersApplied));
            break;
        }

        // Une consultation seule ne touche ni au PID ni à l'acquisition.
        if (!parameterEditor.hasChanges() &&
            actionId == MenuBuilder::NO_ACTION &&
            !acquisitionPausedForMenu)
        {
            menuSessionOpen = false;
            __dmb();
            rp2040.fifo.push(interCoreMessageValue(
                InterCoreMessage::MenuParametersApplied));
            break;
        }

        // Préserver les valeurs calculées pendant la navigation (autotune,
        // diagnostics...) si l'utilisateur ne les a pas modifiées.
        parameterEditor.refreshUnchanged();

        if (!parameterEditor.validate() ||
            !input.validateParameters(
                parameterEditor) ||
            !controller.validateParameters(
                parameterEditor) ||
            !userInstall.validateParameters(
                parameterEditor))
        {
            rp2040.fifo.push(
                interCoreMessageValue(
                    InterCoreMessage::MenuParametersRejected));
            break;
        }

        /*
         * Réglages de conduite seuls (consignes, commande manuelle) : ils
         * sont appliqués sans pause de l'acquisition ni état sûr des sorties,
         * puis sauvegardés par la boucle de contrôle.
         */
        if (actionId == MenuBuilder::NO_ACTION &&
            !acquisitionPausedForMenu &&
            parameterEditor.hasOnlyLiveChanges())
        {
            mutex_enter_blocking(&processDataMutex);

            const bool liveApplied = parameterEditor.apply();

            if (liveApplied)
                userInstall.onParametersApplied();

            mutex_exit(&processDataMutex);

            if (!liveApplied)
            {
                rp2040.fifo.push(
                    interCoreMessageValue(
                        InterCoreMessage::MenuParametersRejected));
                break;
            }

            requestConfigurationSave();
            menuSessionOpen = false;
            __dmb();
            rp2040.fifo.push(interCoreMessageValue(
                InterCoreMessage::MenuParametersApplied));
            break;
        }

        input.pause();
        input.resetAcquisition();
        acquisitionPausedForMenu = true;

        mutex_enter_blocking(&processDataMutex);
        controller.forceSafeOutputs();
        controlOutputsEnabled = false;
        sharedProcessSnapshot = ProcessSnapshot{};

        const bool outputsApplied =
            parameterEditor.apply() && controller.applyOutputSettings();

        if (outputsApplied)
            userInstall.onParametersApplied();

        mutex_exit(&processDataMutex);

        if (!outputsApplied)
        {
            rp2040.fifo.push(
                interCoreMessageValue(
                    InterCoreMessage::MenuParametersRejected));
            break;
        }

        bool actionSucceeded = true;
        bool sensorBoardAction = false;
        bool controllerAction = false;

        if (actionId != MenuBuilder::NO_ACTION)
        {
            mutex_enter_blocking(
                &processDataMutex);

            sensorBoardAction =
                input.handlesMenuAction(
                    actionId);

            controllerAction =
                controller.handlesMenuAction(
                    actionId);

            actionSucceeded =
                sensorBoardAction
                    ? input.executeMenuAction(
                          actionId)
                    : controllerAction
                        ? controller.executeMenuAction(
                              actionId)
                        : userInstall.executeMenuAction(
                              actionId);

            controller.forceSafeOutputs();

            mutex_exit(&processDataMutex);
        }

        const bool configurationSaved =
            storage.save(
                userInstall.configurationKey(),
                userInstall.getParameters());

        if (actionSucceeded &&
            actionId != MenuBuilder::NO_ACTION &&
            !configurationSaved)
        {
            mutex_enter_blocking(
                &processDataMutex);

            if (sensorBoardAction)
            {
                input.onMenuActionSaveFailed(
                    actionId);
            }
            // L'acquittement n'a rien à sauvegarder ni à annuler.
            else if (!controllerAction)
            {
                userInstall.onMenuActionSaveFailed(
                    actionId);
            }

            controller.forceSafeOutputs();

            mutex_exit(&processDataMutex);

            Serial.println(
                "Menu action cancelled because configuration save failed");
        }
        else if (actionSucceeded &&
                 sensorBoardAction &&
                 configurationSaved)
        {
            input.onMenuActionSaved(actionId);
        }

        configurationSavePending = false;

        if (!actionSucceeded)
        {
            Serial.println(
                "Menu action rejected");
        }

        input.resetAcquisition();
        input.startContinuous();

        acquisitionPausedForMenu = false;
        menuSessionOpen = false;
        controlOutputsEnabled = false;
        lastMeasurementTime = millis();

        __dmb();

        rp2040.fifo.push(
            interCoreMessageValue(
                configurationSaved
                    ? InterCoreMessage::MenuParametersApplied
                    : InterCoreMessage::MenuParametersAppliedNotSaved));
        break;
    }

    default:
        break;
    }
}

void OPC::handleUIMessage(
    InterCoreMessage message)
{
    switch (message)
    {
    case InterCoreMessage::ClockParametersCaptured:
        if (uiState != UIState::ClockCaptureRequested)
            break;
        __dmb();
        menu.refresh();
        lastMenuActivity = millis();
        uiState = UIState::Menu;
        break;

    case InterCoreMessage::ClockParametersApplied:
    case InterCoreMessage::ClockParametersRejected:
    {
        if (uiState != UIState::ClockApplyRequested)
            break;
        __dmb();
        const bool saved = message == InterCoreMessage::ClockParametersApplied;
        menu.clockParametersApplied(saved);
        lastMenuActivity = millis();
        uiState = UIState::Menu;
        break;
    }

    case InterCoreMessage::StartupFailed:
        if (uiState != UIState::Starting)
            break;

        // startupError a été écrit par le cœur contrôle avant l'envoi.
        __dmb();

        uiState = UIState::StartupFailed;
        showStartupError();
        lastStartupErrorPrint = millis();
        break;

    case InterCoreMessage::ParametersReady:
        initMenu();
        copyProcessSnapshot();
        uiState = UIState::Home;
        showHomeScreen(true);
        break;

    case InterCoreMessage::PrintDataAvailable:
    {
        FixedBufferPrint bufferedOutput(
            serialPrintBuffer,
            sizeof(serialPrintBuffer));

        mutex_enter_blocking(
            &processDataMutex);

        controller.printStatusEvents(bufferedOutput);
        controller.print(bufferedOutput);
        // Vieille méthode de print pour un CSV
        //controller.printCSVPsychro(bufferedOutput);

        // Juste une méthode temporaire pour avoir la temp de l'ADC sur un sweep
        //bufferedOutput.printf("ADC_temp : %.2f\n", input.getAdcTemperature());

        mutex_exit(&processDataMutex);

        if (uiState == UIState::Home)
        {
            copyProcessSnapshot();
            showHomeScreen(false);
        }

        Serial.write(
            serialPrintBuffer,
            bufferedOutput.size());
        break;
    }

    case InterCoreMessage::HomeSetpointApplied:
        if (uiState != UIState::HomeSetpointApplyRequested)
            break;

        __dmb();

        // Nouvelle consigne relue dans l'installation, puis affichage normal.
        copyProcessSnapshot();
        setpointEditor.end();
        uiState = UIState::Home;
        showHomeScreen(false);
        break;

    case InterCoreMessage::MenuParametersCaptured:
        if (uiState !=
                UIState::CaptureRequested)
        {
            break;
        }

        /* Le cœur contrôle a préparé l'état avant son acquittement. */
        __dmb();

        menu.show();

        lastMenuActivity = millis();
        uiState = UIState::Menu;
        break;

    case InterCoreMessage::MenuParametersApplied:
    case InterCoreMessage::MenuParametersAppliedNotSaved:
        if (uiState !=
                UIState::ApplyRequested)
        {
            break;
        }

        __dmb();

        if (message ==
            InterCoreMessage::MenuParametersAppliedNotSaved)
        {
            Serial.println(
                "Parameters applied but configuration save failed");
        }

        copyProcessSnapshot();

        uiState = UIState::Home;
        showHomeScreen(true);
        break;

    case InterCoreMessage::MenuParametersRejected:
        if (uiState !=
                UIState::ApplyRequested)
        {
            break;
        }

        Serial.println(
            "Menu parameter validation failed");

        menu.show();
        lastMenuActivity = millis();
        uiState = UIState::Menu;
        break;

    default:
        break;
    }
}

bool OPC::validateRestoredParameters(
    const ParameterEditor& editor) const
{
    return
        input.validateParameters(editor) &&
        controller.validateParameters(editor) &&
        userInstall.validateParameters(editor);
}
