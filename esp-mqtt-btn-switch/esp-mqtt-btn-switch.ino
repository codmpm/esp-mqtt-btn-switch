// ESP8266 Momentary Standalone Relay Switch with MQTT
// 2017, Patrik Mayer - patrik.mayer@codm.de
//
// debounce from http://blog.erikthe.red/2015/08/02/esp8266-button-debounce/
// mqtt client from https://github.com/knolleary/pubsubclient/tree/master/examples/mqtt_esp8266
// Arduino OTA from https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA
// DHT from https://github.com/adafruit/Adafruit_DHT_Unified

// system state on <mqttTopicPrefix>status (retained)
// system ip on <mqttTopicPrefix>ip (retained)
// current switch state on <mqttTopicPrefix>state
// send 1/0 to <mqttTopicPrefix>do to switch (retained)

// if using DHT:
// temperature on <mqttTopicPrefix>temperate (retained)
// humidity on <mqttTopicPrefix>humidity (retained)

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

//install Adafruits's DHT_Unified and DHT
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

//--------- Configuration
// WiFi
const char* ssid = "<your-wifi-ssid>";
const char* password = "<your-wifi-key>";

const char* mqttServer = "<mqtt-broker-ip-or-host>";
const char* mqttUser = "<mqtt-user>";
const char* mqttPass = "<mqtt-password>";
const char* mqttClientName = "<mqtt-client-id>"; //will also be used hostname and OTA name
const char* mqttTopicPrefix = "<mqtt-topic-prefix>";


//#define USE_DHT //comment in if DHT should be used
#define DHTTYPE DHT22 //DHT11, DHT21, DHT22

// I/O
const int btnPin = 14; //IO14 on WiFi Relay

#ifdef USE_DHT
const int   dhtPin = 5;  //IO04 on WiFi Relay
const int dhtInterval = 60000; //millis
//---------

char mqttTopicTemp[64];
char mqttTopicHum[64];

long lastDHTTime = 0;
#endif

// internal vars
WiFiClient espClient;
PubSubClient client(espClient);

char mqttTopicState[64];
char mqttTopicStatus[64];
char mqttTopicDo[64];
char mqttTopicIp[64];

long lastReconnectAttempt = 0; //For the non blocking mqtt reconnect (in millis)
long lastDebounceTime = 0; // Holds the last time debounce was evaluated (in millis).
const int debounceDelay = 80; // The delay threshold for debounce checking.

int onoff = false; //is relay on or off
int wantedState = false; //wanted state
int debounceState; //internal state for debouncing

#ifdef USE_DHT
DHT_Unified dht(dhtPin, DHTTYPE);
#endif

void setup() {
  // Configure the pin mode as an input.
  pinMode(btnPin, INPUT_PULLUP);

  //output pins for L9110
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);

  //switch off relay
  digitalWrite(12, HIGH);
  digitalWrite(13, LOW);

  Serial.begin(115200);

  // Attach an interrupt to the pin, assign the onChange function as a handler and trigger on changes (LOW or HIGH).
  attachInterrupt(btnPin, onChangeButton, CHANGE);

#ifdef USE_DHT
  //initialize DHT
  dht.begin();
#endif

  //put in mqtt prefix
  sprintf(mqttTopicState, "%sstate", mqttTopicPrefix);
  sprintf(mqttTopicStatus, "%sstatus", mqttTopicPrefix);
  sprintf(mqttTopicDo, "%sdo", mqttTopicPrefix);
  sprintf(mqttTopicIp, "%sip", mqttTopicPrefix);

#ifdef USE_DHT
  sprintf(mqttTopicTemp, "%stemperature", mqttTopicPrefix);
  sprintf(mqttTopicHum, "%shumidity", mqttTopicPrefix);
#endif

  setup_wifi();
  client.setServer(mqttServer, 1883);
  client.setCallback(mqttCallback);



  //----------- OTA
  ArduinoOTA.setHostname(mqttClientName);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    delay(1000);
    ESP.restart();
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.println("ready...");
}

void loop() {

  //handle mqtt connection, non-blocking
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (MqttReconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
  client.loop();

  //handle state change
  if (onoff != wantedState) {
    doOnOff();
  }

#ifdef USE_DHT
  //check DHT
  checkDHT();
#endif

  //handle OTA
  ArduinoOTA.handle();

}



// Gets called by the interrupt.
void onChangeButton() {

  int reading = digitalRead(btnPin); // Get the pin reading.
  if (reading == debounceState) return; // Ignore dupe readings.

  boolean debounce = false;

  // Check to see if the change is within a debounce delay threshold.
  if ((millis() - lastDebounceTime) <= debounceDelay) {
    debounce = true;
  }

  // This update to the last debounce check is necessary regardless of debounce state.
  lastDebounceTime = millis();

  if (debounce) return; // Ignore reads within a debounce delay threshold.
  debounceState = reading; // All is good, persist the reading as the state.

  if (reading) {
    wantedState = !wantedState;
  }

}

void doOnOff() {
  onoff = !onoff;
  Serial.println("new relay state: " + String(onoff));

  if (onoff) {
    turnOn();
  } else {
    turnOff();
  }
}


void turnOn() {
  //12 0, 13 1
  digitalWrite(12, LOW);
  digitalWrite(13, HIGH);

  client.publish(mqttTopicState, "1", true);

}

void turnOff() {
  //12 1, 13 0
  digitalWrite(12, HIGH);
  digitalWrite(13, LOW);

  client.publish(mqttTopicState, "0", true);
}

#ifdef USE_DHT
void checkDHT() {

  if (millis() - lastDHTTime > dhtInterval) {

    sensors_event_t event;
    char dhtBuf[8];

    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println("Error reading temperature!");
    } else {
      sprintf(dhtBuf, "%.2f", event.temperature);
      client.publish(mqttTopicTemp, dhtBuf, true);

      Serial.print("Temperature: ");
      Serial.print(dhtBuf);
      Serial.println(" °C");

    }

    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println("Error reading humidity!");
    } else {
      sprintf(dhtBuf, "%.2f", event.temperature);
      client.publish(mqttTopicHum, dhtBuf, true);

      Serial.print("Humidity: ");
      Serial.print(dhtBuf);
      Serial.println("%");
    }

    lastDHTTime = millis();
  }

}
#endif

void setup_wifi() {

  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA); //disable AP mode, only station
  WiFi.hostname(mqttClientName);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


bool MqttReconnect() {

  if (!client.connected()) {

    Serial.print("Attempting MQTT connection...");

    // Attempt to connect with last will retained
    if (client.connect(mqttClientName, mqttUser, mqttPass, mqttTopicStatus, 1, true, "offline")) {

      Serial.println("connected");

      // Once connected, publish an announcement...
      char curIp[16];
      sprintf(curIp, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

      client.publish(mqttTopicStatus, "online", true);
      client.publish(mqttTopicState, ((onoff) ? "1" : "0") , true);
      client.publish(mqttTopicIp, curIp, true);

      // ... and (re)subscribe
      client.subscribe(mqttTopicDo);
      Serial.print("subscribed to ");
      Serial.println(mqttTopicDo);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
  }
  return client.connected();
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((char)payload[0] == '1') {
    wantedState = true;
  } else {
    wantedState = false;
  }

}


