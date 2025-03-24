#pragma once

#include <StreamString.h>
#include <Update.h>
#include <WebServer.h>

extern const char* __COMPILED_APP_VERSION__;

#ifndef MYCILA_SAFEBOOT_FANCY_WEBSITE
static String serverIndex =
  R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
     </head>
     <body>
     <h1>SafeBoot</h1>
     Chip: ${B}<br>SafeBoot-Version: ${V}<br><br>
     <form method='POST' action='' enctype='multipart/form-data'>
         <label for='firmware'><strong>Firmware:</strong></label>
         <br>
         <input type='file' accept='.bin,.bin.gz' name='firmware'>
         <br>
         <input type='submit' value='Update Firmware' onclick="this.disabled=true; this.value = 'Updating...'; this.form.submit();">
     </form>
     <br>
     <form method='POST' action='cancel' enctype='multipart/form-data'>
        <input type='hidden' name='cancel' value='true'>
         <input type='submit' value='Cancel and Reboot'>
     </form>
     </body>
     </html>)";
static const char* successResponse = "<META http-equiv=\"refresh\" content=\"10;URL=/\">Update Success! Rebooting...";
static const char* cancelResponse = "<META http-equiv=\"refresh\" content=\"10;URL=/\">Rebooting...";
#else
// embedded new Website
extern const uint8_t update_html_start[] asm("_binary_assets_minified_update_html_gz_start");
extern const uint8_t update_html_end[] asm("_binary_assets_minified_update_html_gz_end");
static const char* successResponse = "Update Success! Rebooting...";
static const char* cancelResponse = "Rebooting...";
#endif

class HTTPUpdateServer {
  public:
    void setup(WebServer* server) {
      setup(server, "/update");
    }

    void setup(WebServer* server, const String& path) {
      _server = server;

      // Add chip model and flash size to Website
      _chipSpecs = ESP.getChipModel();
      _chipSpecs += " (" + String(ESP.getFlashChipSize() >> 20) + " MB)";
#ifndef MYCILA_SAFEBOOT_FANCY_WEBSITE
      serverIndex.replace("${B}", _chipSpecs);

      // Add SafeBoot-Version to Website
      serverIndex.replace("${V}", __COMPILED_APP_VERSION__);
#endif

      // handler for cancel
      _server->on(
        path == "/" ? "/cancel" : (path + "/cancel"),
        HTTP_POST,
        [&]() {
#ifndef MYCILA_SAFEBOOT_FANCY_WEBSITE
          _server->send(200, "text/html", cancelResponse);
#else
          _server->send(200, "text/plain", cancelResponse);
#endif
          _server->client().stop();
          delay(500);
          ESP.restart();
        },
        [&]() {});

      // handler for the /update form page
      _server->on(path, HTTP_GET, [&]() {
#ifndef MYCILA_SAFEBOOT_FANCY_WEBSITE
        _server->send(200, "text/html", serverIndex);
#else
        _server->sendHeader(F("Content-Encoding"), F("gzip"));
        _server->send_P(200,
                        "text/html",
                        reinterpret_cast<const char*>(update_html_start),
                        update_html_end - update_html_start);
#endif
      });

      // handler for the /update form POST (once file upload finishes)
      _server->on(
        path,
        HTTP_POST,
        [&]() {
          if (Update.hasError()) {
#ifndef MYCILA_SAFEBOOT_FANCY_WEBSITE
            _server->send(200, "text/html", "Update error: " + _updaterError);
#else
            _server->send(500, "text/plain", "Update error: " + _updaterError);
#endif
          } else {
            _server->client().setNoDelay(true);
#ifndef MYCILA_SAFEBOOT_FANCY_WEBSITE
            _server->send(200, "text/html", successResponse);
#else
            _server->send(200, "text/plain", successResponse);
#endif
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
#ifndef MYCILA_SAFEBOOT_FANCY_WEBSITE
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace, U_FLASH)) { // start with max available size
#else
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, static_cast<int>(_otaMode))) { // start with max available size
#endif
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

#ifdef MYCILA_SAFEBOOT_FANCY_WEBSITE
      // set OTA Mode
      _server->on("/ota_mode", HTTP_POST, [&]() {
        if (_server->hasArg("plain")) {
          if (_server->arg("plain") != "0") {
            _otaMode = U_SPIFFS;
          }
        }

        _server->send(200, "text/plain", _otaMode == U_FLASH ? "0" : "1");
      });

      // serve boardname info (when available)
      _server->on("/chipspecs", HTTP_GET, [&]() {
          _server->send(200, "text/plain", _chipSpecs.c_str());
      });

      // serve sbversion info (when available)
      _server->on("/sbversion", HTTP_GET, [&]() {
        _server->send(200, "text/plain", __COMPILED_APP_VERSION__);
      });
#endif
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
    String _chipSpecs;
#ifdef MYCILA_SAFEBOOT_FANCY_WEBSITE
    uint32_t _otaMode = U_FLASH;
#endif
};
