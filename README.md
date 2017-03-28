# ESP8266 Momentary Standalone Relay Switch with MQTT

This is used to toggle a relay with a push button, using the EX-Store [WiFi-Relay Board](https://ex-store.de/ESP8266-WiFi-Relay-V31). The Switch will work if no WiFi or MQTT is present. I'm using it in combination with a coupling relay to replace a surge switch (Eltako S12-100).

It should work on any ESP8266 with a connected relay. The difference will be, that the EX-Store relay board is using a [L9110](http://www.elecrow.com/download/datasheet-l9110.pdf) motor driver which needs two GPIO's to be switched. To change this behaviour simply edit the `turnOn()` or `turnOff()` functions.

Connect the push button to `GND` and `IO14` to toggle the relay. The internal Pull-Up is used.

After first flashing this sketch, you could update the ESP via [ArduinoOTA](http://esp8266.github.io/Arduino/versions/2.3.0/doc/ota_updates/readme.html). The OTA name is set to the `mqttClienName` which is also used for the ESP-hostname. After an OTA update the ESP gets restarted.

Starting with `0.0.3` you can attach a DHT temperature/humidity sensor to `IO04` (configurable). Please comment in the `USE_DHT` define to use this feature.

## MQTT
* The current state (`1`/`0`) of the relay is published to `<topic-prefix>state`. 
* The online status (`online`/`offline`) is published to `<topic-prefix>status`. 
* The IP of the device is published to `<topic-prefix>ip`.

These topics are retained and the status will be set to `offline` using the last will of MQTT. The mqtt connection is now non-blocking (finally)

To change the switch' state, publish `1` or `0` to `<topic-prefix>do`.

The temperature and humidity values (if used) will be read every 60 seconds (configurable), retained and put to this topics:

* `<topic-prefix>temperature`
* `<topic-prefix>humidity`


## Config
Change the settings at the top of the .ino-file corresponding to your needs. Ensure that you choose a unique mqtt client id. If you do not need credentials just set user and password to an empty string.

The serial console is left open to debug and check the WiFi connection. Baudrate 115200, 8N1.

## Problems
If you get `error: 'class ArduinoOTAClass' has no member named 'getCommand'` comment out the complete `ArduinoOTA.onStart` method. It only puts the OTA-Type to the serial console when OTA is happening, so not realy needed.

Also you can update `ArduinoOTA.cpp` and `ArduinoOTA.h` to the latest master from https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA


## Created with
- Arduino 1.8.1 (https://www.arduino.cc/)
- ESP8266 board definition 2.3.0 (https://github.com/esp8266/Arduino)
- PubSubClient 2.6.0 by Nick O'Leary (https://github.com/knolleary/pubsubclient)
- Arduino OTA 2.3.0 http://esp8266.github.io/Arduino/versions/2.3.0/doc/ota_updates/readme.html
- Adafruit Unified Sensor library https://github.com/adafruit/Adafruit_DHT_Unified
- Adafruit DHT library https://github.com/adafruit/DHT-sensor-library

## Misc.
Bear with me as C/C++ is not my first language. Any suggestions and pull requests are welcome.
