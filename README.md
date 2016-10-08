# ESP8266 Momentary Standalone Relay Switch with MQTT

This is used to toggle a relay with a push button, using the EX-Store [WiFi-Relay Board](https://ex-store.de/ESP8266-WiFi-Relay-V31). The Switch will work if no WiFi or MQTT is present. I'm using it in combination with a coupling relay to replace a surge switch (Eltako S12-100).

It should work on any ESP8266 with a connected relay. The difference could be, that the EX-Store relay board is using a [L9110](http://www.elecrow.com/download/datasheet-l9110.pdf) motor driver which needs two GPIO's to be switched. To change this behaviour simply edit the `turnOn()` or `turnOff()` functions.

Connect the push button to `GND` and `GPIO14` to toggle the relay.

## MQTT
The current state (`1`/`0`) of the relay is published to `<topic-prefix>state`. The online status (`online`/`offline`) is published to `<topic-prefix>status`. Both topics are retained and the status will be set to `offline` using last will of MQTT.

To change the switch' state publish `1` or `0` to `<topic-prefix>do`.

## Config
Change the settings at the top of the .ino-file corresponding to your needs. Ensure that you choose a unique mqtt client id. I haven't tested this without authentication against the broker.

The serial console is left open to debug and check the WiFi connection. Baudrate 115200.

## Misc.
Bear with me as C/C++ is not my first language. Any suggestions and pull requests are welcome.
