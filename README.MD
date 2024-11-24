#ESP32Framework
A basic framework for developing ESP32 based applications. The framework includes support for multiple instances, multi-target logging, Over-the-Air (OTA) software updates, MQTT messaging, Wifi, Display and Graphics, Audio, and many more useful tools.

##Product Details
###Requirements
####Instance Management
1. Each instance shall support a way to uniquely identify itself
 a. This may be an auto incrementing method or random number generation
####MQTT messaging
1. Topic schema is {app_name}/Topic1/Topic2/.../Topic9 for outgoing (status/info) messages
  a. ID shall be included in top level of payload with multiple parameters
  b. ID shall be the last topic for single payload parameters unless the ID itself is the single parameter
2. Topic schema is {app_name}/{app_id}/Topic1/Topic2/.../Topic9 for incoming (command) messages
 a. An special number/code will be reserved for the ALL {app_id} to which all instances shall respond
3. Payload shall be text for a single payload parameter or JSON for payloads with multiple parameters
####Firmware Updates
1. Instance will check a known server for updates
2. Firmware image will be secured
    a. May be secured by preshared key (maybe not possible using SPIFFS)
    b. May be secured by encrypted channel