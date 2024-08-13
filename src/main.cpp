// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#define TAG "SafeBoot"

AsyncWebServer webServer(80);

String getEspID() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String espId = String(chipId, HEX);
  espId.toUpperCase();
  return espId;
}

void setup() {
  ESP_LOGI(TAG, "Starting...");
  const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  if (partition) {
    esp_ota_set_boot_partition(partition);
  } else {
    ESP_LOGE(TAG, "No OTA partition found to boot from!");
  }

  ElegantOTA.setAutoReboot(true);
  ElegantOTA.begin(&webServer);

  webServer.rewrite("/", "/update");

  webServer.onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("/");
  });

  WiFi.mode(WIFI_AP);
  WiFi.softAP(String("SafeBoot-") + getEspID());
  ESP_LOGI(TAG, "AP started!");

  webServer.begin();
  ESP_LOGI(TAG, "Ready at http://192.168.4.1");
}

void loop() {
  ElegantOTA.loop();
}
