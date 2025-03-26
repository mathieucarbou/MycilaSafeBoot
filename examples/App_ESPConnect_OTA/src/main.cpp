// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */
#include <MycilaESPConnect.h>
#include <MycilaSystem.h>

AsyncWebServer webServer(80);
Mycila::ESPConnect espConnect(webServer);

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  // reboot esp into SafeBoot
  webServer.on("/safeboot", HTTP_POST, [](AsyncWebServerRequest* request) {
    Serial.println("Restarting in SafeBoot mode...");
    request->send(200, "text/html", "<META http-equiv=\"refresh\" content=\"10;URL=/\">Restarting in SafeBoot mode...");
    Mycila::System::restartFactory("safeboot", 250);
  });

  // network state listener is required here in async mode
  espConnect.listen([](__unused Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    switch (state) {
      case Mycila::ESPConnect::State::NETWORK_CONNECTED:
      case Mycila::ESPConnect::State::AP_STARTED:
        // serve your home page here
        webServer.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
                   request->send(200, "text/html", "<h1>MyAwsomeApp</h1><br><form method='POST' action='/safeboot' enctype='multipart/form-data'><input type='submit' value='Restart in SafeBoot mode'></form>");
                 })
          .setFilter([](__unused AsyncWebServerRequest* request) { return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
        webServer.begin();
        break;

      case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
        webServer.end();
        break;

      default:
        break;
    }
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);

  Serial.println("====> Trying to connect to saved WiFi or will start portal in the background...");

  espConnect.begin(APP_NAME, APP_NAME);

  Serial.println("====> setup() completed...");
}

void loop() {
  espConnect.loop();
}
