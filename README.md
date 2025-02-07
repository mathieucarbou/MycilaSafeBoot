# MycilaSafeBoot

[![Latest Release](https://img.shields.io/github/release/mathieucarbou/MycilaSafeBoot.svg)](https://GitHub.com/mathieucarbou/MycilaSafeBoot/releases/)
[![Download](https://img.shields.io/badge/Download-safeboot-green.svg)](https://github.com/mathieucarbou/MycilaSafeBoot/releases)

[![GPLv3 license](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.txt)
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
- [License](#license)

[![](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ssid.jpeg)](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ssid.jpeg)
[![](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ota.jpeg)](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ota.jpeg)

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

**A SafeBoot partition is a small bootable recovery partition allowing you to use [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA) to flash the firmware.**
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

The SafeBoot partition is also automatically booted wen the firmware is missing.

## How it works

1. When a user wants to update the app firmware, we have to tell the app to reboot in recovery mode.

2. Once booted in recovery mode, an Access Point is created with the SSID `SafeBoot`.

[![](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ssid.jpeg)](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ssid.jpeg)

3. Connect to the Access Point.

4. Now, you can flash the new firmware, either with [ArduinoOTA](https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update) or with [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA) by going to `http://192.168.4.1`

5. After the flash is successful, the ESP will reboot in the new firmware.

[![](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ota.jpeg)](https://mathieu.carbou.me/MycilaSafeBoot/safeboot-ota.jpeg)

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
> SAFEBOOT_BOARD=esp32dev pio run -e safeboot
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

Flash this factory image on an ESP32:

```bash
esptool.py write_flash 0x0 .pio/build/esp32dev/firmware.factory.bin
```

Restart the ESP.
The app loads, shows a button to restart in SafeBoot mode.
After clicking on it, the ESP will reboot into SafeBoot mode.
From there, you can access [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA) to flash a new firmware, even from another application.

## How to reboot in SafeBoot mode from the app

You can use [MycilaSystem](https://github.com/mathieucarbou/MycilaSystem):

```cpp
#include <MycilaSystem.h>

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

## License

This project is GPL because the SafeBoot code depends on ElegantOTA, which is GPL.
But as a final project, you do not depend on the source code.
To use SafeBoot, your project depends on:

1. The python script `factory,py` which is MIT
2. The generated binary firmware image, either downloaded from the release section or built from the SafeBoot project.

This GPL-ed binary output can be used without modification in the final factory firmware.
The SafeBoot image is just added to the factory firmware image, so the GPL constrains do not apply.

Refs:

- https://www.gnu.org/licenses/gpl-faq.html#WhatCaseIsOutputGPL
- https://www.gnu.org/licenses/gpl-faq.html#Prelinking
