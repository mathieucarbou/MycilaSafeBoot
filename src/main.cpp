// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */

#include <ArduinoOTA.h>
#include <HTTPUpdateServer.h>
#include <HardwareSerial.h>
#include <MycilaESPConnect.h>
#include <MycilaLogger.h>

#ifndef MYCILA_SAFEBOOT_NO_MDNS
  #include <ESPmDNS.h>
#endif

#include <esp_ota_ops.h>
#include <esp_partition.h>

#define TAG "SafeBoot"

Mycila::Logger logger;

static WebServer webServer(80);
static HTTPUpdateServer httpUpdater;
static Mycila::ESPConnect espConnect;
static Mycila::ESPConnect::Config espConnectConfig;
static String hostname;

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
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
  delay(100);
#else
  while (!Serial)
    yield();
#endif

  logger.forwardTo(&Serial);
  logger.setLevel(ARDUHAL_LOG_LEVEL_DEBUG);

  // Init hostname
  hostname = "SafeBoot-" + getChipIDStr();
  logger.info(TAG, "Hostname: %s", hostname.c_str());

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
      logger.warn(TAG, "Connect timeout! Starting AP mode instead...");
      // if ETH DHCP times out, we start AP mode
      espConnectConfig.apMode = true;
      espConnect.setConfig(espConnectConfig);
    }
  });

  // connect...
  espConnect.begin(hostname.c_str(), hostname.c_str(), "", espConnectConfig);

  logger.info(TAG, "Connected to network!");
  logger.info(TAG, "IP Address: %s", espConnect.getIPAddress().toString().c_str());
  logger.info(TAG, "Hostname: %s", espConnect.getHostname().c_str());
  if (espConnect.getWiFiSSID().length()) {
    logger.info(TAG, "WiFi SSID: %s", espConnect.getWiFiSSID().c_str());
  }

  // starte http
  logger.info(TAG, "Starting HTTP server...");
  webServer.begin();

#ifndef MYCILA_SAFEBOOT_NO_MDNS
  // Start mDNS
  MDNS.begin(hostname.c_str());
  MDNS.addService("http", "tcp", 80);
#endif

  // Start OTA
  logger.info(TAG, "Starting OTA server...");
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.setMdnsEnabled(true);
  ArduinoOTA.begin();

  logger.info(TAG, "Done!");
}

void loop() {
  webServer.handleClient();
  ArduinoOTA.handle();
}
