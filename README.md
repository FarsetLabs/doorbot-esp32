# doorbot-esp32

RFID Card Reader and Door Relay Opening Module for use with the [doorbot](https://www.github.com/FarsetLabs/doorbot) project.

It's general operation is:

* Setup Wi-Fi Connection*
* Setup MFRC522 Card Connection
* Loop waiting for card presence detection
* On Card Detect, Grab the UID from the presented Card
* Attempt to trigger an 'open' action* on the door identified by the onboard device_id, using the same device id and the UID from the presented card as the Username and Password for HTTP Basic Auth
* If 200 response; flash green on RGB LED and unlock door via relay for 5 seconds
* Otherwise flash red for 5 seconds and resume loop

*Configured through build-time environment variables, see `platform.ini` for details

## Hardware Setup

This project uses an ESP-WROOM-32 package, an RC522 RFID module

![ESP32 Pinout](img/ESP-Pinout.jpg)

![RC522 Pinout](img/RC522-Pinout.jpg)

### Pin Mappings:

#### RC522 -> ESP32

| RC522 | ESP32   | Description       |
| ----- | ------- | ----------------- |
| 3v3   | 3v3     | VIN               |
| RST   | D22     | Reset             |
| GND   | GND     | Ground            |
| IRQ   | <Blank> | Unused            |
| MISO  | D19     | SPI Master Input  |
| MOSI  | D23     | SPI Master Output |
| SCK   | D18     | SPI Clock         |
| SDA   | D21     | Serial Data Line  |

#### RGB LED -> ESP32

| RGB LED | ESP32   | Description |
| ------- | ------- | ----------- |
| (-)     | GND     | Ground      |
| R       | D2      | Red         |
| G       | D4      | Green       |
| B       | <blank> | Blue        |

#### RELAY -> ESP32

| RELAY | ESP32 |                          |
| ----- | ----- | ------------------------ |
| NO    | D15   | Normally Open (Actuator) |
| +     | 3v3   | +VE                      |
| -     | GND   | Ground                   |







