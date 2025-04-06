# MycilaSafeBoot

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaSafeBoot.svg)](https://GitHub.com/mathieucarbou/MycilaSafeBoot/releases/)
[![Download](https://img.shields.io/badge/Download-safeboot-green.svg)](https://github.com/mathieucarbou/MycilaSafeBoot/releases)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

[![Build](https://github.com/mathieucarbou/MycilaSafeBoot/actions/workflows/build.yml/badge.svg)](https://github.com/mathieucarbou/MycilaSafeBoot/actions/workflows/build.yml)
[![GitHub latest commit](https://badgen.net/github/last-commit/mathieucarbou/MycilaSafeBoot)](https://GitHub.com/mathieucarbou/MycilaSafeBoot/commit/)
[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/mathieucarbou/MycilaSafeBoot)

MycilaSafeBoot is a Web OTA recovery partition for ESP32 / Arduino.

It allows to have only one application partition to use the maximum available flash size.

The idea is not new: [Tasmota also uses a SafeBoot partition](https://tasmota.github.io/docs/Safeboot/).



- [Overview](#overview)
- [How it works](#how-it-works)
- [How to integrate the SafeBoot in your project](#how-to-integrate-the-safeboot-in-your-project)
- [How to build the SafeBoot firmware image](#how-to-build-the-safeboot-firmware-image)
- [SafeBoot Example](#safeboot-example)
- [How to reboot in SafeBoot mode from the app](#how-to-reboot-in-safeboot-mode-from-the-app)
- [Configuration options to manage build size](#configuration-options-to-manage-build-size)
- [Options matrix](#options-matrix)
- [Default board options](#default-board-options)
- [How to OTA update firmware from PlatformIO](#how-to-ota-update-firmware-from-platformio)

![](https://private-user-images.githubusercontent.com/61346/426535795-7eda5f6e-7900-4380-921f-8e54fb2b2e2c.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3NDM5NjQ4MjIsIm5iZiI6MTc0Mzk2NDUyMiwicGF0aCI6Ii82MTM0Ni80MjY1MzU3OTUtN2VkYTVmNmUtNzkwMC00MzgwLTkyMWYtOGU1NGZiMmIyZTJjLnBuZz9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNTA0MDYlMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjUwNDA2VDE4MzUyMlomWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPTdjN2Y5YmQ2YmU0YjAyOTEzNzdiMzc4YTExNzM4NjgwNWI5MzRkMjc4MWJjZDZlNWMwNTExYmIxMDMyYTVjM2MmWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0In0.uJq7XczPOdOlZTsLLgYw6IeRHdbwrk1n8y7YOnuMVGY)

## Overview

Usually, a normal partition table when supporting OTA updates on a 4MB ESP32 looks like this:

```
# Name,   Type, SubType, Offset,   Size,    Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xE000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x1F0000,
app1,     app,  ota_1,   0x200000, 0x1F0000,
spiffs,   data, spiffs,  0x3F0000, 0x10000,
```

which can also be written as:

```
# Name   ,Type ,SubType  ,Offset ,Size  ,Flags
nvs      ,data ,nvs      ,36K    ,20K   ,
otadata  ,data ,ota      ,56K    ,8K    ,
app0     ,app  ,ota_0    ,64K    ,1984K ,
app1     ,app  ,ota_1    ,2048K  ,1984K ,
spiffs   ,data ,spiffs   ,4032K  ,64K   ,
```

Because of the need to have 2 partitions with the same size, the firmware is then limited to only 2MB in this case when the ESP has 4MB flash.
2MB is left unused (the OTA process will switch to the updated partition once completed).

**A SafeBoot partition is a small bootable recovery partition allowing you to flash the firmware.**
Consequently, the firmware can take all the remaining space on the flash.

**The SafeBoot partition is 655360 bytes only.**

**Example for 4MB partition** with a SafeBoot partition and an application size of 3MB:

```
# Name,   Type, SubType,  Offset,   Size,    Flags
nvs,      data, nvs,      0x9000,   0x5000,
otadata,  data, ota,      0xE000,   0x2000,
safeboot, app,  factory,  0x10000,  0xA0000,
app,      app,  ota_0,    0xB0000,  0x330000,
spiffs,   data, spiffs,   0x3E0000, 0x10000,
coredump, data, coredump, 0x3F0000, 0x10000,
```

which can also be written as:

```
# Name   ,Type ,SubType  ,Offset ,Size  ,Flags
nvs      ,data ,nvs      ,36K    ,20K   ,
otadata  ,data ,ota      ,56K    ,8K    ,
safeboot ,app  ,factory  ,64K    ,640K  ,
app      ,app  ,ota_0    ,704K   ,3264K ,
spiffs   ,data ,spiffs   ,3968K  ,64K   ,
coredump ,data ,coredump ,4032K  ,64K   ,
```

**Example for 8Mb partition** with a SafeBoot partition and an application size of 7MB:

```
# Name,   Type, SubType,  Offset,   Size,    Flags
nvs,      data, nvs,      0x9000,   0x5000,
otadata,  data, ota,      0xE000,   0x2000,
safeboot, app,  factory,  0x10000,  0xA0000,
app,      app,  ota_0,    0xB0000,  0x730000,
spiffs    data, spiffs,   0x7E0000, 0x10000,
coredump, data, coredump, 0x7F0000, 0x10000,
```

which can also be written as:

```
# Name   ,Type ,SubType  ,Offset ,Size  ,Flags
nvs      ,data ,nvs      ,36K    ,20K   ,
otadata  ,data ,ota      ,56K    ,8K    ,
safeboot ,app  ,factory  ,64K    ,640K  ,
app      ,app  ,ota_0    ,704K   ,7312K ,
spiffs   ,data ,spiffs   ,8128K  ,64K   ,
coredump ,data ,coredump ,8192K  ,64K   ,
```

The SafeBoot partition is also automatically booted when the firmware is missing.

## How it works

1. When a user wants to update the app firmware, we have to tell the app to reboot in recovery mode.

2. Once booted in recovery mode, an Access Point is created with the SSID `SafeBoot`.

[![](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ssid.jpeg)](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ssid.jpeg)

3. Connect to the Access Point.

4. Now, you can flash the new firmware, either with `ArduinoOTA` or from the web page by going to `http://192.168.4.1`

5. After the flash is successful, the ESP will reboot in the new firmware.

SafeBoot partition also supports [MycilaESPConnect](https://github.com/mathieucarbou/MycilaESPConnect), which means if your application saves some network settings (WiFi SSID, Ethernet or WiFi static IP, etc), they will be reused.

## How to integrate the SafeBoot in your project

In the PIO file, some settings are added to specify the partition table and the SafeBoot location and the script to generate the factory image.

```ini
extra_scripts = post:factory.py
board_build.partitions = partitions-4MB-safeboot.csv
board_build.app_partition_name = app
custom_safeboot_url = https://github.com/mathieucarbou/MycilaSafeBoot/releases/download/latest/safeboot-esp32dev.bin
```

It is also possible to point to a folder if you download the SafeBoot project locally:

```ini
custom_safeboot_dir = ../../tools/SafeBoot
```

It is also possible to point to a pre-downloaded safeoot image:

```ini
custom_safeboot_file = safeboot.bin
```

You can find in the [Project Releases](https://github.com/mathieucarbou/MycilaSafeBoot/releases) the list of available SafeBoot images, with the Python script to add to your build.

## How to build the SafeBoot firmware image

Go inside `tools/SafeBoot` and run:

```bash
> pio run -e esp32dev
```

If your board does not exist, you can specify it like this:

```bash
> SAFEBOOT_BOARD=my-board pio run -e safeboot
```

`SAFEBOOT_BOARD` is the environment variable to specify the board to build the SafeBoot firmware for.

At the end you should see these lines:

```
Firmware size valid: 619744 <= 655360
SafeBoot firmware created: /Users/mat/Data/Workspace/me/MycilaSafeBoot/.pio/build/dev/safeboot.bin
```

## SafeBoot Example

Go inside `examples/App` and execute:

```bash
> pio run
```

You should see at the end of the build something like:

```
Generating factory image for serial flashing
Downloading SafeBoot image from https://github.com/mathieucarbou/MycilaSafeBoot/releases/download/latest/safeboot-esp32dev.bin
    Offset | File
 -   0x1000 | /Users/mat/Data/Workspace/me/MycilaSafeBoot/examples/App/.pio/build/esp32dev/bootloader.bin
 -   0x8000 | /Users/mat/Data/Workspace/me/MycilaSafeBoot/examples/App/.pio/build/esp32dev/partitions.bin
 -   0xe000 | /Users/mat/.platformio/packages/framework-arduinoespressif32@src-17df1753722b7b9e1913598420d4e038/tools/partitions/boot_app0.bin
 -  0x10000 | /Users/mat/Data/Workspace/me/MycilaSafeBoot/examples/App/.pio/build/esp32dev/safeboot.bin
 -  0xb0000 | /Users/mat/Data/Workspace/me/MycilaSafeBoot/examples/App/.pio/build/esp32dev/firmware.bin

[...]

Wrote 0x1451a0 bytes to file /Users/mat/Data/Workspace/me/MycilaSafeBoot/examples/App/.pio/build/esp32dev/firmware.factory.bin, ready to flash to offset 0x0
Factory image generated: /Users/mat/Data/Workspace/me/MycilaSafeBoot/examples/App/.pio/build/esp32dev/firmware.factory.bin
```

the `factory.py` script generates a complete factory image named `firmware.factory.bin` with all this content.

It can be downloaded from [https://github.com/mathieucarbou/MycilaSafeBoot/releases](https://github.com/mathieucarbou/MycilaSafeBoot/releases).

Flash this factory image on an ESP32:

```bash
esptool.py write_flash 0x0 .pio/build/esp32dev/firmware.factory.bin
```

Restart the ESP.
The app loads, shows a button to restart in SafeBoot mode.
After clicking on it, the ESP will reboot into SafeBoot mode.
From there, you can access the web page to flash a new firmware, even from another application.

## How to reboot in SafeBoot mode from the app

You can use [MycilaSystem](https://github.com/mathieucarbou/MycilaSystem):

```cpp
#include <MycilaSystem.h>

espConnect.saveConfiguration(); // if you want to save ESPConnect settings for network
Mycila::System::restartFactory("safeboot");
```

or this custom code:

```cpp
#include <esp_ota_ops.h>
#include <esp_partition.h>

const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_FACTORY, partitionName);
if (partition) {
  esp_ota_set_boot_partition(partition);
  esp_restart();
  return true;
} else {
  ESP_LOGE("SafeBoot", "SafeBoot partition not found");
  return false;
}
```

## Configuration options to manage build size

Squezing everything into the SafeBoot partition (655360 bytes only) is a tight fit especially on ethernet enabled boards.

Disabling the logging capabilites saves about 12 kbytes in the final build. Just comment out `MYCILA_SAFEBOOT_LOGGING` in `platformio.ini`.

```ini
; -D MYCILA_SAFEBOOT_LOGGING
```

Disabling mDNS saves about 24 kbytes. Enable both [...]\_NO_DNS options in `platformio.ini` to reduce the build size:

```ini
-D ESPCONNECT_NO_MDNS
-D MYCILA_SAFEBOOT_NO_MDNS
```

### Options matrix

| Board                | mDNS: on, logger: on | mDNS: on, logger: off | mDNS: off, logger: off |
| -------------------- | -------------------- | --------------------- | ---------------------- |
| denky_d4             | NOT SUPPORTED        | OK                    | OK                     |
| esp32-c3-devkitc-02  | OK                   | OK                    | OK                     |
| esp32-c6-devkitc-1   | NOT SUPPORTED        | NOT SUPPORTED         | OK                     |
| esp32-gateway        | NOT SUPPORTED        | OK                    | OK                     |
| esp32-poe            | NOT SUPPORTED        | NOT SUPPORTED         | OK                     |
| esp32-poe-iso        | NOT SUPPORTED        | NOT SUPPORTED         | OK                     |
| esp32-s2-saola-1     | OK                   | OK                    | OK                     |
| esp32-s3-devkitc-1   | OK                   | OK                    | OK                     |
| esp32-solo1          | OK                   | OK                    | OK                     |
| esp32dev             | OK                   | OK                    | OK                     |
| esp32s3box           | OK                   | OK                    | OK                     |
| lilygo-t-eth-lite-s3 | OK                   | OK                    | OK                     |
| lolin_s2_mini        | OK                   | OK                    | OK                     |
| tinypico             | NOT SUPPORTED        | OK                    | OK                     |
| wemos_d1_uno32       | OK                   | OK                    | OK                     |
| wipy3                | NOT SUPPORTED        | OK                    | OK                     |
| wt32-eth01           | NOT SUPPORTED        | NOT SUPPORTED         | OK                     |

## Default board options

| Board                | mDNS | Logging | Ethernet |
| :------------------- | :--: | :-----: | :------: |
| denky_d4             |  ✅  |   ❌    |    ❌    |
| esp32-c3-devkitc-02  |  ✅  |   ✅    |    ❌    |
| esp32-c6-devkitc-1   |  ❌  |   ❌    |    ❌    |
| esp32-gateway        |  ✅  |   ❌    |    ✅    |
| esp32-poe            |  ❌  |   ❌    |    ✅    |
| esp32-poe-iso        |  ❌  |   ❌    |    ✅    |
| esp32-s2-saola-1     |  ✅  |   ✅    |    ❌    |
| esp32-s3-devkitc-1   |  ✅  |   ✅    |    ❌    |
| esp32-solo1          |  ✅  |   ✅    |    ❌    |
| esp32dev             |  ✅  |   ✅    |    ❌    |
| esp32s3box           |  ✅  |   ✅    |    ❌    |
| lilygo-t-eth-lite-s3 |  ✅  |   ✅    |    ✅    |
| lolin_s2_mini        |  ✅  |   ✅    |    ❌    |
| tinypico             |  ✅  |   ❌    |    ❌    |
| wemos_d1_uno32       |  ✅  |   ✅    |    ❌    |
| wipy3                |  ✅  |   ❌    |    ❌    |
| wt32-eth01           |  ❌  |   ❌    |    ✅    |

## How to OTA update firmware from PlatformIO

First make sure you created an HTTP endpoint that can be called to restart the app in SafeBoot mode.
See [How to reboot in SafeBoot mode from the app](#how-to-reboot-in-safeboot-mode-from-the-app).

Then add to your PlatformIO `platformio.ini` file:

```ini
board_build.partitions = partitions-4MB-safeboot.csv
upload_protocol = espota
; set OTA upload port to the ip-address when not using mDNS
upload_port = 192.168.125.99
; when mDNS is enabled, just point the upload to the hostname
; upload_port = MyAwesomeApp.local
custom_safeboot_restart_path = /api/system/safeboot
extra_scripts =
  tools/safeboot.py
```

The `safeboot.py` script can be downloaded from the release page: [https://github.com/mathieucarbou/MycilaSafeBoot/releases](https://github.com/mathieucarbou/MycilaSafeBoot/releases). The partition table `partitions-4MB-safeboot.csv` is found in the [example folder](https://github.com/mathieucarbou/MycilaSafeBoot/tree/main/examples/App_ESPConnect_OTA).

- `upload_protocol = espota` tells PlatformIO to use Arduino OTA to upload the firmware
- `upload_port` is the IP address or mDNS name of the ESP32
- `custom_safeboot_restart_path` is the path to call to restart the app in SafeBoot mode

Once done, just run a `pio run -t upload` or `pio run -t uploadfs` for example and you will see the app automatically restarting in SafeBoot mode, then upload will be achieved, then the ESP will be restarted with your new app.

See `examples/App_ESPConnect_OTA` for an example.
