
#include <WiFiNINA.h>           // Wifi Client Library für Nano 33 IoT Board
#include <ArduinoMqttClient.h>  // MQTT Bibliothek, ggf. über Sketch / Include Librar / Manage Libraries nachinstallieren
// WiFi Zugangsdaten lagen - Das Mico-B Kurs Netzwerk ist vorkonfiguriert
#include "wifi_secrets.h"

const char broker[] = "192.168.37.4";
const int port = 1883;
// Ein einfaches Topic ... nur eins zum Senden und Empfangen. Ist langweilig aber soll nur zeigen wie es geht
const char topic[] = "micob/Fensterwarner/Status";
// Digital Input PIN
#define D_PIN 13
#define D_PIN2 12
unsigned long y = 0;
enum States { WINDOW_OPEN,
              WINDOW_CLOSED };
States states = WINDOW_OPEN;

unsigned long state_ticker;


WiFiClient client;              // Wifi Client
MqttClient mqttClient(client);  // MQTT Client, an Wifi gebunden

unsigned long lastConnectionTime = 0;               // Stand millies Zähler bei letztem Update
const unsigned long postingInterval = 10L * 1000L;  // Post data every 10 seconds.

// Message callback Funktion
void onMqttMessage(int /*messageSize*/) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());

  Serial.print("', value = ");
  const float zahl = mqttClient.parseFloat();
  Serial.println(zahl);
}

String receiveMQTTMessage() {
  int messageSize = mqttClient.parseMessage();
  String message = "";
  if (messageSize > 0) {
    // we received a message, collect the contents
    String state_topic = mqttClient.messageTopic();
    Serial.print("\nReceived a message with state_topic '" + state_topic + "', length " + String(messageSize) + " bytes:\n");

    while (mqttClient.available()) {
      message += (char)mqttClient.read();
    }
    Serial.println(message);
  }
  return message;  // Return the message content
}

void setup() {
  delay(200);  // Manchmal hängen die Nano IoT Boards im Startup fest und lassen sich nicht mehr ansprechen
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Mico-B Beispiel Projekt");



  //Initialize serial and wait for port to WINDOW_OPEN:
  // print some greedings with ending line break
  Serial.println("Digital read (count)");
  // input with pullup resistor
  pinMode(D_PIN, INPUT_PULLUP);
  pinMode(D_PIN2, OUTPUT);

  state_ticker = millis();

  // attacht interrupt
  attachInterrupt(digitalPinToInterrupt(D_PIN), stateChange, CHANGE);
}

volatile int count = 0;
int last_count = count;

void stateChange() {
  count++;
}


void loop() {
  // Reconnect to WiFi if not connected
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Try WiFi connection ...");
    WiFi.begin(SECRET_SSID, SECRET_PASS);  // Connect to WPA/WPA2 Wi-Fi network.
    delay(4000);
  }
  // Reconnect if MQTT client is not connected.
  while (!mqttClient.connected()) {
    Serial.println("Try MQTT connection ... ");
    mqttClient.connect(broker, port);
    delay(1000);
    // subscribe to a topic after connection
    mqttClient.subscribe(topic);
    mqttClient.subscribe("WINDOW_ALARM");
  }

  mqttClient.poll();  // Call the loop continuously

  bool MUTE_STATE;
  // send message, the Print interface can be used to set the message contents

  if (count != last_count) {
    y = millis();
    Serial.println(count);
    last_count = count;
    int x = digitalRead(D_PIN);
    Serial.print(x);
    digitalWrite(D_PIN2, !x);
    if (x == 1) {
      Serial.println("switchtoopen");
      states = WINDOW_OPEN;
      mqttClient.beginMessage("WINDOW_STATE");
      mqttClient.print("OPEN");
      mqttClient.endMessage();
      Serial.println();

    } else {
      states = WINDOW_CLOSED;
      MUTE_STATE=false;
      mqttClient.beginMessage("WINDOW_STATE");
      mqttClient.print("CLOSED");
      mqttClient.endMessage();
      Serial.println();
      Serial.println("switchtoclose");
    }
  }

    String msg = "";

    unsigned long cur_time = 0;
    msg = receiveMQTTMessage();
    if (msg == "MUTE") {
      cur_time = millis();
      MUTE_STATE = true;
      Serial.println(msg);
    }



    switch (states) {
      case WINDOW_OPEN:
        if (y + 10000 <= millis()) {
          y = y + 5000;
          if ((MUTE_STATE == false) || (cur_time + 10000 * 5 <= millis())) {
            Serial.println("WINDOW_ALARM");
            mqttClient.beginMessage("WINDOW_ALARM");
            mqttClient.print("ALARM");
            mqttClient.endMessage();
            Serial.println();
          }else{
            Serial.print("Muted ");
            Serial.println(MUTE_STATE);
          }
        }
        break;
      case WINDOW_CLOSED:
        break;
    }

    if (state_ticker + 1000 < millis()) {
      String cur_state = "";
      if (states == WINDOW_OPEN) {
        cur_state = "OPEN";
      } else {
        cur_state = "CLOSED";
      }

      Serial.print(msg);

      Serial.println(cur_state);
      mqttClient.beginMessage("WINDOW_STATE");
      mqttClient.print(cur_state);
      mqttClient.endMessage();
      Serial.println();

      state_ticker = millis();
    }
  }
