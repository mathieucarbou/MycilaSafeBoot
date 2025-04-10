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

#ifndef LWIP_OPEN_SRC
#define LWIP_OPEN_SRC
#endif
#include "ArduinoOTA.h"
#include "NetworkClient.h"
#ifndef MYCILA_SAFEBOOT_NO_MDNS
#include "ESPmDNS.h"
#endif
#include "Update.h"

// #define OTA_DEBUG Serial

ArduinoOTAClass::ArduinoOTAClass() : _initialized(false), _state(OTA_IDLE), _size(0), _cmd(0) {}

ArduinoOTAClass::~ArduinoOTAClass() {
  end();
}

ArduinoOTAClass& ArduinoOTAClass::setHostname(const char* hostname) {
  _hostname = hostname;
  return *this;
}

void ArduinoOTAClass::begin() {
  if (_initialized) {
    return;
  }

  if (!_udp_ota.begin(3232)) {
    return;
  }
#ifndef MYCILA_SAFEBOOT_NO_MDNS
#ifdef CONFIG_MDNS_MAX_INTERFACES
  MDNS.begin(_hostname);
  MDNS.enableArduino(3232, false);
#endif
#endif
  _initialized = true;
  _state = OTA_IDLE;
}

int ArduinoOTAClass::parseInt() {
  char data[INT_BUFFER_SIZE];
  uint8_t index = 0;
  char value;
  while (_udp_ota.peek() == ' ') {
    _udp_ota.read();
  }
  while (index < INT_BUFFER_SIZE - 1) {
    value = _udp_ota.peek();
    if (value < '0' || value > '9') {
      data[index++] = '\0';
      return atoi(data);
    }
    data[index++] = _udp_ota.read();
  }
  return 0;
}

String ArduinoOTAClass::readStringUntil(char end) {
  String res = "";
  int value;
  while (true) {
    value = _udp_ota.read();
    if (value <= 0 || value == end) {
      return res;
    }
    res += (char)value;
  }
  return res;
}

void ArduinoOTAClass::_onRx() {
  if (_state == OTA_IDLE) {
    int cmd = parseInt();
    if (cmd != U_FLASH && cmd != U_SPIFFS) {
      return;
    }
    _cmd = cmd;
    _ota_port = parseInt();
    _size = parseInt();
    _udp_ota.read();
    _md5 = readStringUntil('\n');
    _md5.trim();
    if (_md5.length() != 32) {
      return;
    }

    _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
    _udp_ota.print("OK");
    _udp_ota.endPacket();
    _ota_ip = _udp_ota.remoteIP();
    _state = OTA_RUNUPDATE;
  }
}

void ArduinoOTAClass::_runUpdate() {
  if (!Update.begin(_size, _cmd, -1, LOW, nullptr)) {
    _state = OTA_IDLE;
    return;
  }
  Update.setMD5(_md5.c_str());
  NetworkClient client;
  if (!client.connect(_ota_ip, _ota_port)) {
    _state = OTA_IDLE;
  }

  uint32_t written = 0, total = 0, tried = 0;

  while (!Update.isFinished() && client.connected()) {
    size_t waited = 1000;
    size_t available = client.available();
    while (!available && waited) {
      delay(1);
      waited -= 1;
      available = client.available();
    }
    if (!waited) {
      if (written && tried++ < 3) {
        if (!client.printf("%lu", written)) {
          _state = OTA_IDLE;
          break;
        }
        continue;
      }
      _state = OTA_IDLE;
      Update.abort();
      return;
    }
    if (!available) {
      _state = OTA_IDLE;
      break;
    }
    tried = 0;
    static uint8_t buf[1460];
    if (available > 1460) {
      available = 1460;
    }
    size_t r = client.read(buf, available);
    if (r != available) {
      if ((int32_t)r < 0) {
        delay(1);
        continue; // let's not try to write 4 gigabytes when client.read returns -1
      }
    }

    written = Update.write(buf, r);
    if (written > 0) {
      client.printf("%lu", written);
      total += written;
    }
  }

  if (Update.end()) {
    client.print("OK");
    client.stop();
    delay(10);
    // let serial/network finish tasks that might be given in _end_callback
    delay(100);
    ESP.restart();
  } else {
    Update.printError(client);
    client.stop();
    delay(10);
    _state = OTA_IDLE;
  }
}

void ArduinoOTAClass::end() {
  _initialized = false;
  _udp_ota.stop();
#ifndef MYCILA_SAFEBOOT_NO_MDNS
#ifdef CONFIG_MDNS_MAX_INTERFACES
  MDNS.end();
#endif
#endif
  _state = OTA_IDLE;
}

void ArduinoOTAClass::handle() {
  if (!_initialized) {
    return;
  }
  if (_state == OTA_RUNUPDATE) {
    _runUpdate();
    _state = OTA_IDLE;
  }
  if (_udp_ota.parsePacket()) {
    _onRx();
  }
  _udp_ota.clear(); // always clear, even zero length packets must be cleared.
}

int ArduinoOTAClass::getCommand() {
  return _cmd;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ARDUINOOTA)
ArduinoOTAClass ArduinoOTA;
#endif
