# MycilaSafeBoot

[![GPLv3 license](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.txt)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaSafeBoot/actions/workflows/build.yml/badge.svg)](https://github.com/mathieucarbou/MycilaSafeBoot/actions/workflows/build.yml)

MycilaSafeBoot is a Web OTA recovery partition for ESP32 / Arduino.

It allows to have only one application partition to use the maximum available flash size.

The idea is not new: [Tasmota also uses a SafeBoot partition](https://tasmota.github.io/docs/Safeboot/).

- [Overview](#overview)
- [How it works](#how-it-works)
- [How to build the SafeBoot firmware image](#how-to-build-the-safeboot-firmware-image)
- [SafeBoot Example](#safeboot-example)
- [How to integrate the SafeBoot in your project](#how-to-integrate-the-safeboot-in-your-project)
- [How to reboot in SafeBoot mode from the app](#how-to-reboot-in-safeboot-mode-from-the-app)

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

Because of the need to have 2 partitions with the same size, the firmware is then limited to only 2MB in this case when the ESP has 4MB flash.
2MB is left unused (the OTA process will switch to the updated partition once completed).

**A SafeBoot partition is a small bootable recovery partition allowing you to use ElegantOTA to flash the firmware.**
Consequently, the firmware can take all the remaining space on the flash.

**The SafeBoot partition is 655360 bytes only.**

Example for 4MB partition with a SafeBoot partition and an application size of 3MB:

```
# Name,   Type, SubType,  Offset,   Size,    Flags
nvs,      data, nvs,      0x9000,   0x5000,
otadata,  data, ota,      0xE000,   0x2000,
safeboot, app,  factory,  0x10000,  0xA0000,
app,      app,  ota_0,    0xB0000,  0x330000,
fs,       data, spiffs,   0x3E0000, 0x10000,
coredump, data, coredump, 0x3F0000, 0x10000,
```

Example for 8Mb partition with a SafeBoot partition and an application size of 7MB:

```
# Name,   Type, SubType,  Offset,   Size,    Flags
nvs,      data, nvs,      0x9000,   0x5000,
otadata,  data, ota,      0xE000,   0x2000,
safeboot, app,  factory,  0x10000,  0xA0000,
app,      app,  ota_0,    0xB0000,  0x730000,
fs,       data, spiffs,   0x7E0000, 0x10000,
coredump, data, coredump, 0x7F0000, 0x10000,
```

The SafeBoot partition is also automatically booted wen the firmware is missing.

## How it works

1. When a user wants to update the app firmware, we have to tell the app to reboot in recovery mode.
2. Once booted in recovery mode, an Access Point is created with the SSID `SafeBoot`.
3. Connect to it, and upload the firmware as usual.
4. ElegantOTA will then flash the firmware to the `app` partition and reboot in it.

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
From there, you can access ElegantOTA to flash a new firmware, even from another application.

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

You can find in the [Project Releases](https://github.com/mathieucarbou/MycilaSafeBoot/releases) the list of available SafeBoot images, with the Python script to add to your build.

## How to reboot in SafeBoot mode from the app

You can use [MycilaSystem](https://github.com/mathieucarbou/MycilaSystem):

```cpp
#include <MycilaSystem.h>

Mycila::System.restartFactory("safeboot");
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
