#include "OPC.h"

#include <SPI.h>
#include <Wire.h>
#include <Hardware/pinout.h>
#include <hardware/sync.h>

#include <hmi/HomeScreen.h>

namespace
{
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

OPC::OPC() : tft(&SPI1, LCD_CS, LCD_DC, -1)
{
    mutex_init(&processDataMutex);
    pinMode(LCD_RESET, OUTPUT);
    digitalWrite(LCD_RESET, 1);

}



void OPC::initSerial()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println("Open Process Controller v0.3");
}



void OPC::initDisplay()
{
    SPI1.setSCK(LCD_SCK);
    SPI1.setTX(LCD_MOSI);

    tft.init(135, 240);
    tft.setRotation(3);
    tft.setSPISpeed(48000000);
    tft.setTextWrap(false);
    tft.cp437(true);
    tft.fillScreen(ST77XX_BLACK);
}

void OPC::initBME280()
{
    Wire.setSDA(BME_SDA);
    Wire.setSCL(BME_SCL);

    bmeInitialized =
        bme.begin(0x76, &Wire);

    if (!bmeInitialized)
    {
        Serial.println("BME280 initialization failed");
        return;
    }

    bme.setSampling(
        Adafruit_BME280::MODE_NORMAL,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::SAMPLING_X16,
        Adafruit_BME280::FILTER_OFF,
        Adafruit_BME280::STANDBY_MS_0_5);
}

void OPC::initSensorBoard()
{
    // Set pin 23 HIGH to switch the pico DC-DC converter to PWM (improved ripple)
	// Improves a lot measurement stability
    pinMode(23,OUTPUT);
    digitalWrite(23,HIGH);

    input.init();
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
    if(!input.newMeasurement)
        return false;

    input.newMeasurement = false;
    
    // Faire la MAJ des mesures (conversion data -> mesure)
    unsigned long times = millis();

    mutex_enter_blocking(
        &processDataMutex);

    if (!controlOutputsEnabled)
        controller.resume(times);

    controller.updateMeasurementsAndRegulators(
        times);

    controller.captureMeasurements(
        sharedMeasurementSnapshot,
        times);

    lastMeasurementTime = times;

    controlOutputsEnabled =
        controller.outputsHealthy();

    if (!controlOutputsEnabled)
        controller.forceSafeOutputs();

    mutex_exit(&processDataMutex);

    rp2040.fifo.push_nb(PRINT_DATA_AVAILABLE);

    return true;
}

void OPC::controlPoll()
{
    storage.poll();

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
    if (userInstall.requiresBME280() &&
        !bmeInitialized)
    {
        Serial.println(
            "BME280 required by installation but unavailable");
        return false;
    }

    if (!userInstall.prepareParameterRegistration())
    {
        Serial.println(
            "Parameter storage initialization failed");
        return false;
    }

    if (!userInstall.begin(
            input,
            bme,
            controller))
    {
        Serial.println(
            "Installation initialization failed");
        return false;
    }

    if (!userInstall.completeParameterRegistration())
    {
        Serial.println(
            "Framework parameter registration failed");
        return false;
    }

    const bool storageReady =
        storage.begin();

    const Storage::RestoreResult restoreResult =
        storage.restore(
            userInstall.name(),
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
    {
        Serial.println(
            "Output initialization failed");
        return false;
    }

    controlOutputsEnabled = false;
    lastMeasurementTime = millis();

    input.startContinuous();

    return true;
}

void OPC::initMenu()
{
    if (menu.isInitialized())
        return;

    parameterEditor.capture();

    if (!menuDefinition.build(
            userInstall.getParameters(),
            "Parametres"))
    {
        Serial.println("Automatic menu generation failed");
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

void OPC::copyMeasurementSnapshot()
{
    mutex_enter_blocking(
        &processDataMutex);

    displayMeasurementSnapshot =
        sharedMeasurementSnapshot;

    mutex_exit(&processDataMutex);
}

void OPC::showHomeScreen(
    bool fullRefresh)
{
    HomeScreenContext context{
        tft,
        displayMeasurementSnapshot,
        millis(),
        fullRefresh
    };

    userInstall.printHomeScreen(context);
}

void OPC::requestMenu()
{
    if (uiState != UIState::Home ||
        !menu.isInitialized())
    {
        return;
    }

    uiState = UIState::PauseRequested;

    rp2040.fifo.push(
        PAUSE_ADC_INTERRUPTS);
}

void OPC::requestParameterApply()
{
    if (uiState != UIState::Menu)
        return;

    menu.close();

    uiState = UIState::ApplyRequested;

    /*
     * Les brouillons ont été écrits par le coeur 1.
     * La barrière garantit leur visibilité avant
     * l'envoi de la commande au coeur 0.
     */
    __dmb();

    rp2040.fifo.push(
        APPLY_MENU_PARAMETERS);
}

void OPC::uiPoll()
{
    int32_t movement =
        encoder.takeRotation();

    const bool clicked =
        encoder.takeClick();

    if (uiState == UIState::Home)
    {
        if (clicked)
            requestMenu();

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

    bool exitRequested = false;

    if (clicked)
        exitRequested = menu.enter();

    menu.poll();

    const uint32_t now = millis();

    if (hadActivity)
        lastMenuActivity = now;

    const uint32_t timeout =
        userInstall.menuTimeoutMs();

    const bool timedOut =
        (now - lastMenuActivity) >= timeout;

    if (exitRequested || timedOut)
        requestParameterApply();
}

void OPC::handleControlMessage(
    uint32_t message)
{
    switch (message)
    {
    case PAUSE_ADC_INTERRUPTS:
        if (!acquisitionPausedForMenu)
        {
            input.pause();
            input.resetAcquisition();

            mutex_enter_blocking(
                &processDataMutex);

            controller.forceSafeOutputs();
            controlOutputsEnabled = false;

            mutex_exit(&processDataMutex);

            acquisitionPausedForMenu = true;
        }

        rp2040.fifo.push(
            ACQUISITION_PAUSED);
        break;

    case APPLY_MENU_PARAMETERS:
        if (!acquisitionPausedForMenu)
        {
            rp2040.fifo.push(
                MENU_PARAMETERS_REJECTED);
            break;
        }

        /*
         * La commande FIFO vient du coeur 1 :
         * les brouillons ne seront plus modifiés tant
         * que la réponse n'aura pas été reçue.
         */
        __dmb();

        if (!parameterEditor.validate() ||
            !input.validateParameters(
                parameterEditor) ||
            !controller.validateParameters(
                parameterEditor) ||
            !userInstall.validateParameters(
                parameterEditor) ||
            !parameterEditor.apply())
        {
            rp2040.fifo.push(
                MENU_PARAMETERS_REJECTED);
            break;
        }

        mutex_enter_blocking(
            &processDataMutex);

        const bool outputsApplied =
            controller.applyOutputSettings();

        mutex_exit(&processDataMutex);

        if (!outputsApplied)
        {
            rp2040.fifo.push(
                MENU_PARAMETERS_REJECTED);
            break;
        }

        userInstall.onParametersApplied();

        const bool configurationSaved =
            storage.save(
                userInstall.name(),
                userInstall.getParameters());

        input.resetAcquisition();
        input.startContinuous();

        acquisitionPausedForMenu = false;
        controlOutputsEnabled = false;
        lastMeasurementTime = millis();

        __dmb();

        rp2040.fifo.push(
            configurationSaved
                ? MENU_PARAMETERS_APPLIED
                : MENU_PARAMETERS_APPLIED_NOT_SAVED);
        break;
    }
}

void OPC::handleUIMessage(
    uint32_t message)
{
    switch (message)
    {
    case PARAMETERS_READY:
        initMenu();
        copyMeasurementSnapshot();
        uiState = UIState::Home;
        showHomeScreen(true);
        break;

    case PRINT_DATA_AVAILABLE:
    {
        FixedBufferPrint bufferedOutput(
            serialPrintBuffer,
            sizeof(serialPrintBuffer));

        mutex_enter_blocking(
            &processDataMutex);

        controller.print(bufferedOutput);
        //controller.printCSVPsychro(bufferedOutput);

        mutex_exit(&processDataMutex);

        if (uiState == UIState::Home)
        {
            copyMeasurementSnapshot();
            showHomeScreen(false);
        }

        Serial.write(
            serialPrintBuffer,
            bufferedOutput.size());
        break;
    }

    case ACQUISITION_PAUSED:
        if (uiState !=
                UIState::PauseRequested)
        {
            break;
        }

        parameterEditor.capture();
        menu.show();

        lastMenuActivity = millis();
        uiState = UIState::Menu;
        break;

    case MENU_PARAMETERS_APPLIED:
    case MENU_PARAMETERS_APPLIED_NOT_SAVED:
        if (uiState !=
                UIState::ApplyRequested)
        {
            break;
        }

        __dmb();

        if (message ==
            MENU_PARAMETERS_APPLIED_NOT_SAVED)
        {
            Serial.println(
                "Parameters applied but configuration save failed");
        }

        displayMeasurementSnapshot =
            MeasurementSnapshot{};

        uiState = UIState::Home;
        showHomeScreen(true);
        break;

    case MENU_PARAMETERS_REJECTED:
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
