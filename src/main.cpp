// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2025 Mathieu Carbou
 */

#include <ArduinoOTA.h>
#include <HardwareSerial.h>
#include <MycilaESPConnect.h>
#include <StreamString.h>
// #include <WebServer.h>

#include <esp_http_server.h>
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

#define SCRATCH_BUFSIZE 8192

extern const char* __COMPILED_APP_VERSION__;
extern const uint8_t update_html_start[] asm("_binary__pio_embed_website_html_gz_start");
extern const uint8_t update_html_end[] asm("_binary__pio_embed_website_html_gz_end");
static const char* successResponse = "Update Success! Rebooting...";
static const char* cancelResponse = "Rebooting...";

static Mycila::ESPConnect espConnect;
static Mycila::ESPConnect::Config espConnectConfig;
static StreamString updaterError;

// static WebServer webServer(80);
httpd_handle_t server = NULL;
httpd_config_t config = HTTPD_DEFAULT_CONFIG();

static String getChipIDStr() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  String espId = String(chipId, HEX);
  espId.toUpperCase();
  return espId;
}

// 404
esp_err_t handler_404(httpd_req_t* req, httpd_err_code_t err) {
  httpd_resp_set_status(req, "302 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  return ESP_OK;
}

// GET /
static esp_err_t handler_get_root(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_send(req, reinterpret_cast<const char*>(update_html_start), update_html_end - update_html_start);
  return ESP_OK;
}
static const httpd_uri_t route_get_root = {
  .uri = "/",
  .method = HTTP_GET,
  .handler = handler_get_root,
};

// GET /chipspecs
static esp_err_t handler_get_chip(httpd_req_t* req) {
  String chipSpecs = ESP.getChipModel();
  chipSpecs += " (" + String(ESP.getFlashChipSize() >> 20) + " MB)";
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, chipSpecs.c_str());
  return ESP_OK;
}
static const httpd_uri_t route_get_chip = {
  .uri = "/chipspecs",
  .method = HTTP_GET,
  .handler = handler_get_chip,
};

// GET /sbversion
static esp_err_t handler_get_version(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, __COMPILED_APP_VERSION__);
  return ESP_OK;
}
static const httpd_uri_t route_get_version = {
  .uri = "/sbversion",
  .method = HTTP_GET,
  .handler = handler_get_version,
};

// POST /cancel
static esp_err_t handler_post_cancel(httpd_req_t* req) {
  httpd_resp_set_hdr(req, "Connection", "close");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, cancelResponse);

  delay(1000);
  ESP.restart();

  return ESP_OK;
}
static const httpd_uri_t route_post_cancel = {
  .uri = "/cancel",
  .method = HTTP_POST,
  .handler = handler_post_cancel,
};

// POST /
static char scratch[SCRATCH_BUFSIZE];
static esp_err_t handler_post_root(httpd_req_t* req) {
  int otaMode = U_FLASH;

  char* buffer = nullptr;
  size_t buffer_len;

  // parse query params
  buffer_len = httpd_req_get_url_query_len(req) + 1;
  if (buffer_len > 1) {
    buffer = new char[buffer_len];
    if (httpd_req_get_url_query_str(req, buffer, buffer_len) == ESP_OK) {
      if (strstr(buffer, "mode=1")) {
        otaMode = U_SPIFFS;
      }
    }
    delete[] buffer;
  }
  LOG("Mode: %d\n", otaMode);

  if (!Update.begin(UPDATE_SIZE_UNKNOWN, otaMode)) {
    updaterError.concat("Update error: ");
    Update.printError(updaterError);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, updaterError.c_str());
    return ESP_FAIL;
  }

  int received;
  int remaining = req->content_len;

  while (remaining > 0) {
    if ((received = httpd_req_recv(req, scratch, min(remaining, SCRATCH_BUFSIZE))) <= 0) {
      if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        /* Retry if timeout occurred */
        continue;
      }

      Update.end();
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
      return ESP_FAIL;
    }

    if (Update.write((uint8_t*)buffer, received) != received) {
      updaterError.concat("Update error: ");
      Update.printError(updaterError);
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, updaterError.c_str());
      return ESP_FAIL;
    }

    remaining -= received;
  }

  if (!Update.end(true)) {
    updaterError.concat("Update error: ");
    Update.printError(updaterError);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, updaterError.c_str());
    return ESP_FAIL;
  }

  httpd_resp_set_hdr(req, "Connection", "close");
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, successResponse);

  delay(1000);
  ESP.restart();

  return ESP_OK;
}
static const httpd_uri_t route_post_root = {
  .uri = "/",
  .method = HTTP_POST,
  .handler = handler_post_root,
};

static void start_web_server() {
  config.lru_purge_enable = true;

  ESP_ERROR_CHECK(httpd_start(&server, &config));

  httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, handler_404);
  httpd_register_uri_handler(server, &route_get_root);
  httpd_register_uri_handler(server, &route_get_chip);
  httpd_register_uri_handler(server, &route_get_version);
  httpd_register_uri_handler(server, &route_post_cancel);
  httpd_register_uri_handler(server, &route_post_root);

  // webServer.onNotFound([]() {
  //   webServer.sendHeader("Location", "/");
  //   webServer.send(302, "text/plain", "");
  // });

  // webServer.on("/", HTTP_GET, [&]() {
  //   webServer.sendHeader("Content-Encoding", "gzip");
  //   webServer.send_P(200, "text/html", reinterpret_cast<const char*>(update_html_start), update_html_end - update_html_start);
  // });

  // webServer.on("/chipspecs", HTTP_GET, [&]() {
  //   String chipSpecs = ESP.getChipModel();
  //   chipSpecs += " (" + String(ESP.getFlashChipSize() >> 20) + " MB)";
  //   webServer.send(200, "text/plain", chipSpecs.c_str());
  // });

  // webServer.on("/sbversion", HTTP_GET, [&]() {
  //   webServer.send(200, "text/plain", __COMPILED_APP_VERSION__);
  // });

  // webServer.on("/cancel", HTTP_POST, [&]() {
  //     webServer.send(200, "text/plain", cancelResponse);
  //     webServer.client().stop();
  //     delay(1000);
  //     ESP.restart(); }, [&]() {});

  // webServer.on("/", HTTP_POST, [&]() {
  //     if (Update.hasError()) {
  //       webServer.send(500, "text/plain", "Update error: " + updaterError);
  //     } else {
  //       webServer.client().setNoDelay(true);
  //       webServer.send(200, "text/plain", successResponse);
  //       webServer.client().stop();
  //       delay(500);
  //       ESP.restart();
  //     } }, [&]() {
  //     // handler for the file upload, gets the sketch bytes, and writes
  //     // them through the Update object
  //     HTTPUpload& upload = webServer.upload();

  //     if (upload.status == UPLOAD_FILE_START) {
  //       updaterError.clear();
  //       int otaMode = U_FLASH;
  //       if (webServer.hasArg("mode") && webServer.arg("mode") == "1") {
  //         otaMode = U_SPIFFS;
  //       }
  //       LOG("Mode: %d\n", otaMode);
  //       if (!Update.begin(UPDATE_SIZE_UNKNOWN, otaMode)) { // start with max available size
  //         Update.printError(updaterError);
  //       }
  //     } else if (upload.status == UPLOAD_FILE_WRITE && !updaterError.length()) {
  //       if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
  //         Update.printError(updaterError);
  //       }
  //     } else if (upload.status == UPLOAD_FILE_END && !updaterError.length()) {
  //       if (!Update.end(true)) { // true to set the size to the current progress
  //         Update.printError(updaterError);
  //       }
  //     } else if (upload.status == UPLOAD_FILE_ABORTED) {
  //       Update.end();
  //     }
  //     delay(0); });

  // webServer.begin();

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
  // webServer.handleClient();
  ArduinoOTA.handle();
}
