////////////////////////////////////////////////////////////////////
/// @file framework.h
/// @brief Contains all Framework #includes, globals and core functions
////////////////////////////////////////////////////////////////////

#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <Arduino.h>
#include <time.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}

#include <FS.h>

#ifndef APP_NAME
#define APP_NAME ESP32FWApp
#endif

#ifndef SECRETS_H
#define SECRETS_H

#define NTP_SERVER "pool.ntp.org"

#define WIFI_SSID "APName"
#define WIFI_PASSWORD "APPassword"

#define HTTP_SERVER "192.168.0.12"
#define HTTP_PORT 5000

#define MQTT_HOST IPAddress(192, 168, 0, 200)
#define MQTT_PORT 1883

#define LATITUDE 37.3380937
#define LONGITUDE -121.8853892

#define APP_KEY ""
#define APP_SECRET "536CB6A57A55C82BEDD22A9566A47"

#define RTSP_PORT 554

#endif // SECRETS_H

#include <WiFi.h>
#include <Preferences.h>
#include <SPI.h>

#include <LittleFS.h>

#include <Update.h>

#ifdef USE_DEEP_SLEEP
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
//#include "driver/adc.h"
#endif


#ifdef USE_WEB_SERVER
#include <ESPAsyncWebServer.h>
AsyncWebServer webServer(80);
#endif

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

#include <ArduinoLog.h>

#include <TLogPlus.h>

// using namespace TLogPlus;


#ifdef WEBSTREAM_LOGGING
#include <WebSerialStream.h>
using namespace TLogPlusStream;
WebSerialStream webSerialStream = WebSerialStream();
#endif

#ifdef SYSLOG_LOGGING
#include <SyslogStream.h>
using namespace TLogPlusStream;
SyslogStream syslogStream = SyslogStream();
#endif

#ifdef MQTT_LOGGING
#include <MqttlogStream.h>
#include "main.h"
using namespace TLogPlusStream;
// EthernetClient client;
WiFiClient client;
MqttStream mqttStream = MqttStream(&client);
char topic[128] = "log/foo";
#endif

#define minimum(a, b) (((a) < (b)) ? (a) : (b))
#define maximum(a, b) (((a) > (b)) ? (a) : (b))

const char *appName = APP_NAME;
const char *appSecret = APP_SECRET;
#define FIRMWARE_VERSION "v0.0.1"

const char *ntpServer = NTP_SERVER;
String hostname = APP_NAME;

int appInstanceID = -1;
char friendlyName[100] = "NoNameSet";

bool isFirstLoop = true;
bool isGoodTime = false;

// ********** Connectivity Parameters **********
AsyncMqttClient mqttClient;

int volume = 50; // Volume is %
int bootCount = 0;
esp_sleep_wakeup_cause_t wakeup_reason;
esp_reset_reason_t reset_reason;
int maxOtherIndex = -1;
bool shouldReboot = false;
Preferences preferences;
bool mqttConnected = false;
bool wifiConnected = false;

uint8_t macAddress[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// **************** Debug Parameters ************************
String methodName = "";

#pragma region Standard Helper Functions

void reboot(const char *message)
{
    Log.infoln("Rebooting ESP32: %s", message);
    ESP.restart();
}

bool isNullorEmpty(char *str)
{
    if ((str == NULL) || (str[0] == '\0'))
        return true;
    else
        return false;
}

bool isNullorEmpty(String str)
{
    return isNullorEmpty(str.c_str());
}

// check a string to see if it is numeric
bool isNumeric(char *str)
{
    for (byte i = 0; str[i]; i++)
    {
        if (!isDigit(str[i]))
            return false;
    }
    return true;
}

// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes)
{
    if (bytes < 1024)
        return String(bytes) + " B";
    else if (bytes < (1024 * 1024))
        return String(bytes / 1024.0) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        return String(bytes / 1024.0 / 1024.0) + " MB";
    else
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

#pragma endregion

#pragma region File System

void initFS()
{
#ifndef LittleFS
    // Initialize LittleFS
    if (!LittleFS.begin(true))
    {
        Log.errorln("An Error has occurred while mounting LittleFS");
        return;
    }
#else
    if (!LittleFS.begin())
    {
        Log.errorln("Flash FS initialisation failed!");
        while (1)
            yield(); // Stay here twiddling thumbs waiting
    }
#endif
    Log.infoln("Flash FS available!");
}

#pragma endregion

#pragma region ESP32-CAM

#ifdef USE_ESP32_CAM
#include "OV2640.h"

OV2640 cam;
#define RESOLUTION FRAMESIZE_XGA // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
#define QUALITY 10               // JPEG quality 10-63 (lower means better quality)
#define PIN_FLASH_LED 4          // GPIO4 for AIThinker module, set to -1 if not needed!

void initCAM()
{
    camera_config_t cconfig;
    cconfig = esp32cam_aithinker_config;
    if (psramFound() && psramInit())
    {
        Log.infoln("Configuring CAM to use PSRAM");
        cconfig.frame_size = RESOLUTION; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
        cconfig.jpeg_quality = QUALITY;
        cconfig.fb_count = 2;
    }
    else
    {
        Log.warningln("PSRAM NOT FOUND. Configuring CAM normally.");
        if (RESOLUTION > FRAMESIZE_SVGA)
        {
            cconfig.frame_size = FRAMESIZE_SVGA;
        }
        cconfig.jpeg_quality = 12;
        cconfig.fb_count = 1;
    }

    if (PIN_FLASH_LED > -1)
    {
        // pinMode(PIN_FLASH_LED, OUTPUT);
        // setflash(0);
    }

    if (cam.init(cconfig) != 0)
        Log.errorln("Failed to configure camera!");
    else
        Log.infoln("Camera configuration complete.");
}

#endif

#pragma endregion

#pragma region Display and Drawing Functions

#ifdef USE_GRAPHICS

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI(); // Create object "tft"
SPIClass tftspi = tft.getSPIinstance();

int screenWidth = tft.width();
int screenHeight = tft.height();
int screenCenterX = tft.width() / 2;
int screenCenterY = tft.height() / 2;

void clearScreen()
{
    tft.fillScreen(TFT_BLACK);
}

#endif

#pragma endregion

#pragma region OpenFontRenderer Drawing Functions

#ifdef USE_OPEN_FONT_RENDERER

#include <OpenFontRender.h>

OpenFontRender ofr;

void drawString(String text, int x, int y)
{
    ofr.setCursor(x, y);
    if (ofr.getAlignment() == Align::MiddleCenter)
    {
        ofr.setCursor(x, y - ofr.getFontSize() / 5 - 2);
    }
    ofr.printf(text.c_str());
}

void drawString(String text, int x, int y, int font_size)
{
    ofr.setFontSize(font_size);
    drawString(text, x, y);
}

void drawString(String text, int x, int y, int font_size, int color)
{
    ofr.setFontColor(color);
    drawString(text, x, y, font_size);
}

void drawString(String text, int x, int y, int font_size, int color, int bg_color)
{
    ofr.setFontColor(color, bg_color);
    drawString(text, x, y, font_size);
}

#endif

#pragma endregion

#pragma region SD Card Functions

#ifdef USE_SD_CARD
#include <SD_MMC.h>
#define SD SD_MMC
//#define SD_CS 5

// printDirectory
void printDirectory(File dir, int numTabs)
{
    while (true)
    {
        File entry = dir.openNextFile();
        String res = "";
        if (!entry)
        {

            // no more files
            break;
        }
        for (uint8_t i = 0; i < numTabs; i++)
        {
            res += " ";
        }
        res += entry.name();
        if (entry.isDirectory())
        {
            res += "/";
            Log.infoln(res.c_str());
            printDirectory(entry, numTabs + 1);
        }
        else
        {

            // Files have sizes, directories do not.
            res += "  ";
            uint32_t tSize = static_cast<uint32_t>(entry.size());
            res += String(tSize);
            Log.infoln(res.c_str());
        }
        entry.close();
    }
}

void initSD()
{
    String oldMethodName = methodName;
    methodName = "initSD()";
    Log.verboseln("Entering...");

#ifdef USE_GRAPHICS1
    if (!SD.begin(SD_CS, tftspi))
    {
        Log.errorln("SD Card Mount Failed");
        return;
    }
#else
    if (!SD.begin("/sdcard", true))
    {
        Log.errorln("SD Card Mount Failed");
        return;
    }
#endif

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE)
    {
        Log.errorln("No SD card attached");
        return;
    }
    Log.infoln("SD Card Type: %s", cardType == CARD_MMC ? "MMC" : cardType == CARD_SD ? "SDSC"
                                                              : cardType == CARD_SDHC ? "SDHC"
                                                                                      : "UNKNOWN");

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Log.infoln("SD Card Size: %l MB", cardSize);

    File root = SD.open("/");
    if (!root)
    {
        Log.errorln("Failed to open SD Card!!!");
    }
    else
    {
        printDirectory(root, 0);
        root.close();
    }

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
}
#endif

#pragma endregion

#pragma region Audio Functions

#ifdef USE_AUDIO
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

AudioGeneratorMP3 *mp3;
AudioOutputI2S *out;
bool mp3Done = true;

/// @fn void initAudioOutput()
/// @brief The only function to

void initAudioOutput()
{
    out = new AudioOutputI2S(0, 2, 8, -1); // Output to builtInDAC
    out->SetOutputModeMono(true);
    out->SetGain(1.0);
}

void playMP3(char *filename)
{
    AudioFileSourceSD *file;
    AudioFileSourceID3 *id3;

    file = new AudioFileSourceSD(filename);
    id3 = new AudioFileSourceID3(file);
    mp3 = new AudioGeneratorMP3();
    if (!mp3->begin(id3, out))
    {
        Log.errorln("Failed to begin MP3 decode.");
    }
    else
    {
        mp3Done = false;
    }
}

void playMP3Loop()
{
    if (mp3->isRunning())
    {
        if (!mp3->loop())
        {
            mp3->stop();
            mp3Done = true;
        }
    }
    else
    {
        mp3Done = true;
    }
}

#endif

#pragma endregion

#pragma region PNG Decoder Functions

#ifdef USE_PNG_DECODER

#include <PNGdec.h>

#define MAX_IMAGE_WIDTH 320

// PNGDec Veriables
File pngfile;
PNG png;
int32_t xPos = 0;
int32_t yPos = 0;

// PNGDec File I/O Helper Functions
void *pngOpen(const char *filename, int32_t *size)
{
    Log.verboseln("Attempting to open %s\n", filename);
    pngfile = LittleFS.open(filename, "r");
    if (!pngfile)
        Log.errorln("Failed to open %s", filename);
    else
        *size = pngfile.size();
    return &pngfile;
}

#ifdef USE_SD_CARD
void *pngOpenSD(const char *filename, int32_t *size)
{
    Log.verboseln("Attempting to open %s\n", filename);
    pngfile = SD.open(filename, "r");
    if (!pngfile)
        Log.errorln("Failed to open %s", filename);
    else
        *size = pngfile.size();
    return &pngfile;
}
#endif

void pngClose(void *handle)
{
    File pngfile = *((File *)handle);
    if (pngfile)
        pngfile.close();
}

int32_t pngRead(PNGFILE *page, uint8_t *buffer, int32_t length)
{
    if (!pngfile)
        return 0;
    page = page; // Avoid warning
    return pngfile.read(buffer, length);
}

int32_t pngSeek(PNGFILE *page, int32_t position)
{
    if (!pngfile)
        return 0;
    page = page; // Avoid warning
    return pngfile.seek(position);
}

/// @brief PNGDecoder Helper function to allow the decoder engine to draw a single line
///        of the image
/// @param pDraw
void pngDraw(PNGDRAW *pDraw)
{
    uint16_t lineBuffer[MAX_IMAGE_WIDTH];
    static uint16_t dmaBuffer[MAX_IMAGE_WIDTH]; // static so buffer persists after fn exit

    png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    tft.pushImage(xPos, yPos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
}

void drawPNG(const char *filename, int x, int y)
{

    int16_t rc = png.open(filename, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    xPos = x;
    yPos = y;
    if (rc == PNG_SUCCESS)
    {
        tft.startWrite();
        Log.verboseln("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
        uint32_t dt = millis();
        if (png.getWidth() > MAX_IMAGE_WIDTH)
        {
            Serial.println("Image too wide for allocated lin buffer!");
        }
        else
        {
            rc = png.decode(NULL, 0);
            png.close();
        }
        tft.endWrite();
    }
}

#ifdef USE_SD_CARD

void drawPNGFromSD(const char *filename, int x, int y)
{

    int16_t rc = png.open(filename, pngOpenSD, pngClose, pngRead, pngSeek, pngDraw);
    xPos = x;
    yPos = y;
    if (rc == PNG_SUCCESS)
    {
        tft.startWrite();
        Log.verboseln("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
        uint32_t dt = millis();
        if (png.getWidth() > MAX_IMAGE_WIDTH)
        {
            Serial.println("Image too wide for allocated lin buffer!");
        }
        else
        {
            rc = png.decode(NULL, 0);
            png.close();
        }
        tft.endWrite();
    }
}
#endif

#endif

#pragma endregion

#pragma region JPEG Decoder Functions

#ifdef USE_JPEG_DECODER

#include <JPEGDecoder.h>

void renderJPEG(int xpos, int ypos)
{

    // retrieve information about the image
    uint16_t *pImg;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    uint32_t max_x = JpegDec.width;
    uint32_t max_y = JpegDec.height;

    // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
    // Typically these MCUs are 16x16 pixel blocks
    // Determine the width and height of the right and bottom edge image blocks
    uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
    uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an image
    uint32_t drawTime = millis();

    // save the coordinate of the right and bottom edges to assist image cropping
    // to the screen size
    max_x += xpos;
    max_y += ypos;

    // read each MCU block until there are no more
    while (JpegDec.read())
    {

        // save a pointer to the image block
        pImg = JpegDec.pImage;

        // calculate where the image block should be drawn on the screen
        int mcu_x = JpegDec.MCUx * mcu_w + xpos; // Calculate coordinates of top left corner of current MCU
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        // check if the image block size needs to be changed for the right edge
        if (mcu_x + mcu_w <= max_x)
            win_w = mcu_w;
        else
            win_w = min_w;

        // check if the image block size needs to be changed for the bottom edge
        if (mcu_y + mcu_h <= max_y)
            win_h = mcu_h;
        else
            win_h = min_h;

        // copy pixels into a contiguous block
        if (win_w != mcu_w)
        {
            uint16_t *cImg;
            int p = 0;
            cImg = pImg + win_w;
            for (int h = 1; h < win_h; h++)
            {
                p += mcu_w;
                for (int w = 0; w < win_w; w++)
                {
                    *cImg = *(pImg + w + p);
                    cImg++;
                }
            }
        }

        // calculate how many pixels must be drawn
        uint32_t mcu_pixels = win_w * win_h;

        tft.startWrite();

        // draw image MCU block only if it will fit on the screen
        if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
        {

            // Now set a MCU bounding window on the TFT to push pixels into (x, y, x + width - 1, y + height - 1)
            tft.setAddrWindow(mcu_x, mcu_y, win_w, win_h);

            // Write all MCU pixels to the TFT window
            while (mcu_pixels--)
            {
                // Push each pixel to the TFT MCU area
                tft.pushColor(*pImg++);
            }
        }
        else if ((mcu_y + win_h) >= tft.height())
            JpegDec.abort(); // Image has run off bottom of screen so abort decoding

        tft.endWrite();
    }

    // calculate how long it took to draw the image
    drawTime = millis() - drawTime;

    // print the results to the serial port
    // Log.infoln("renderJPEG(): Total render time was: %ims", drawTime);
}

void drawArrayJpeg(const uint8_t arrayname[], uint32_t array_size, int xpos, int ypos)
{

    boolean decoded = JpegDec.decodeArray(arrayname, array_size);

    if (decoded)
    {
        // render the image onto the screen at given coordinates
        renderJPEG(xpos, ypos);
    }
    else
    {
        Log.errorln("Jpeg file format not supported!");
    }
}

#endif

#pragma endregion

#pragma region HTTP Client Functions

#include <HTTPClient.h>

int webGet(String req, String &res)
{
    String oldMethodName = methodName;
    methodName = "webGet(String req, String &res)";

    int result = -1;

    Log.verboseln("Connecting to http://%s:%d%s", HTTP_SERVER, HTTP_PORT,
                  req.c_str());

    WiFiClient client;
    HTTPClient http;
    String payload;
    Log.verboseln("[HTTP] begin...");

    // int connRes = client.connect(IPAddress(192,168,0,12), 5000);
    // Log.verboseln("Connected: %d", connRes);

    // if (http.begin(client, req))
    if (http.begin(client, HTTP_SERVER, HTTP_PORT, req))
    { // HTTP

        Log.verboseln("[HTTP] GET...");
        // start connection and send HTTP header
        int httpCode = http.GET();
        result = httpCode;

        // httpCode will be negative on error
        if (httpCode > 0)
        {
            // HTTP header has been send and Server response header has
            // been handled
            Log.verboseln("[HTTP] GET... code: %d\n", httpCode);

            // file found at server
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
            {
                res = http.getString();
            }
        }
        else
        {
            Log.errorln("[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str());
        }

        http.end();
    }
    else
    {
        Log.warningln("[HTTP] Unable to connect");
    }

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
    return result;
}
#pragma endregion


#pragma region RTSP Server

#ifdef USE_RTSP

#include "OV2640Streamer.h"
#include "CRtspSession.h"
// Use this URL to connect the RTSP stream, replace the IP address with the address of your device
// rtsp://192.168.0.109:8554/mjpeg/1

/** Forward dedclaration of the task handling RTSP */
void rtspTask(void *pvParameters);

/** Task handle of the RTSP task */
TaskHandle_t rtspTaskHandler;

/** WiFi server for RTSP */
WiFiServer rtspServer(8554);

/** Stream for the camera video */
CStreamer *streamer = NULL;
/** Session to handle the RTSP communication */
CRtspSession *session = NULL;
/** Client to handle the RTSP connection */
WiFiClient rtspClient;
/** Flag from main loop to stop the RTSP server */
boolean stopRTSPtask = false;

/**
 * Starts the task that handles RTSP streaming
 */
void initRTSP(void)
{
    String oldMethodName = methodName;
    methodName = "ProcessAppWifiDisconnectTasks()";
    Log.verboseln("Entering...");

    // Create the task for the RTSP server
    xTaskCreate(rtspTask, "RTSP", 4096, NULL, 1, &rtspTaskHandler);

    // Check the results
    if (rtspTaskHandler == NULL)
    {
        Log.errorln("Create RTSP task failed");
    }
    else
    {
        Log.infoln("RTSP task up and running");
    }
    
    Log.verboseln("Exiting...");
    methodName = oldMethodName;

}

/**
 * Called to stop the RTSP task, needed for OTA
 * to avoid OTA timeout error
 */
void stopRTSP(void)
{
    stopRTSPtask = true;
}

/**
 * The task that handles RTSP connections
 * Starts the RTSP server
 * Handles requests in an endless loop
 * until a stop request is received because OTA
 * starts
 */
void rtspTask(void *pvParameters)
{
    String oldMethodName = methodName;
    methodName = "ProcessAppWifiDisconnectTasks()";
    Log.verboseln("Entering...");

    uint32_t msecPerFrame = 50;
    static uint32_t lastimage = millis();

    // rtspServer.setNoDelay(true);
    rtspServer.setTimeout(1);
    rtspServer.begin();

    while (1)
    {
        // If we have an active client connection, just service that until gone
        if (session)
        {
            session->handleRequests(0); // we don't use a timeout here,
            // instead we send only if we have new enough frames

            uint32_t now = millis();
            if (now > lastimage + msecPerFrame || now < lastimage)
            { // handle clock rollover
                session->broadcastCurrentFrame(now);
                lastimage = now;
            }

            // Handle disconnection from RTSP client
            if (session->m_stopped)
            {
                Log.infoln("RTSP client closed connection");
                delete session;
                delete streamer;
                session = NULL;
                streamer = NULL;
            }
        }
        else
        {
            rtspClient = rtspServer.accept();
            // Handle connection request from RTSP client
            if (rtspClient)
            {
                Log.infoln("RTSP client started connection");
                streamer = new OV2640Streamer(&rtspClient, cam); // our streamer for UDP/TCP based RTP transport

                session = new CRtspSession(&rtspClient, streamer); // our threads RTSP session and state
                delay(100);
            }
        }
        if (stopRTSPtask)
        {
            // User requested RTSP server stop
            if (rtspClient)
            {
                Log.infoln("Shut down RTSP server requested.");
                delete session;
                delete streamer;
                session = NULL;
                streamer = NULL;
            }
            // Delete this task
            vTaskDelete(NULL);
        }
        delay(10);
    }
    Log.verboseln("Exiting...");
    methodName = oldMethodName;

}

#endif

#pragma endregion


#endif // FRAMEWORK_H