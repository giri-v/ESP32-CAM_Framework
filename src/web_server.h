////////////////////////////////////////////////////////////////////
/// @file web_server.h
/// @brief Contains all functions related to the webserver
////////////////////////////////////////////////////////////////////

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "framework.h"

#include "webpages.h"

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
bool checkUserWebAuth(AsyncWebServerRequest *request);
void notFound(AsyncWebServerRequest *request);
void initWebServer();
String processor(const String &var);
String listFiles(fs::FS &fs, bool ishtml);

// list all of the files, if ishtml=true, return html rather than simple text
String listFiles(fs::FS &fs, bool ishtml)
{
    String returnText = "";
    Log.infoln("Listing files stored on SPIFFS");
    File root = fs.open("/");
    File foundfile = root.openNextFile();
    if (ishtml)
    {
        returnText += "<table><tr><th align='left'>Name</th><th align='left'>Size</th><th></th><th></th></tr>";
    }
    while (foundfile)
    {
        if (ishtml)
        {
            if (foundfile.isDirectory())
            {
                returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td></td>";
                returnText += "<td></td>";
                returnText += "<td></td></tr>";
            }
            else
            {
            returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td>" + humanReadableSize(foundfile.size()) + "</td>";
            returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'download\')\">Download</button>";
            returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'delete\')\">Delete</button></tr>";
            }
        }
        else
        {
            returnText += "File: " + String(foundfile.name()) + " Size: " + humanReadableSize(foundfile.size()) + "\n";
        }
        foundfile = root.openNextFile();
    }
    if (ishtml)
    {
        returnText += "</table>";
    }
    root.close();
    foundfile.close();
    return returnText;
}



// parses and processes webpages
// if the webpage has %SOMETHING% or %SOMETHINGELSE% it will replace those strings with the ones defined
String processor(const String &var)
{
    if (var == "FIRMWARE")
    {
        return FIRMWARE_VERSION;
    }

    if (var == "FREESPIFFS")
    {
        return humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes()));
    }

    if (var == "USEDSPIFFS")
    {
        return humanReadableSize(SPIFFS.usedBytes());
    }

    if (var == "TOTALSPIFFS")
    {
        return humanReadableSize(SPIFFS.totalBytes());
    }

    if (var == "APP_NAME")
    {
        return String(appName);
    }

    return "Undefined!";
}


void initWebServer()
{
    // configure web webServer
    Log.infoln("Initializing Web Server on Port 80");

    // if url isn't found
    webServer.onNotFound(notFound);

    // run handleUpload function when any file is uploaded
    webServer.onFileUpload(handleUpload);

    // Handle static files
    webServer.serveStatic("", SPIFFS, "/www/");

    // visiting this page will cause you to be logged out
    webServer.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request)
               {
    request->requestAuthentication();
    request->send(401); });

    // presents a "you are now logged out webpage
    webServer.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request)
               {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Log.infoln(logmessage.c_str());
    request->send_P(401, "text/html", logout_html, processor); });

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
               {
                   String logmessage = "Client:" + request->client()->remoteIP().toString() + +" " + request->url();

                   if (checkUserWebAuth(request))
                   {
                       logmessage += " Auth: Success";
                       Log.infoln(logmessage.c_str());
                       request->send_P(200, "text/html", index_html, processor);
                   }
                   else
                   {
                       logmessage += " Auth: Failed";
                       Log.infoln(logmessage.c_str());
                       return request->requestAuthentication();
                   }
               });

    webServer.on("/simple.css", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
                   String logmessage = "Client:" + request->client()->remoteIP().toString() + +" " + request->url();

                   if (checkUserWebAuth(request))
                   {
                       logmessage += " Auth: Success";
                       Log.infoln(logmessage.c_str());
                       request->send_P(200, "text/css", simple_css, processor);
                   }
                   else
                   {
                       logmessage += " Auth: Failed";
                       Log.infoln(logmessage.c_str());
                       return request->requestAuthentication();
                   } });

    webServer.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (checkUserWebAuth(request)) {
      request->send(200, "text/html", reboot_html);
      logmessage += " Auth: Success";
      Log.infoln(logmessage.c_str());
      shouldReboot = true;
    } else {
      logmessage += " Auth: Failed";
      Log.infoln(logmessage.c_str());
      return request->requestAuthentication();
    } });

    webServer.on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request)
               {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Log.infoln(logmessage.c_str());
      request->send(200, "text/plain", listFiles(SPIFFS, true));
    } else {
      logmessage += " Auth: Failed";
      Log.infoln(logmessage.c_str());
      return request->requestAuthentication();
    } });

#ifdef USE_SD_CARD
    webServer.on("/listSDfiles", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Log.infoln(logmessage.c_str());
      request->send(200, "text/plain", listFiles(SD, true));
    } else {
      logmessage += " Auth: Failed";
      Log.infoln(logmessage.c_str());
      return request->requestAuthentication();
    } });
#endif

    webServer.on("/file", HTTP_GET, [](AsyncWebServerRequest *request)
               {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Log.infoln(logmessage.c_str());

      if (request->hasParam("name") && request->hasParam("action")) {
        const char *fileName = request->getParam("name")->value().c_str();
        const char *fileAction = request->getParam("action")->value().c_str();

        logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + "?name=" + String(fileName) + "&action=" + String(fileAction);

        if (!SPIFFS.exists(fileName)) {
            logmessage += " ERROR: file does not exist";
            Log.infoln(logmessage.c_str());
            request->send(400, "text/plain", "ERROR: file does not exist");
        } else {
            logmessage += " file exists";
            Log.infoln(logmessage.c_str());
            if (strcmp(fileAction, "download") == 0)
            {
                logmessage += " downloaded";
                request->send(SPIFFS, fileName, "application/octet-stream");
          } else if (strcmp(fileAction, "delete") == 0) {
            logmessage += " deleted";
            SPIFFS.remove(fileName);
            request->send(200, "text/plain", "Deleted File: " + String(fileName));
          } else {
            logmessage += " ERROR: invalid action param supplied";
            request->send(400, "text/plain", "ERROR: invalid action param supplied");
          }
          Log.infoln(logmessage.c_str());
        }
      } else {
        request->send(400, "text/plain", "ERROR: name and action params required");
      }
    } else {
      logmessage += " Auth: Failed";
      Log.infoln(logmessage.c_str());
      return request->requestAuthentication();
    } });

    webServer.begin();
}

void notFound(AsyncWebServerRequest *request)
{
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Log.infoln(logmessage.c_str());
    request->send(404, "text/plain", "Not found");
}

// used by webServer.on functions to discern whether a user has the correct httpapitoken OR is authenticated by username and password
bool checkUserWebAuth(AsyncWebServerRequest *request)
{
    bool isAuthenticated = false;

    if (request->authenticate(appName, appSecret))
    {
        Log.infoln("is authenticated via username and password");
        isAuthenticated = true;
    }
    return isAuthenticated;
}

// handles uploads to the filserver
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    // make sure authenticated before allowing upload
    if (checkUserWebAuth(request))
    {
        String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
        Log.infoln(logmessage.c_str());

        if (!index)
        {
            logmessage = "Upload Start: " + String(filename);
            // open the file on first call and store the file handle in the request object
            request->_tempFile = SPIFFS.open("/" + filename, "w");
            Log.infoln(logmessage.c_str());
        }

        if (len)
        {
            // stream the incoming chunk to the opened file
            request->_tempFile.write(data, len);
            logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
            Log.infoln(logmessage.c_str());
        }

        if (final)
        {
            logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
            // close the file handle as the upload is now done
            request->_tempFile.close();
            Log.infoln(logmessage.c_str());
            request->redirect("/");
        }
    }
    else
    {
        Log.infoln("Auth: Failed");
        return request->requestAuthentication();
    }
}

#endif // WEB_SERVER_H