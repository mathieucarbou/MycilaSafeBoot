#pragma once

#include <StreamString.h>
#include <Update.h>
#include <WebServer.h>

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

class HTTPUpdateServer {
  public:
    void setup(WebServer* server) {
      setup(server, "/update");
    }

    void setup(WebServer* server, const String& path) {
      _server = server;

      // handler for cancel
      _server->on(
        path == "/" ? "/cancel" : (path + "/cancel"),
        HTTP_POST,
        [&]() {
          _server->send(200, "text/plain", cancelResponse);
          _server->client().stop();
          delay(500);
          ESP.restart();
        },
        [&]() {});

      // handler for the /update form page
      _server->on(path, HTTP_GET, [&]() {
        _server->sendHeader("Content-Encoding", "gzip");
        _server->send_P(200, "text/html", reinterpret_cast<const char*>(update_html_start), update_html_end - update_html_start);
      });

      // handler for the /update form POST (once file upload finishes)
      _server->on(
        path,
        HTTP_POST,
        [&]() {
          if (Update.hasError()) {
            _server->send(500, "text/plain", "Update error: " + _updaterError);
          } else {
            _server->client().setNoDelay(true);
            _server->send(200, "text/plain", successResponse);
            _server->client().stop();
            delay(500);
            ESP.restart();
          }
        },
        [&]() {
          // handler for the file upload, gets the sketch bytes, and writes
          // them through the Update object
          HTTPUpload& upload = _server->upload();

          if (upload.status == UPLOAD_FILE_START) {
            _updaterError.clear();
            int otaMode = U_FLASH;
            if (_server->hasArg("mode") && _server->arg("mode") == "1") {
              otaMode = U_SPIFFS;
            }
            LOG("Mode: %d\n", otaMode);
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, otaMode)) { // start with max available size
              _setUpdaterError();
            }
          } else if (upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              _setUpdaterError();
            }
          } else if (upload.status == UPLOAD_FILE_END && !_updaterError.length()) {
            if (!Update.end(true)) { // true to set the size to the current progress
              _setUpdaterError();
            }
          } else if (upload.status == UPLOAD_FILE_ABORTED) {
            Update.end();
          }
          delay(0);
        });

      // serve boardname info (when available)
      _server->on("/chipspecs", HTTP_GET, [&]() {
        String chipSpecs = ESP.getChipModel();
        chipSpecs += " (" + String(ESP.getFlashChipSize() >> 20) + " MB)";
        _server->send(200, "text/plain", chipSpecs.c_str());
      });

      // serve sbversion info (when available)
      _server->on("/sbversion", HTTP_GET, [&]() {
        _server->send(200, "text/plain", __COMPILED_APP_VERSION__);
      });
    }

  protected:
    void _setUpdaterError() {
      StreamString str;
      Update.printError(str);
      _updaterError = str.c_str();
    }

  private:
    WebServer* _server;
    String _updaterError;
};
