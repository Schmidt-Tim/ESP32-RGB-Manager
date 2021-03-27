#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <NeoPixelBus.h>


//--------------------------------------------------
// Variables - Wifi
//--------------------------------------------------

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

IPAddress static_ip(192, 168, 2, 99);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress dns(192, 168, 2, 30);


//--------------------------------------------------
// Variables - Webserver
//--------------------------------------------------

const char* URL_HOME = "/home";
const char* URL_VERIFY = "/verify";
const char* URL_UPDATE = "/update";

const String PARAMETER_PASSWORD = "password";

AsyncWebServer server_http(80);


//--------------------------------------------------
// Variables - Updating
//--------------------------------------------------

const String FIRMWARE_VERSION = "0.2.3";
const String UPDATE_PASSWORD = "CHANGE_THIS_PASSWORD";

bool HasUpdatePermission = false;
bool RebootESP32 = false;





//--------------------------------------------------
// SETUP
//--------------------------------------------------

void setup() {
  Serial.begin(9600); //Serial output for Debugging
  
  //Connect Wifi
  WiFi.config(static_ip, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  } else {
    Serial.printf("WiFi Success!\n"); 
  }   

  //Setup WebServer
  server_http.on(URL_HOME, HTTP_GET, WebHomeGet);
  
  server_http.on(URL_VERIFY, HTTP_GET, WebVerifyGet);
  server_http.on(URL_VERIFY, HTTP_POST, WebVerifyPost);
  
  server_http.on(URL_UPDATE, HTTP_GET, WebUpdateGet);
  server_http.on(URL_UPDATE, HTTP_POST, WebUpdatePost, OnFileUpload);  

  server_http.onNotFound(OnNotFound);

  server_http.begin();
}


//--------------------------------------------------
// LOOP
//--------------------------------------------------

void loop() {
  if (RebootESP32) {
    Serial.println(F("Esp reboot ..."));
    delay(100);
    ESP.restart();
  }
}





//--------------------------------------------------
// Builder
//--------------------------------------------------

String BuildHtmlVerify(String alert = "") {
  return "<!DOCTYPE html><html><head><title>ESP32-RGB-Manager - Verify</title><script>let msg = '" + alert + "'; if(msg != '') alert(msg);</script></head><body><h1>For updating the firmware, verification is required!</h1><form method='POST' action='" + URL_VERIFY + "'><label>Password:</label><input type='password' name='" + PARAMETER_PASSWORD + "' required><br><input type='submit' value='Verify...'><br></form></body></html>";
}

String BuildHtmlUpdate(String alert = "") {
  return "<!DOCTYPE html><html><head><title>ESP-32 RGB-Manager - Update</title><script>let msg = '" + alert + "'; if(msg != '') alert(msg);</script></head><body><h1>Installed firmware version: " + FIRMWARE_VERSION + "</h1><h2>Select the firmware (.bin) you want to install:</h2><form method='POST' action='" + URL_UPDATE + "' enctype='multipart/form-data'><input type='file' required><br><input type='submit' value='Run Update'></form></body></html>";
}

String BuildHtmlHome(String alert = "") {
  return "<!DOCTYPE html><html><head><title>ESP-32 RGB-Manager - Verify</title><script>let msg = '" + alert + "'; if(msg != '') alert(msg);</script></head><body><h1>HOME</h1></body></html>";
}


//--------------------------------------------------
// Home
//--------------------------------------------------

void OnNotFound(AsyncWebServerRequest *request) {
  request->redirect(URL_HOME); //Redirect URL_UPDATE
}

void WebHomeGet(AsyncWebServerRequest *request) {
  request->send(200, "text/html", BuildHtmlHome()); //Show home.html
}


//--------------------------------------------------
// Verify
//--------------------------------------------------

void WebVerifyGet(AsyncWebServerRequest *request) {
  if(HasUpdatePermission) {
    request->redirect(URL_UPDATE); //Redirect URL_UPDATE
  } else {
    request->send(200, "text/html", BuildHtmlVerify()); //Show verify.html
  }
}

void WebVerifyPost(AsyncWebServerRequest *request) {
  //Check Password -> Set Flag
  if(request->hasParam(PARAMETER_PASSWORD, true)) {
    String hPassword = request->getParam(PARAMETER_PASSWORD, true)->value();
    if(hPassword == UPDATE_PASSWORD) {
      HasUpdatePermission = true;
    }
  }

  //Check Flag
  if(HasUpdatePermission) {
    request->redirect(URL_UPDATE); //Redirect URL_UPDATE
  } else {
    request->send(200, "text/html", BuildHtmlVerify("Password incorrect!")); //Reload verify.html and inject error message
  }
}


//--------------------------------------------------
// Update
//--------------------------------------------------

void WebUpdateGet(AsyncWebServerRequest *request) {
  if(!HasUpdatePermission) {
    request->redirect(URL_VERIFY); //Redirect URL_VERIFY
  } else {
    request->send(200, "text/html", BuildHtmlUpdate()); //Show update.html
  }
}

void WebUpdatePost(AsyncWebServerRequest *request) {
  bool hSuccess = !Update.hasError();

  //First Respond, then set Restart-Flag
  if(hSuccess) {
    request->redirect(URL_HOME); //Redirect URL_HOME
    RebootESP32 = true; //Flag for reboot -> Loop()
  } else {
    //Reload with injected error message
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", BuildHtmlUpdate("Update FAILED!"));
    response->addHeader("Connection", "close");
    request->send(response);
  }
}

void OnFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if(HasUpdatePermission) {
    if (!index) {
      Serial.printf("Update Start: %s\n", filename.c_str());
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index + len);
      }
      else {
        Update.printError(Serial);
      }
    }
  }
}
