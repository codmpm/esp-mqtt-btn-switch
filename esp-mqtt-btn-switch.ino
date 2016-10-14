// ESP8266 Momentary Standalone Relay Switch with MQTT
// 2016, Patrik Mayer - patrik.mayer@codm.de
//
// debounce from http://blog.erikthe.red/2015/08/02/esp8266-button-debounce/
// mqtt client from https://github.com/knolleary/pubsubclient/tree/master/examples/mqtt_esp8266

// system state on <mqttTopicPrefix>status
// current switch state on <mqttTopicPrefix>state
// send 1/0 to <mqttTopicPrefix>do to switch

#include <ESP8266WiFi.h>
#include <PubSubClient.h>


//--------- Configuration
// WiFi
const char* ssid = "<your-wifi-ssid>";
const char* password = "<your-wifi-key>";

const char* mqttServer = "<mqtt-broker-ip-or-host>";
const char* mqttUser = "<mqtt-user>";
const char* mqttPass = "<mqtt-password>";
const char* mqttClientName = "<mqtt-client-id>";
const char* mqttTopicPrefix = "<mqtt-topic-prefix>";

// I/O
const int   btnPin = 14; //IO14 on WiFi Relay
//---------


// internal vars
WiFiClient espClient;
PubSubClient client(espClient);

char mqttTopicState[64];
char mqttTopicStatus[64];
char mqttTopicDo[64];


long lastDebounceTime = 0; // Holds the last time debounce was evaluated (in millis).
const int debounceDelay = 50; // The delay threshold for debounce checking.

int onoff = false; //is relay on or off
int wantedState = false; //wanted state
int debounceState; //internal state for debouncing

void setup() {
  // Configure the pin mode as an input.
  pinMode(btnPin, INPUT_PULLUP);

  //output pins for L9110
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);

  //switch off relay
  digitalWrite(12, HIGH);
  digitalWrite(13, LOW);

  // Attach an interrupt to the pin, assign the onChange function as a handler and trigger on changes (LOW or HIGH).
  attachInterrupt(btnPin, onChangeButton, CHANGE);

  Serial.begin(115200);

  //put in mqtt prefix
  sprintf(mqttTopicState, "%sstate", mqttTopicPrefix);
  sprintf(mqttTopicStatus, "%sstatus", mqttTopicPrefix);
  sprintf(mqttTopicDo, "%sdo", mqttTopicPrefix);

  setup_wifi();
  client.setServer(mqttServer, 1883);
  client.setCallback(mqttCallback);

  Serial.println("ready...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (onoff != wantedState) {
    doOnOff();
  }

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


void setup_wifi() {

  delay(10);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA); //disbale AP mode, only station
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


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {

    Serial.print("Attempting MQTT connection...");

    // Attempt to connect with last will retained
    if (client.connect(mqttClientName, mqttUser, mqttPass, mqttTopicStatus, 1, true, "offline")) {
      Serial.println("connected");

      // Once connected, publish an announcement...
      client.publish(mqttTopicStatus, "online", true);
      client.publish(mqttTopicState, ((onoff) ? "1" : "0") , true);

      // ... and (re)subscribe
      client.subscribe(mqttTopicDo);

      Serial.print("subscribed to ");
      Serial.println(mqttTopicDo);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
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


