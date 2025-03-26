// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */

#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include <MycilaESPConnect.h>
#include <StreamString.h>

#include <esp_ota_ops.h>
#include <esp_partition.h>

#ifndef MYCILA_SAFEBOOT_NO_MDNS
  #include <ESPmDNS.h>
#endif

#ifdef SAFEBOOT_LOGGING
  #define LOG(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
  #define LOG(format, ...)
#endif

extern const char* __COMPILED_APP_VERSION__;
extern const uint8_t update_html_start[] asm("_binary__pio_embed_website_html_gz_start");
extern const uint8_t update_html_end[] asm("_binary__pio_embed_website_html_gz_end");
static const char* successResponse = "Update Success! Rebooting...";
static const char* cancelResponse = "Rebooting...";

static AsyncWebServer webServer(80);
static Mycila::ESPConnect espConnect;
static Mycila::ESPConnect::Config espConnectConfig;
static StreamString updaterError;

static String getChipIDStr() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String espId = String(chipId, HEX);
  espId.toUpperCase();
  return espId;
}

static void start_web_server() {
  webServer.on("/cancel", HTTP_POST, [](AsyncWebServerRequest* request) {
    request->onDisconnect([]() {
      delay(1000);
      ESP.restart();
    });
    request->send(200, "text/plain", cancelResponse);
  });

  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", update_html_start, update_html_end - update_html_start);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webServer.on(
    "/",
    HTTP_POST,
    [](AsyncWebServerRequest* request) {
      if (Update.hasError() || updaterError.length()) {
        request->send(500, "text/plain", "Update error: " + updaterError);
      } else {
        request->onDisconnect([]() {
          delay(1000);
          ESP.restart();
        });
        request->send(200, "text/plain", successResponse);
      } },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (!index) {
        updaterError.clear();
        int otaMode = request->hasParam("mode") && request->getParam("mode")->value() == "1" ? U_SPIFFS : U_FLASH;
        LOG("Mode: %d\n", otaMode);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, otaMode))
          Update.printError(updaterError);
      }
      if (len && !updaterError.length() && Update.write(data, len) != len)
        Update.printError(updaterError);
      if (final && !updaterError.length() && !Update.end(true))
        Update.printError(updaterError);
    });

  webServer.on("/chipspecs", HTTP_GET, [](AsyncWebServerRequest* request) {
    String chipSpecs = ESP.getChipModel();
    chipSpecs += " (" + String(ESP.getFlashChipSize() >> 20) + " MB)";
    request->send(200, "text/plain", chipSpecs.c_str());
  });

  webServer.on("/sbversion", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", __COMPILED_APP_VERSION__);
  });

  webServer.onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("/");
  });

  webServer.begin();

#ifndef MYCILA_SAFEBOOT_NO_MDNS
  MDNS.addService("http", "tcp", 80);
#endif

  LOG("Web Server started\n");
}

static void set_next_partition_to_boot() {
  const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  if (partition) {
    esp_ota_set_boot_partition(partition);
  }
}

static void start_network_manager() {
  // load ESPConnect configuration
  espConnect.loadConfiguration(espConnectConfig);
  espConnect.setBlocking(true);
  espConnect.setAutoRestart(false);

  // reuse a potentially set hostname from main app, or set a default one
  if (!espConnectConfig.hostname.length()) {
    espConnectConfig.hostname = "SafeBoot-" + getChipIDStr();
  }

  // If the passed config is to be in AP mode, or has a SSID that's fine.
  // If the passed config is empty, we need to check if the board supports ETH.
  // - For boards relying only on Wifi, if a SSID is not set and AP is not set (config empty), then we need to go to AP mode.
  // - For boards supporting Ethernet, we do not know if an Ethernet cable is plugged, so we cannot directly start AP mode, because the config might be no AP mode and no SSID.
  //   So we will start, wait for connect timeout (20 seconds), to get DHCP address from ETH, and if failed, we start in AP mode
  if (!espConnectConfig.apMode && !espConnectConfig.wifiSSID.length()) {
#ifdef ESPCONNECT_ETH_SUPPORT
    espConnect.setCaptivePortalTimeout(20);
#else
    espConnectConfig.apMode = true;
#endif
  }

  espConnect.listen([](Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    if (state == Mycila::ESPConnect::State::NETWORK_TIMEOUT) {
      LOG("Connect timeout! Starting AP mode...\n");
      // if ETH DHCP times out, we start AP mode
      espConnectConfig.apMode = true;
      espConnect.setConfig(espConnectConfig);
    }
  });

  // connect...
  espConnect.begin(espConnectConfig.hostname.c_str(), "", espConnectConfig);

  if (espConnectConfig.apMode) {
    LOG("AP: %s\n", espConnect.getAccessPointSSID().c_str());
  } else if (espConnect.getWiFiSSID().length()) {
    LOG("SSID: %s\n", espConnect.getWiFiSSID().c_str());
  }
  LOG("IP: %s\n", espConnect.getIPAddress().toString().c_str());
  LOG("Hostname: %s\n", espConnect.getHostname().c_str());
}

static void start_mdns() {
#ifndef MYCILA_SAFEBOOT_NO_MDNS
  MDNS.begin(espConnectConfig.hostname.c_str());
  LOG("mDNS started\n");
#endif
}

static void start_arduino_ota() {
  ArduinoOTA.setHostname(espConnectConfig.hostname.c_str());
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.setMdnsEnabled(true);
  ArduinoOTA.begin();
  LOG("OTA Server started on port 3232\n");
}

void setup() {
#ifdef SAFEBOOT_LOGGING
  Serial.begin(115200);
  #if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
  #else
  while (!Serial)
    yield();
  #endif
#endif

  LOG("Version: %s\n", __COMPILED_APP_VERSION__);
  set_next_partition_to_boot();
  start_network_manager();
  start_mdns();
  start_web_server();
  start_arduino_ota();
}

void loop() {
  ArduinoOTA.handle();
}
