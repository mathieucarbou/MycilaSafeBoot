// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include <ESPAsyncWebServer.h>
#include <MycilaSystem.h>
#include <WiFi.h>

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
  WiFi.mode(WIFI_AP);
  WiFi.softAP(String("MyAwesomeApp-") + getEspID());

  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", "<form method='POST' action='/safeboot' enctype='multipart/form-data'><input type='submit' value='Restart in SafeBoot mode'></form>");
  });

  webServer.on("/safeboot", HTTP_POST, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Restarting in SafeBoot mode... Look for an Access Point named: SafeBoot-" + getEspID());
    Mycila::System.restartFactory("safeboot");
  });

  webServer.begin();
}

void loop() {
  delay(100);
}
