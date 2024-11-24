#include <Arduino.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}

#define APP_NAME "esp32FWApp"

#include <secrets.h>
// #define TELNET_LOGGING

#include <WiFi.h>
#include <Preferences.h>
#include <SPI.h>

#include <FS.h>
#ifndef LittleFS
#include <SPIFFS.h>
#else
#include <LittleFS.h>
#endif

#include <Update.h>

#include <esp_camera.h>

#define PIN_FLASHLIGHT 4

// CAMERA_MODEL_AI_THINKER
#define CAMERA_MODEL_AI_THINKER // Has PSRAM


#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#include <HTTPClient.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

#include <ArduinoLog.h>
#define LOG_LEVEL LOG_LEVEL_INFO
#include <TLogPlus.h>

#ifndef SECRETS_H
#define SECRETS_H

#define HOSTNAME "myESP8266";
#define NTP_SERVER "pool.ntp.org"

#define WIFI_SSID "APName"
#define WIFI_PASSWORD "APPassword"

#define MQTT_HOST IPAddress(192, 168, 0, 200)
#define MQTT_PORT 1883

#define LATITUDE 37.3380937
#define LONGITUDE -121.8853892

#endif // SECRETS_H

// using namespace TLogPlus;

#ifdef TELNET_LOGGING
#include <TelnetSerialStream.h>
using namespace TLogPlusStream;
TelnetSerialStream telnetSerialStream = TelnetSerialStream();
#endif

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
using namespace TLogPlusStream;
// EthernetClient client;
WiFiClient client;
MqttStream mqttStream = MqttStream(&client);
char topic[128] = "log/foo";
#endif

//int chip_id = ESP.getEfuseMac();

// **************** Debug Parameters ************************
String methodName = "";

// ********* App Parameters *****************
const char *appName = APP_NAME;
int appID = -1;

int volume = 50; // Volume is %
int bootCount = 0;
esp_sleep_wakeup_cause_t wakeup_reason;
esp_reset_reason_t reset_reason;
esp_chip_info_t chip_info;
esp_chip_model_t chip_model;

int maxOtherIndex = -1;

Preferences preferences;

// ********** Time/NTP Parameters **********
const char *ntpServer = NTP_SERVER;
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

const char *localTZ = "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00";

// ********** Connectivity Parameters **********
String hostname = HOSTNAME;

char ipAddress[100];
IPAddress deviceIP;
uint8_t macAddress[] = {0x32, 0xAE, 0xA4, 0x07, 0x0D, 0x66}; // TODO: make this a meaningful default

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
TimerHandle_t checkFWUpdateTimer;
TimerHandle_t appIDWaitTimer;

const int timerInterval = 30000;  // time between each HTTP POST image
unsigned long previousMillis = 0; // last time image was sent

// MQTT Topics (25 character limit per level)
char onlineTopic[100];
char willTopic[100];
char idTopic[100];
char snapshotTopic[100];

char appSubTopic[100];

// Firmware Update Parameters
const char *remoteUpdateUrl = "http://192.168.0.12/internal/iot";
char fwUpdateUrl[200];
char latestFWImageIndexUrl[200];
char latestFirmwareFileName[100];

const char *tempImageName = "/images/temp%i.jpg";
int tempImageCounter = 0;

bool bcameraReady = false;
bool bcaptureFailed = false;

bool bFlash = false;

// put function declarations here:
void loadPrefs();
void storePrefs();
void mqttPublishID();
int myFunction(int, int);

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

void loadPrefs()
{
  String oldMethodName = methodName;
  methodName = "loadPrefs()";
  Log.verboseln("Entering...");

  bool doesExist = preferences.isKey("appID");
  if (doesExist)
  {
    Log.infoln("Loading settings.");
    appID = preferences.getInt("appID");
    volume = preferences.getInt("Volume");
    bootCount = preferences.getInt("BootCount");
    // enableSnapshot = preferences.getBool("EnableSnapshot");
  }
  else
  {
    Log.warningln("Could not find Preferences!");
    Log.noticeln("appID not set yet!");
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

  preferences.putInt("appID", appID);
  preferences.putInt("Volume", volume);
  preferences.putInt("BootCount", bootCount);
  // preferences.putBool("EnableSnapshot", enableSnapshot);

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void logTimestamp()
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

  Log.infoln("Time: %s", c);
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

void initFS()
{
#ifndef LittleFS
  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Log.errorln("An Error has occurred while mounting SPIFFS");
    return;
  }
#else
  if (!LittleFS.begin())
  {
    Log.errorln("Flash FS initialisation failed!");
    while (1)
      yield(); // Stay here twiddling thumbs waiting
  }
  Log.infoln("Flash FS available!");

#endif
}

void initAppStrings()
{
  sprintf(onlineTopic, "%s/online", appName);
  sprintf(willTopic, "%s/offline", appName);
  sprintf(idTopic, "%s/id", appName);

  sprintf(appSubTopic, "%s/#", appName);
  sprintf(fwUpdateUrl, "%s/%s/firmware", remoteUpdateUrl, appName);

  sprintf(latestFWImageIndexUrl, "%s/%s/firmware/id", remoteUpdateUrl, appName);
}

void initAppInstanceStrings()
{
  sprintf(onlineTopic, "%s/status", appName);
  sprintf(willTopic, "%s/offline", appName);
  sprintf(idTopic, "%s/id", appName);

  sprintf(appSubTopic, "%s/#", appName);
}

// Check if photo capture was successful
int getImageSize(char *fn)
{
  File f_pic = SPIFFS.open(fn);
  return f_pic.size();
}

// Capture Photo and Save it to SPIFFS
int captureImagetoFile()
{
  String oldMethodName = methodName;
  methodName = "captureImagetoFile()";
  Log.verboseln("Entering...");

  camera_fb_t *fb = NULL; // pointer
  bool ok = 0;            // Boolean indicating if the picture has been taken correctly

  if (!bcameraReady)
  {
    Log.warningln("Camera Not Ready. Can't capture image!!!");

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
    return -1;
  }

  // Take a photo with the camera
  Log.infoln("Capturing image to file");

  //if (bFlash)
    digitalWrite(PIN_FLASHLIGHT, HIGH);



  fb = esp_camera_fb_get();
  if (!fb)
  {
    Log.errorln("Camera capture failed");
    bcaptureFailed = true;

    Log.verboseln("Exiting...");
    methodName = oldMethodName;
    return -1;
  }

  // Photo file name
  char fn[40] = "/images/temp.jpg";
  int fs = 0;
  while (SPIFFS.exists(fn))
  {
    sprintf(fn, tempImageName, tempImageCounter++);
  }
  File file = SPIFFS.open(fn, FILE_WRITE);
  Log.infoln("Picture file name: %s", fn);

  // Insert the data in the photo file
  if (!file)
  {
    Log.errorln("Failed to open file in writing mode!!!!");
  }
  else
  {
    file.write(fb->buf, fb->len); // payload (image), payload length
    fs = file.size();
    Log.infoln("%s %i bytes ", fn, fs );
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
  digitalWrite(PIN_FLASHLIGHT, LOW);

  // check if file has been correctly saved in SPIFFS
  int checkSize = getImageSize(fn);
  if (checkSize != fs)
  {
    //Log.errorln("File not saved correctly. Erasing file!!!");
    //fs = 0;
    //SPIFFS.remove(fn);
    //tempImageCounter--;
  }

  Log.infoln("File saved successfully. %i", checkSize);
  Log.verboseln("Exiting...");
  methodName = oldMethodName;

  return fs;
}

void sendImage(char *filename)
{
  String oldMethodName = methodName;
  methodName = "sendImage(char *filename)";
  Log.verboseln("Entering...");

  File file = SPIFFS.open(filename, "r");

  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  size_t fileSize = file.size();
  char *buffer = new char[fileSize + 1]; // +1 for null terminator

  if (file.readBytes(buffer, fileSize) != fileSize)
  {
    Serial.println("Error reading file");
    delete[] buffer;
    return;
  }

  buffer[fileSize] = '\0'; // Add null terminator

  mqttClient.publish("mbmon/snapshot", 1, false, buffer);
 

  delete[] buffer; // Free memory when done
  file.close();

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void sendAllImages()
{
  String oldMethodName = methodName;
  methodName = "sendAllImages()";
  Log.verboseln("Entering...");

  // Photo file name
  char fn[40] = "/images/temp.jpg";
  int fs = 0;

int imgCount = 0;

  while (SPIFFS.exists(fn))
  {
    sendImage(fn);
    imgCount++;
    SPIFFS.remove(fn);
    sprintf(fn, tempImageName, tempImageCounter++);
  }
  Log.infoln("Sending %i images.", imgCount);
  
  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void captureAndSendImage()
{
  String oldMethodName = methodName;
  methodName = "captureAndSendImage()";
  Log.verboseln("Entering...");

  captureImagetoFile();

  if (mqttClient.connected())
  {
    sendAllImages();
  }
  else
  {
  }

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void connectToWifi()
{
  String oldMethodName = methodName;
  methodName = "connectToWifi()";

  Log.infoln("Connecting...");
  WiFi.hostname(hostname);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  methodName = oldMethodName;
}

void connectToMqtt()
{
  String oldMethodName = methodName;
  methodName = "connectToMqtt()";

  Log.infoln("Connecting to MQTT Broker...");
  mqttClient.connect();

  methodName = oldMethodName;
}

void onWifiConnect(const WiFiEvent_t &event)
{
  String oldMethodName = methodName;
  methodName = "onWifiConnect(const WiFiEventStationModeGotIP &event)";
  Log.verboseln("Entering...");

  (void)event;

  deviceIP = WiFi.localIP();
  Log.infoln("Connected to Wi-Fi. IP address: %p", deviceIP);

  Log.infoln("Connecting to NTP Server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // Log.infoln("Connected to NTP Server!");

  connectToMqtt();

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void onWifiDisconnect(const WiFiEvent_t &event)
{
  String oldMethodName = methodName;
  methodName = "onWifiDisconnect(const WiFiEventStationModeDisconnected &event)";
  Log.verboseln("Entering...");

  (void)event;

  Log.infoln("Disconnected from Wi-Fi.");
  Log.infoln("Disconnecting mqttReconnectTimer");
  xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  // mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  Log.infoln("Reconnecting to WiFi...");
  xTimerStart(wifiReconnectTimer, 0);
  // wifiReconnectTimer.once(2, connectToWifi);

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void WiFiEvent(WiFiEvent_t event)
{
  String oldMethodName = methodName;
  methodName = "WiFiEvent(WiFiEvent_t event)";
  Log.verboseln("Entering...");

  switch (event)
  {
  case ARDUINO_EVENT_WIFI_READY:
    Log.infoln("Wifi ready.");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    Log.infoln("Wifi station start.");
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    Log.infoln("Wifi station connected.");
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    onWifiConnect(event);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    onWifiDisconnect(event);
    break;
  }

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void mqttPublishID()
{
  String oldMethodName = methodName;
  methodName = "mqttPublishID()";
  Log.verboseln("Entering...");

  char onlineTopic[20];
  char payloadJson[20];
  sprintf(onlineTopic, "%s/online", appName);
  sprintf(payloadJson, "%i", appID);
  Log.infoln("Published %s topic", onlineTopic);
  int pubRes = mqttClient.publish(onlineTopic, 1, false, payloadJson);

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void mqttPublishWill()
{
  String oldMethodName = methodName;
  methodName = "mqttPublishID()";
  Log.verboseln("Entering...");

  char offlineTopic[20];
  char payloadJson[20];
  sprintf(offlineTopic, "%s/offline", appName);
  sprintf(payloadJson, "%i", appID);
  Log.infoln("Published Last Will and Testament %s", offlineTopic);
  mqttClient.publish(offlineTopic, 1, true, payloadJson);

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void onMqttConnect(bool sessionPresent)
{
  String oldMethodName = methodName;
  methodName = "onMqttConnect(bool sessionPresent)";
  Log.verboseln("Entering...");

  Log.infoln("Connected to MQTT broker: %p , port: %d", MQTT_HOST, MQTT_PORT);
  Log.infoln("Session present: %T", sessionPresent);

  sendAllImages();
  
  captureAndSendImage();

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  String oldMethodName = methodName;
  methodName = "onMqttConnect(bool sessionPresent)";
  Log.verboseln("Entering...");

  (void)reason;

  Log.warningln("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void logMQTTMessage(char *topic, int len, char *payload)
{
  Log.infoln("[%s] {%s} ", topic, payload);
}

void onMqttIDMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  String oldMethodName = methodName;
  methodName = "onMqttIDMessage()";
  Log.verboseln("Entering...");

  char *topics[10];
  int topicCounter = 0;
  char *token = strtok(topic, "/");

  while (token != NULL)
  {
    strcpy(topics[topicCounter++], token);
    token = strtok(NULL, "/");
  }

  if (strcmp(topic, appName) == 0) // Handle all wichime messages
  {
    if ((strstr(topic, "offline") != NULL) || (strstr(topic, "online") != NULL))
    {
      int otherIndex = atoi(payload);
      if (otherIndex >= 0)
        maxOtherIndex = max(maxOtherIndex, otherIndex);
    }
  }

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void doUpdateFirmware(char *fileName)
{
  String oldMethodName = methodName;
  methodName = "onMqttMessage()";
  Log.verboseln("Entering...");

  File file = SPIFFS.open(fileName);

  if (!file)
  {
    Log.errorln("Failed to open file for reading");
    return;
  }

  Log.infoln("Starting update..");

  size_t fileSize = file.size();

  if (!Update.begin(fileSize))
  {

    Log.warningln("Cannot do the update!");
    // TODO: publish a message that the update failed

    return;
  };

  Update.writeStream(file);

  if (Update.end())
  {
    Log.infoln("Successful update");
  }
  else
  {

    Log.errorln("Error Occurred: %s", String(Update.getError()));
    // TODO: publish a message that the update failed
    return;
  }

  file.close();

  Log.infoln("Reset in 2 seconds...");
  delay(2000);

  ESP.restart();
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

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  String oldMethodName = methodName;
  methodName = "onMqttMessage()";
  Log.verboseln("Entering...");

  logMQTTMessage(topic, len, payload);

  return;

  char *topics[10];
  int topicCounter = 0;
  char *token = strtok(topic, "/");

  while ((token != NULL) && (topicCounter < 11))
  {
    strcpy(topics[topicCounter++], token);
    token = strtok(NULL, "/");
  }

  if (topicCounter > 10)
  {
    Log.noticeln("MQTT Topic has > 10 levels.");
    return;
  }

  if ((strcmp(topic, appName) == 0) && (topicCounter > 1))
  {
    // Handle all messages for our App (AppName is in topic #1)
    if (isNumeric(topics[1]))
    {
      // This is a command because there is an AppID in topic #2
      int cmdTargetID = atoi(topics[1]);
      if (cmdTargetID == appID)
      { // This command is for us (AppID == our appID)
      }
      else
      { // This command is for another instance
      }
    }
    else
    { // This is not a command
    }
  }
  else if (strcmp(topic, "otherThingTopic") == 0)
  {
    // This is another App's message we are interested in
    Log.infoln("Got CallerID topic");

    char msg[len + 1];
    memcpy(msg, payload, len);
    msg[len] = 0;
    logMQTTMessage(topic, len, msg);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, msg);

    // Test if parsing succeeds.
    if (error)
    {
      Log.errorln("deserializeJson() failed: %s", error.c_str());
    }
    else
    {
    }
  }
  else // This is an unhandled topic
  {
  }

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void logMACAddress(uint8_t baseMac[6])
{
  char mac[200];
  sprintf(mac, "MAC Address: { %02x:%02x:%02x:%02x:%02x:%02x }",
          baseMac[0], baseMac[1], baseMac[2],
          baseMac[3], baseMac[4], baseMac[5]);
  Log.infoln(mac);
}

void initializeCamera()
{
  String oldMethodName = methodName;
  methodName = "initializeCamera()";
  Log.verboseln("Entering...");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // init with high specs to pre-allocate larger buffers
  if (psramFound())
  {
    Log.infoln("Found PSRAM. Using FRAMESIZE_SVGA.");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10; // 0-63 lower number means higher quality
    config.fb_count = 2;
  }
  else
  {
    Log.infoln("Could not find PSRAM. Using FRAMESIZE_CIF.");
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12; // 0-63 lower number means higher quality
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Log.errorln("Camera init failed with error 0x%x", err);
  }
  else
  {
    bcameraReady = true;
    Log.infoln("Camera initialized successfully.");
  }

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void print_wakeup_reason()
{

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Log.infoln("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Log.infoln("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Log.infoln("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Log.infoln("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Log.infoln("Wakeup caused by ULP program");
    break;
  default:
    Log.infoln("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

void setAppID()
{
  String oldMethodName = methodName;
  methodName = "setAppID()";
  Log.verboseln("Entering...");

  appID = maxOtherIndex + 1;
  storePrefs();

  Log.infoln("Got appID, restarting...");
  esp_restart();

  /*
    mqttPublishID();
    Log.infoln("About to change onMessage handler.");
    mqttClient.onMessage(NULL);
    Log.infoln("Changed onMessage handler to NULL.");
    mqttClient.onMessage(onMqttMessage);
    Log.infoln("Changed onMessage handler.");
    xTimerDelete(appIDWaitTimer, 0);
  */
  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

int getlatestFirmware(char *fileName)
{
  String oldMethodName = methodName;
  methodName = "int getlatestFirmware(char *fileName)";

  int result = -1;
  int httpCode = -1;

  WiFiClient client;
  HTTPClient http;
  String payload;

  File f = SPIFFS.open(fileName, FILE_WRITE);
  if (f)
  {
    http.begin(fwUpdateUrl);
    httpCode = http.GET();
    if (httpCode > 0)
    {
      if (httpCode == HTTP_CODE_OK)
      {
        WiFiClient *stream = http.getStreamPtr();
        while (stream->available())
        {
          char c = stream->read();
          f.print(c);
        }
      }
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    f.close();
    http.end();
  }

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
  return httpCode;
}

int webGet(String req, String &res)
{
  String oldMethodName = methodName;
  methodName = "webGet(String req, String &res)";

  int result = -1;

  WiFiClient client;
  HTTPClient http;
  String payload;
  Log.verboseln("[HTTP] begin...");

  if (http.begin(client, req))
  { // HTTP

    Log.verboseln("[HTTP] GET...");
    // start connection and send HTTP header
    int httpCode = http.GET();
    result = httpCode;

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
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

void checkFWUpdate()
{
  String oldMethodName = methodName;
  methodName = "checkFWUpdate()";

  String imageID;
  int code = webGet(latestFWImageIndexUrl, imageID);
  sprintf(latestFirmwareFileName, "/firmware/%s_%s.bin", appName, imageID);

  code = getlatestFirmware(latestFirmwareFileName);

  doUpdateFirmware(latestFirmwareFileName);

  Log.infoln("Completed checking for FW updates.");
  xTimerDelete(checkFWUpdateTimer, 0);

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}


IRAM_ATTR void interruptService()
{
}

void setup()
{
  String oldMethodName = methodName;
  methodName = "setup()";

  bcameraReady = false;

  Serial.begin(115200);
  Serial.println("Starting....");

#ifdef TELNET
  Log.addPrintStream(std::make_shared<TelnetSerialStream>(telnetSerialStream));
#endif

#ifdef WEBSTREAM
  Log.addPrintStream(std::make_shared<WebSerialStream>(webSerialStream));
#endif

#ifdef SYSLOG_LOGGING
  syslogStream.setDestination(SYSLOG_HOST);
  syslogStream.setRaw(false); // wether or not the syslog server is a modern(ish) unix.
#ifdef SYSLOG_PORT
  syslogStream.setPort(SYSLOG_PORT);
#endif
  Log.addPrintStream(std::make_shared<SyslogStream>(syslogStream));
#endif

#ifdef MQTT_HOST
  // mqttStream.setServer(MQTT_HOST);
  // mqttStream.setTopic(topic);
  // Log.addPrintStream(std::make_shared<MqttStream>(mqttStream));
#endif

  TLogPlus::Log.begin();

  Log.begin(LOG_LEVEL, &TLogPlus::Log, false);
  Log.setPrefix(printTimestamp);

  // esp_chip_info()
  esp_base_mac_addr_get(macAddress);
  logMACAddress(macAddress);

  Log.noticeln("Starting %s...", appName);
  Log.verboseln("Entering ...");

  preferences.begin(appName, false);
  loadPrefs();
  bootCount++;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  reset_reason = esp_reset_reason();
  initFS();

  // Add some custom code here
  initAppStrings();

  // Configure Hardware
  Log.infoln("Configuring hardware.");
  pinMode(PIN_FLASHLIGHT, OUTPUT);
  // pinMode(DOORBELL_PIN, INPUT);
  // attachInterrupt(digitalPinToInterrupt(DOORBELL_PIN), doorbellPressed, FALLING);

  Log.verboseln("MOSI: %i", MOSI);
  Log.verboseln("MISO: %i", MISO);
  Log.verboseln("SCK: %i", SCK);
  Log.verboseln("SS: %i", SS);

  // This is connectivity setup code
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
#ifdef MQTT_USER
  mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
#endif

  if (appID >= 0)
  {
    mqttClient.onMessage(onMqttMessage);
    checkFWUpdateTimer = xTimerCreate("checkFWUpdateTimer", pdMS_TO_TICKS(5000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(checkFWUpdate));
  }
  else
  {
    mqttClient.onMessage(onMqttIDMessage);
    appIDWaitTimer = xTimerCreate("appIDWaitTimer", pdMS_TO_TICKS(10000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(setAppID));
    xTimerStart(appIDWaitTimer, 0);
  }

  connectToWifi();

  initializeCamera();

  captureAndSendImage();

  Log.verboseln("Exiting...");
  methodName = oldMethodName;
}

void loop()
{
  TLogPlus::Log.loop();

  // put your main code here, to run repeatedly:
  if ((millis() % 10000) == 0)
  {
    logTimestamp();
  }

}

// put function definitions here:
int myFunction(int x, int y)
{
  return x + y;
}