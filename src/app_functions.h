////////////////////////////////////////////////////////////////////
/// @file app_functions.h
/// @brief Contains application specific includes. variables and functions
////////////////////////////////////////////////////////////////////

#ifndef APP_FUNCTIONS_H
#define APP_FUNCTIONS_H

#include "framework.h"

#ifdef USE_WEB_SERVER
#include "web_server.h"
#endif

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO) // 2 ^ GPIO_NUMBER in hex
#define USE_EXT0_WAKEUP 1                       // 1 = EXT0 wakeup, 0 = EXT1 wakeup
#define WAKEUP_GPIO GPIO_NUM_2                 // Only RTC IO are allowed - ESP32 Pin example

// ********* Framework App Parameters *****************

int appVersion = 1;

bool isFirstDraw = true;

// ********** Connectivity Parameters **********

// ********** App Global Variables **********

// Should be /internal/iot/firmware
const char *firmwareUrl = "/firmware/";
const char *appRootUrl = "/internal/iot/";

char appSubTopic[100];

char minute[3] = "00";
char currentTime[6] = "00:00";
char meridian[3] = "AM";

TimerHandle_t mqttImageSendTimer;

int baseFontSize = 72;
int appNameFontSize = 56;
int friendlyNameFontSize = 24;
int appInstanceIDFontSize = 18;
int timeFontSize = 128;
int storedImageIndex = 0;

bool rtspServerRunning = false;
int pin2Val = 0;

// ********** Possible Customizations Start ***********
char imageTopic[75];
int otherAppTopicCount = 0;
char otherAppTopic[10][25];
void (*otherAppMessageHandler[10])(char *topic, JsonDocument &doc);

void printTimestamp(Print *_logOutput, int x);
void logTimestamp();
void storePrefs();
void loadPrefs();
void ProcessMqttDisconnectTasks();
void ProcessMqttConnectTasks();
void ProcessWifiDisconnectTasks();
void ProcessWifiConnectTasks();
void appMessageHandler(char *topic, JsonDocument &doc);
void app_loop();
void app_setup();

// Example functions
void setupDisplay();
void initAppStrings();
void initWebServer();
bool checkGoodTime();
bool getNewTime();
void drawSplashScreen();
void drawTime();
void mqttPublishImage();

//////////////////////////////////////////
//// Customizable Functions
//////////////////////////////////////////
void drawSplashScreen()
{
    String oldMethodName = methodName;
    methodName = "drawSplashScreen()";
    Log.verboseln("Entering");

    char showText[100];
    if (appInstanceID < 0)
    {
        sprintf(showText, "Configuring...");
    }
    else
    {
        sprintf(showText, "Name: %s", friendlyName);
    }
    sprintf(showText, "Device ID: %i", appInstanceID);
#ifdef USE_OPEN_FONT_RENDERER
    drawString(appName, screenCenterX, screenCenterY, appNameFontSize);

    drawString(showText, screenCenterX, screenCenterY + appNameFontSize / 2 + friendlyNameFontSize, friendlyNameFontSize);

    drawString(showText, screenCenterX, tft.height() - appInstanceIDFontSize / 2, appInstanceIDFontSize);
#endif

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void setupDisplay()
{
    String oldMethodName = methodName;
    methodName = "setupDisplay()";
    Log.verboseln("Entering");

    Log.infoln("Setting up display.");

#ifdef USE_GRAPHICS
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);
#endif

#ifdef USE_OPEN_FONT_RENDERER
    ofr.setDrawer(tft);
    ofr.loadFont(NotoSans_Bold, sizeof(NotoSans_Bold));
    ofr.setFontColor(TFT_WHITE, TFT_BLACK);
    ofr.setFontSize(baseFontSize);
    ofr.setAlignment(Align::MiddleCenter);
#endif

    drawSplashScreen();

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void initAppStrings()
{
    sprintf(appSubTopic, "%s/#", appName);
    sprintf(imageTopic, "%s/image", appName);
}

void mqttPublishImage()
{
    String oldMethodName = methodName;
    methodName = "mqttPublishImage()";
    Log.verboseln("Entering");

    if (!mqttConnected)
    {
        Log.infoln("MQTT not connected. Caching images to SD.");
        char ifn[30];
        sprintf(ifn, "/imgtemp/cap_%i.jpg", storedImageIndex);
        Log.infoln("Opening %s", ifn);
        /*
        File file = SD_MMC.open(ifn, FILE_WRITE);
        if (file)
        {
            Log.infoln("Opened %s", ifn);
            cam.run();
            file.write(cam.getfb(), cam.getSize()); // payload (image), payload length
            file.close();
            Log.infoln("Saved image to: %s\n", ifn);
            storedImageIndex++;
        }
        else
        {
            Log.errorln("Failed to open file (%s) in writing mode!!!", ifn);
        }
        */
    }
    else
    {
        if (storedImageIndex > 0)
        {
            Log.infoln("Sending cached images to MQTT.");
            for (int i = 0; i < storedImageIndex; i++)
            {
                char ifn[30];
                sprintf(ifn, "/imgtmp/cap_%i", i);
                File file = SD.open(ifn, "r");
                if (!file)
                {
                    Log.errorln("Failed to open file (%s) in reading mode!!!", ifn);
                }

                Log.verboseln("Reading file: %s", ifn);
                int len = file.size();
                char imgBuf[len + 1];
                file.readBytes(imgBuf, len);
                imgBuf[len + 1] = '\0';
                file.close();
                Log.verboseln("Sending via MQTT.");
                int pubRes = mqttClient.publish(imageTopic, 1, false, imgBuf, len);
                // delete[] imgBuf;
            }
        }

        Log.infoln("Sending current image via MQTT.");
        cam.run();

        int pubRes = mqttClient.publish(imageTopic, 1, false, (char *)cam.getfb(), cam.getSize());
    }

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void initRTSPServer()
{
    String oldMethodName = methodName;
    methodName = "initRTSPServer()";
    Log.verboseln("Entering...");

    Log.infoln("Starting RTSP Server.");
    rtspServer.setTimeout(1);
    rtspServer.begin();
    rtspServerRunning = true;

    streamer = new OV2640Streamer(&cam);

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void handleRTSP_loop()
{
    String oldMethodName = methodName;
    methodName = "ProcessAppWifiConnectTasks()";
    Log.verboseln("Entering...");

    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();
    // If we have an active client connection, just service that until gone
    streamer->handleRequests(0); // we don't use a timeout here,
    // instead we send only if we have new enough frames
    uint32_t now = millis();
    if (streamer->anySessions())
    {
        if (now > lastimage + msecPerFrame || now < lastimage)
        { // handle clock rollover
            streamer->streamImage(now);
            lastimage = now;
            // check if we are overrunning our max frame rate
            now = millis();
            if (now > lastimage + msecPerFrame)
            {
                Log.warningln("warning exceeding max frame rate of %d ms", now - lastimage);
            }
        }
    }
    // rtsp client / server
    WiFiClient rtspClient = rtspServer.accept();
    if (rtspClient)
    {
        Log.infoln("Connecting RTSP Client: %s", rtspClient.remoteIP().toString());

        streamer->addSession(&rtspClient);
    }

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void ProcessWifiConnectTasks()
{
    String oldMethodName = methodName;
    methodName = "ProcessAppWifiConnectTasks()";
    Log.verboseln("Entering...");

#ifdef USE_WEB_SERVER
    initWebServer();
#endif

#ifdef USE_RTSP
    initRTSPServer();
#endif

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void ProcessWifiDisconnectTasks()
{
    String oldMethodName = methodName;
    methodName = "ProcessAppWifiDisconnectTasks()";
    Log.verboseln("Entering...");

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void ProcessMqttConnectTasks()
{
    String oldMethodName = methodName;
    methodName = "ProcessMqttConnectTasks()";
    Log.verboseln("Entering...");

    uint16_t packetIdSub1 = mqttClient.subscribe(appSubTopic, 2);
    if (packetIdSub1 > 0)
        Log.infoln("Subscribing to %s at QoS 2, packetId: %u", appSubTopic, packetIdSub1);
    else
        Log.errorln("Failed to subscribe to %s!!!", appSubTopic);

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void ProcessMqttDisconnectTasks()
{
    String oldMethodName = methodName;
    methodName = "ProcessMqttDisconnectTasks()";
    Log.verboseln("Entering...");

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void appMessageHandler(char *topic, JsonDocument &doc)
{
    String oldMethodName = methodName;
    methodName = "appMessageHandler()";
    Log.verboseln("Entering...");

    // Add your implementation here
    char topics[10][25];
    int topicCounter = 0;
    char *token = strtok(topic, "/");

    while ((token != NULL) && (topicCounter < 11))
    {
        strcpy(topics[topicCounter++], token);
        token = strtok(NULL, "/");
    }

    // We can assume the first 2 subtopics are the appName and the appInstanceID
    // The rest of the subtopics are the command

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
    return;
}

void loadPrefs()
{
    String oldMethodName = methodName;
    methodName = "loadPrefs()";
    Log.verboseln("Entering...");

    bool doesExist = preferences.isKey("appInstanceID");
    if (doesExist)
    {
        Log.infoln("Loading settings.");
        appInstanceID = preferences.getInt("appInstanceID");
        volume = preferences.getInt("Volume");
        bootCount = preferences.getInt("BootCount");
        preferences.getString("FriendlyName", friendlyName, 100); // 100 is the max length of the string
        // enableSnapshot = preferences.getBool("EnableSnapshot");
    }
    else
    {
        Log.warningln("Could not find Preferences!");
        Log.noticeln("appInstanceID not set yet!");
    }

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void storePrefs()
{
    String oldMethodName = methodName;
    methodName = "storePrefs()";
    Log.verboseln("Entering...");

    Log.infoln("Storing Preferences.");

    preferences.putInt("appInstanceID", appInstanceID);
    preferences.putInt("Volume", volume);
    preferences.putInt("BootCount", bootCount);
    preferences.putString("FriendlyName", friendlyName);
    // preferences.putBool("EnableSnapshot", enableSnapshot);

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void logTimestamp()
{
    String oldMethodName = methodName;
    methodName = "logTimestamp()";
    Log.verboseln("Entering...");

    char c[20];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (timeinfo->tm_year == 70)
    {
        sprintf(c, "%10lu ", millis());
    }
    else
    {
        strftime(c, 20, "%Y%m%d %H:%M:%S", timeinfo);
    }

    Log.infoln("Time: %s", c);

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

void printTimestamp(Print *_logOutput, int x)
{
    char c[20];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (timeinfo->tm_year == 70)
    {
        sprintf(c, "%10lu ", millis());
    }
    else
    {
        strftime(c, 20, "%Y%m%d %H:%M:%S", timeinfo);
    }
    _logOutput->print(c);
    _logOutput->print(": ");
    _logOutput->print(methodName);
    _logOutput->print(": ");
}

bool checkGoodTime()
{
    String oldMethodName = methodName;
    methodName = "checkGoodTime()";
    Log.verboseln("Entering...");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Log.errorln("Failed to obtain time");
        return false;
    }

    if (timeinfo.tm_year < (2020 - 1900))
    {
        Log.errorln("Failed to obtain time");
        return false;
    }

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
    return true;
}

bool getNewTime()
{
    String oldMethodName = methodName;
    methodName = "getNewTime()";
    Log.verboseln("Entering...");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Log.errorln("Failed to obtain time");
        Log.verboseln("Exiting...");
        methodName = oldMethodName;
        return false;
    }

    char newTime[6] = "00:00";
    strftime(newTime, 20, "%I:%M", &timeinfo);
    strftime(meridian, 3, "%p", &timeinfo);

    if (strcmp(newTime, currentTime) != 0)
    {
        strcpy(currentTime, newTime);
        Log.infoln("Time is now %s %s", currentTime, meridian);
        Log.verboseln("Exiting...");
        methodName = oldMethodName;
        return true;
    }

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
    return false;
}

void redrawScreen()
{
    drawTime();
}

void drawTime()
{
    String oldMethodName = methodName;
    methodName = "drawTime()";
    Log.verboseln("Entering...");

#ifdef USE_GRAPHICS
    tft.fillScreen(TFT_BLACK);

#ifdef USE_OPEN_FONT_RENDERER
    drawString(currentTime, screenCenterX, screenCenterY, timeFontSize);
#endif

#endif

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

/*
IRAM_ATTR void mailboxOpened()
{
    Log.infoln("Going to sleep now");
    esp_deep_sleep_start();
}
*/

void mailboxClosed()
{
    Log.infoln("Going to sleep now");
    WiFi.mode(WIFI_OFF);
    adc_power_off();
    esp_deep_sleep_start();
}

void app_setup()
{
    String oldMethodName = methodName;
    methodName = "app_setup()";
    Log.verboseln("Entering...");

    // Add some custom code here
    initAppStrings();

    // Configure Hardware
    Log.infoln("Configuring hardware.");
    pinMode(WAKEUP_GPIO, INPUT);
    //attachInterrupt(digitalPinToInterrupt(WAKEUP_GPIO), mailboxOpened, FALLING);
    // create a rising interrupt on GPIO 13

    Log.infoln("Configuring wakeup.");
    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK(WAKEUP_GPIO), ESP_EXT1_WAKEUP_ANY_HIGH);

    /*
  If there are no external pull-up/downs, tie wakeup pins to inactive level with internal pull-up/downs via RTC IO
       during deepsleep. However, RTC IO relies on the RTC_PERIPH power domain. Keeping this power domain on will
       increase some power comsumption. However, if we turn off the RTC_PERIPH domain or if certain chips lack the RTC_PERIPH
       domain, we will use the HOLD feature to maintain the pull-up and pull-down on the pins during sleep.
*/
    rtc_gpio_pulldown_en(WAKEUP_GPIO); // GPIO33 is tie to GND in order to wake up in HIGH
    rtc_gpio_pullup_dis(WAKEUP_GPIO);  // Disable PULL_UP in order to allow it to wakeup on HIGH

    mqttImageSendTimer = xTimerCreate("mqttImageSendTimer", pdMS_TO_TICKS(200), pdTRUE,
                                      (void *)0, reinterpret_cast<TimerCallbackFunction_t>(mqttPublishImage));

/*
    File root = SD.open("/");
    if (!root)
    {
        Log.errorln("Failed to open directory");
    }
    else
    {
        printDirectory(root, 0);
    }
    root.close();
*/
    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    // uncomment if LED is still soldered to GPIO4
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    gpio_hold_en(GPIO_NUM_4);

    //xTimerStart(mqttImageSendTimer, pdMS_TO_TICKS(5000));

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}

////////////////////////////////////////////////////////////////////
/// @fn void app_loop()
/// @brief Called by the framework once per loop after all framework
/// processing is complete
////////////////////////////////////////////////////////////////////

void app_loop()
{


    if (digitalRead(WAKEUP_GPIO) == 0)
        mailboxClosed();

            if (rtspServerRunning)
                handleRTSP_loop();

    if ((millis() % 1000) == 0)
    {
        if (!isGoodTime)
        {
            if (!(isGoodTime = checkGoodTime()))
                Log.infoln("Time not set yet.");
        }

#ifdef USE_GRAPHICS

        if (isFirstDraw)
        {
            isFirstDraw = false;
            clearScreen();
            redrawScreen();
        }

        // put your main code here, to run repeatedly:
        if (getNewTime())
        {
            drawTime();
        }
#endif
    }
}
#endif // APP_FUNCTIONS_H