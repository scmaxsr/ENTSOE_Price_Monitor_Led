# ENTSO-E Price Monitor

ESP8266 firmware for a Wemos D1 Mini that shows rolling ENTSO-E day-ahead electricity prices on an 8x8 NeoMatrix LED display and exposes a local web dashboard.

This repository contains only the source files from the firmware `src` directory.

## Hardware

- Wemos D1 Mini or compatible ESP8266 board
- 8x8 NeoPixel/NeoMatrix LED matrix
- USB cable for flashing and serial monitor

## Firmware Stack

- PlatformIO
- Arduino framework for ESP8266
- Adafruit GFX Library
- Adafruit NeoMatrix
- Adafruit NeoPixel

Reference PlatformIO environment:

```ini
[env:esp8266]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200
upload_speed = 115200
lib_deps =
    Adafruit GFX Library
    Adafruit NeoMatrix
    Adafruit NeoPixel
```

## Features

- WiFi configuration portal on first boot or missing configuration
- ENTSO-E Transparency Platform day-ahead price fetch
- Rolling 8-hour graph for the dashboard and LED matrix
- Europe/Amsterdam local time handling, including CET/CEST behavior
- Web dashboard at `/dashboard`
- JSON price API at `/api/prices`
- Settings page at `/settings`
- OTA update page at `/update`
- Factory reset endpoint and page

## Price Handling

The firmware keeps a 24-hour internal price buffer and exposes the next 8 rolling hours for display.

Primary source:

- Official ENTSO-E Transparency Platform XML API

Fallback source:

- `http://spot.utilitarian.io/electricity/NL/YYYY/MM/DD/`

The fallback is used when the official ENTSO-E response does not fill the rolling display window. PT15M data from the fallback is averaged into hourly values. Display values are shown as euro/kWh, for example `0.067 euro`.

## Configuration

Default values are defined in `settings.h`:

- WiFi SSID and password placeholders
- ENTSO-E API key placeholder
- Bidding zone: `10YNL----------L`
- Timezone: `Europe/Amsterdam`

The runtime configuration can be changed through the web settings page after the device is online.

## Web Endpoints

- `/dashboard` - live dashboard with graph and summary
- `/settings` - edit WiFi, ENTSO-E API key, bidding zone and timezone
- `/api/prices` - JSON price data, summary and debug fields
- `/api/config` - read or update configuration
- `/api/reset` - factory reset
- `/scan` - WiFi network scan
- `/update` - OTA firmware upload

The dashboard and API are protected with basic authentication in the web helper.

## OTA Firmware Versions

Compiled ESP8266 OTA firmware images are versioned with Git tags.

Current firmware tag:

```text
v1.3.2
```

Download the tagged source or open the tag in GitHub to use the firmware image from that version. Upload the `.bin` file on the device OTA update page:

```text
http://<device-ip>/update
```

Firmware builds are made for the `d1_mini` PlatformIO environment shown above.

## Using These Sources

Create a normal PlatformIO ESP8266 project and place these files in its `src` directory. Add the dependencies listed above to `platformio.ini`, then build and upload:

```powershell
pio run
pio run --target upload
```

After flashing, connect to the configuration portal if needed, enter WiFi and ENTSO-E settings, then open:

```text
http://<device-ip>/dashboard
```

## Notes

- The firmware is written for memory-constrained ESP8266 hardware.
- Price parsing avoids large JSON/XML dependencies where possible.
- The local dashboard displays only the rolling 8-hour window, while the firmware keeps more source data internally for reliable selection.
