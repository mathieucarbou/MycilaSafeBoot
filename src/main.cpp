// SPDX-License-Identifier:
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */

#include <ArduinoOTA.h>
#include <HTTPUpdateServer.h>
#include <WiFi.h>

// #include <ESPAsyncWebServer.h>
#include <WebServer.h>

#include <esp_ota_ops.h>
#include <esp_partition.h>

#define TAG "SafeBoot"

String getEspID() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String espId = String(chipId, HEX);
  espId.toUpperCase();
  return espId;
}

// AsyncWebServer webServer(80);
WebServer webServer(80);
HTTPUpdateServer httpUpdater;

String hostname = "SafeBoot-";

void setup() {
  // Init hostname
  hostname += getEspID();

  // Set next boot partition
  const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  if (partition) {
    esp_ota_set_boot_partition(partition);
  }

  // Start AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(hostname);

  // Start ElegantOTA
  httpUpdater.setup(&webServer, "/");

  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
  });

  webServer.begin();

  // Start OTA
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.begin();
}

void loop() {
  webServer.handleClient();
  ArduinoOTA.handle();
}
