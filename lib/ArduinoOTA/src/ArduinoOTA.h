// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __ARDUINO_OTA_H
#define __ARDUINO_OTA_H

#include "Network.h"
#include "Update.h"

#define INT_BUFFER_SIZE 16

typedef enum {
  OTA_IDLE,
  OTA_RUNUPDATE
} ota_state_t;

typedef enum {
  OTA_AUTH_ERROR,
  OTA_BEGIN_ERROR,
  OTA_CONNECT_ERROR,
  OTA_RECEIVE_ERROR,
  OTA_END_ERROR
} ota_error_t;

class ArduinoOTAClass {
  public:
    ArduinoOTAClass();
    ~ArduinoOTAClass();

    // Sets the device hostname. Default esp32-xxxxxx
    ArduinoOTAClass& setHostname(const char* hostname);

    // Starts the ArduinoOTA service
    void begin();

    // Ends the ArduinoOTA service
    void end();

    // Call this in loop() to run the service
    void handle();

    // Gets update command type after OTA has started. Either U_FLASH or U_SPIFFS
    int getCommand();

    void setTimeout(int timeoutInMillis);

  private:
    const char* _hostname;
    String _nonce;
    NetworkUDP _udp_ota;
    bool _initialized;
    ota_state_t _state;
    int _size;
    int _cmd;
    int _ota_port;
    IPAddress _ota_ip;
    String _md5;

    void _runUpdate(void);
    void _onRx(void);
    int parseInt(void);
    String readStringUntil(char end);
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ARDUINOOTA)
extern ArduinoOTAClass ArduinoOTA;
#endif

#endif /* __ARDUINO_OTA_H */
