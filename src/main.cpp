// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */

#include <ArduinoOTA.h>
#include <HTTPUpdateServer.h>
#include <HardwareSerial.h>
#include <MycilaESPConnect.h>

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

static WebServer webServer(80);
static HTTPUpdateServer httpUpdater;
static Mycila::ESPConnect espConnect;
static Mycila::ESPConnect::Config espConnectConfig;

static String getChipIDStr() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String espId = String(chipId, HEX);
  espId.toUpperCase();
  return espId;
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

  // Set next boot partition
  const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  if (partition) {
    esp_ota_set_boot_partition(partition);
  }

  // setup routes
  httpUpdater.setup(&webServer, "/");
  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
  });

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

  // start http
  webServer.begin();
  LOG("Web Server started\n");

#ifndef MYCILA_SAFEBOOT_NO_MDNS
  // Start mDNS
  MDNS.begin(espConnectConfig.hostname.c_str());
  MDNS.addService("http", "tcp", 80);
#endif

  // Start OTA
  ArduinoOTA.setHostname(espConnectConfig.hostname.c_str());
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.setMdnsEnabled(true);
  ArduinoOTA.begin();
  LOG("OTA Server started on port 3232\n");
}

void loop() {
  webServer.handleClient();
  ArduinoOTA.handle();
}
