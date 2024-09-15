// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */

#include <ArduinoOTA.h>
#include <ElegantOTA.h>
#include <WiFi.h>

#include <ESPAsyncWebServer.h>

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

AsyncWebServer webServer(80);
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
  ElegantOTA.setAutoReboot(true);
  ElegantOTA.begin(&webServer);

  // Start web server
  webServer.rewrite("/", "/update");
  webServer.onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("/");
  });
  webServer.begin();

  // Start OTA
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.begin();
}

void loop() {
  ElegantOTA.loop();
  ArduinoOTA.handle();
}
