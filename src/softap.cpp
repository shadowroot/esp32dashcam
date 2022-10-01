#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "config.h"

WebServer serverOTA(VIDEO_PORT);
WebServer server(VIDEO_PORT);

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto hiRes = esp32cam::Resolution::find(800, 600);

void
handleMjpeg()
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-HI-RES FAIL");
  }

  Serial.println("STREAM BEGIN");
  WiFiClient client = server.client();
  auto startTime = millis();
  int res = esp32cam::Camera.streamMjpeg(client);
  if (res <= 0) {
    Serial.printf("STREAM ERROR %d\n", res);
    return;
  }
  auto duration = millis() - startTime;
  Serial.printf("STREAM END %dfrm %0.2ffps\n", res, 1000.0 * res / duration);
}

void OnWiFiEvent(WiFiEvent_t event)
{
  switch (event) {
 
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("ESP32 Connected to WiFi Network");
      break;
    case SYSTEM_EVENT_AP_START:
      Serial.println("ESP32 soft AP started");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      Serial.println("Station connected to ESP32 soft AP");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      Serial.println("Station disconnected from ESP32 soft AP");
      break;
    default: break;
  }
}

void setup()
{
  Serial.begin(115200);
  WiFi.onEvent(OnWiFiEvent);
  Serial.println();

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(SOFTAP_SSID, SOFTAP_PASSWORD);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp32.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network
  if (!MDNS.begin(MDNS_HOSTNAME)) {
      Serial.println("Error setting up MDNS responder!");
      while(1) {
          delay(1000);
      }
  }
  MDNS.addService("http", "tcp", VIDEO_PORT);

  Serial.println("mDNS responder started");

  Serial.print("http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleMjpeg);

  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });

  ArduinoOTA.setHostname(MDNS_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.begin();
  server.begin();
}


void loop()
{
  server.handleClient();
  ArduinoOTA.handle();
}