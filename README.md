# BREmote V2 - Open Source eFoil and Esk8 remote 
![Banner](https://github.com/Luddi96/BREmote-V2/blob/main/img/banner.png)

## Features:
* All mechanical parts 3D printed (even the springs)
* Symmetric design
* Sustainable - All external parts can be replaced
* Open Source: 3D Models, Electronics and Software are GPL3.0
* 868/915MHz Link (range dependant on antenna type, position)
* Communication with VESC via UART / CAN
* Low battery alarm (vibration)
* Water ingress alarm integrated in receiver
* Gears / Power Levels / Cruise Control
* Charging and Programming via USB

## Links:
* [Build Video](https://youtu.be/Fw4YdQWvs6I)
* [SW Setup & Config Video](https://youtu.be/r6JIZEq3aTU)
* [Config Tool](https://lbre.de/BREmote/struct.html)
* [Serial Terminal](https://lbre.de/BREmote/sertest.html)
* [Expo Tool](https://lbre.de/BREmote/expo.html)
* [Flash Download Tool](https://dl.espressif.com/public/flash_download_tool.zip)


## Usage:


## Status/Error Codes:
Tx:
* Ex : Remote Errors
* EP: Not paired
* EC: Not Cald
* ESV: Error Config version SPIFFS <> Build
* ESP3: Error init SPIFFS
* ESP4: Error writing SPIFFS
* EHFC: Error HF (LoRa) setting
* EHFI: Error HF (LoRa) init
* EHFP: Error transmit power
* ECH: Error charger

Rx:
* Aux blink:
	* 3x: Error init SPIFFS
	* 2x: Error Config version SPIFFS <> Build
	* 4x: Error writing SPIFFS

* Bind blink:
	* Short Periodic Flash: Not paired
	* Blink: Paired not connected
	* Solid: Connected
	
	* 2: Error transmit power
	* 3: Error HF (LoRa) setting
	* 4: Error HF (LoRa) init

## Inputs at startup:
Tx:
* Left: Calibrate
* Right: Pair
* THR+Left: USB Mode
* THR+Right: Delete Spiffs

Rx:
* Bind pushed: Pair
* Both pushed: Delete config


# Connection Examples:



# Changelog:
### 2025-05-26:
Updated Complete.step (spring assy. missing)
Added RF settings for US/AU(915MHz) in Tx&Rx
### 2025-05-20: [Release V2.1]
Initial release


---
# Credits:
Logo uses "watersport" and "Skate" by Adrien Coquet from https://thenounproject.com/. CC BY 3.0.
