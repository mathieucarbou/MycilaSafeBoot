#pragma once

#include <StreamString.h>
#include <Update.h>
#include <WebServer.h>

static const char serverIndex[] PROGMEM =
  R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
     </head>
     <body>
     <form method='POST' action='' enctype='multipart/form-data'>
         Firmware:<br>
         <input type='file' accept='.bin,.bin.gz' name='firmware'>
         <input type='submit' value='Update Firmware'>
     </form>
     </body>
     </html>)";
static const char successResponse[] PROGMEM = "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";

class HTTPUpdateServer {
  public:
    void setup(WebServer* server) {
      setup(server, "/update");
    }

    void setup(WebServer* server, const String& path) {
      _server = server;

      // handler for the /update form page
      _server->on(path.c_str(), HTTP_GET, [&]() {
        _server->send_P(200, PSTR("text/html"), serverIndex);
      });

      // handler for the /update form POST (once file upload finishes)
      _server->on(
        path.c_str(),
        HTTP_POST,
        [&]() {
          if (Update.hasError()) {
            _server->send(200, "text/html", "Update error: " + _updaterError);
          } else {
            _server->client().setNoDelay(true);
            _server->send(200, "text/html", successResponse);
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
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace, U_FLASH)) { // start with max available size
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
