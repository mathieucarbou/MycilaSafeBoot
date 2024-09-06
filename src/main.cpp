// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <WiFi.h>

#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "ArduinoOTA.h"

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

  ESP_LOGI(TAG, "Starting: %s...", hostname.c_str());

  // Set next boot partition
  const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  if (partition) {
    esp_ota_set_boot_partition(partition);
  } else {
    ESP_LOGE(TAG, "No OTA partition found to boot from!");
  }

  // Start AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(hostname);
  ESP_LOGI(TAG, "AP started!");

  // Start ElegantOTA
  ElegantOTA.setAutoReboot(true);
  ElegantOTA.onStart([]() {
    ESP_LOGI(TAG, "Start updating...");
  });
  ElegantOTA.begin(&webServer);

  // Start web server
  webServer.rewrite("/", "/update");
  webServer.onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("/");
  });
  webServer.begin();
  ESP_LOGI(TAG, "Web OTA started at http://192.168.4.1");

  // Start OTA
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart([]() {
    if (ArduinoOTA.getCommand() == U_FLASH) {
      ESP_LOGI(TAG, "Updating firmware...");
    } else {
      ESP_LOGI(TAG, "Updating filesystem...");
    }
  });
  ArduinoOTA.onEnd([]() { ESP_LOGI(TAG, "Update finished!"); });
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR) {
      ESP_LOGE(TAG, "Update Error: Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      ESP_LOGE(TAG, "Update Error: Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      ESP_LOGE(TAG, "Update Error: Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      ESP_LOGE(TAG, "Update Error: Receive Failed");
    } else if (error == OTA_END_ERROR) {
      ESP_LOGE(TAG, "Update Error: End Failed");
    }
  });
  ArduinoOTA.begin();
  ESP_LOGI(TAG, "Arduino OTA started at 192.168.4.1:3232");
}

void loop() {
  ElegantOTA.loop();
  ArduinoOTA.handle();
}
