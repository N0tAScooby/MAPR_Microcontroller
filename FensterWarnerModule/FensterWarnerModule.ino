
#include <WiFiNINA.h>           // Wifi Client Library für Nano 33 IoT Board
#include <ArduinoMqttClient.h>  // MQTT Bibliothek, ggf. über Sketch / Include Librar / Manage Libraries nachinstallieren
// WiFi Zugangsdaten lagen - Das Netzwerk ist vorkonfiguriert
#include "wifi_secrets.h"
#include "ArduinoLowPower.h"

const char broker[] = "192.168.37.4";
const int port = 1883;
// Ein einfaches Topic ... nur eins zum Senden und Empfangen. Ist langweilig aber soll nur zeigen wie es geht
const char topic[] = "micob/Fensterwarner/Status";
// Digital Input PIN
#define D_PIN 10   //digitaler Input mit internem Pullup --> Fenster Offen = 1 und Fenster geschlossen = 0
#define D_PIN2 12  //Led, welche den Fensterzustand spiegelt
#define D_PIN3 11

// Arduino an sendet Status jede sekunde. Nach secondstillallert wird ALARM gesendet und alle 5 sek. wiederholt, LED bei Sender und Empfänger leuchet. Deepsleep nach 90 sek. --> Status LED am Arduino blinkt nicht mehr und USB-Verbindung getrennt. 

int secondsTillAlert = 30;
int secondsTillUnmute = 60;
int timeTillSleep = 90;
unsigned long cur_time = 0;

unsigned long timeSinceWakeup = 0;
unsigned long y = 0;
enum States { WINDOW_OPEN,
              WINDOW_CLOSED };
States states = WINDOW_OPEN;
bool MUTE_STATE;
unsigned long state_ticker;
int blinkState = 0;

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

void turnoffalarm() {
  digitalWrite(D_PIN3, 0);
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
  Serial.println("Mico-B Fensterwarner");



  //Initialize serial and wait for port to WINDOW_OPEN:
  // print some greedings with ending line break
  Serial.println("Digital read (count)");
  // input with pullup resistor
  pinMode(D_PIN, INPUT_PULLUP);
  pinMode(D_PIN2, OUTPUT);
  pinMode(D_PIN3, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  state_ticker = millis();

  // attacht interrupt
  LowPower.attachInterruptWakeup((D_PIN), stateChange, CHANGE);  //Interuppt zur Erkennung von "Change" --> Änderungen
}

volatile int count = 0;
int last_count = count + 1;  // initial state should be different so it gets executed after startup
bool wakeup = true;

void stateChange() {
  count++;  //Bei Änderung des States wird hochgezählt
  timeSinceWakeup = millis();
  //Serial.println("Aufwachen");
  wakeup = true;
}


void loop() {
  if (wakeup) {
    Serial.begin(9600);
    timeSinceWakeup = millis();
    wakeup = false;
    digitalWrite(LED_BUILTIN, 1);
  }
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
    timeSinceWakeup = millis();

    mqttClient.beginMessage("WINDOW_STATE");
    mqttClient.print("Hello There! I have been awoken!");
    mqttClient.endMessage();
  }

  mqttClient.poll();  // Call the loop continuously

  if (timeSinceWakeup + timeTillSleep * 1000 < millis() && !wakeup) {
    Serial.println("Tired. Going to sleep...zzzz");
    mqttClient.beginMessage("WINDOW_STATE");
    mqttClient.print("GoingToSleep");
    mqttClient.endMessage();
    Serial.println();
    digitalWrite(LED_BUILTIN, 0);
    LowPower.sleep();
  }

  if (count != last_count) {  //immer wenn hochgezählt wird, dann beginne die ms zu zählen und den Fensterstatus auszulesen
    y = millis();
    //Serial.println(count);
    last_count = count;
    int x = digitalRead(D_PIN);
    //Serial.print(x);
    digitalWrite(D_PIN2, x);  //Fensterstatus wird auch als Signal auf Pin 12 wieder ausgegeben --> StausLED
    if (x == 1) {
      Serial.println("switchtoopen");
      states = WINDOW_OPEN;
      mqttClient.beginMessage("WINDOW_STATE");
      mqttClient.print("OPEN");
      mqttClient.endMessage();
      //Serial.println();

    } else {
      states = WINDOW_CLOSED;
      MUTE_STATE = false;
      mqttClient.beginMessage("WINDOW_STATE");
      mqttClient.print("CLOSED");
      mqttClient.endMessage();
      Serial.println();
      Serial.println("switchtoclose");
    }
  }


  String msg = "";
  msg = receiveMQTTMessage();
  if (msg == "MUTE") {
    cur_time = millis();  //Zeitfenster startet, in welchem kein Alarm gesendet werden soll
    MUTE_STATE = true;
    Serial.println(msg);
  }



  switch (states) {
    case WINDOW_OPEN:
      if (y + 1000 * secondsTillAlert <= millis()) {
        y = y + 5000;
        //Serial.println((MUTE_STATE == false));
        //Serial.println((cur_time + 1000 * secondsTillUnmute <= millis()));
        if ((MUTE_STATE == false) || (cur_time + 1000 * secondsTillUnmute <= millis())) {  //Zeitfenster endet nach den hier definierten ms und Alarm wird nur gesendet wenn Fenster offen und! nicht gemutet oder mute-Zeit abgelaufen
          Serial.println("WINDOW_ALARM");
          mqttClient.beginMessage("WINDOW_ALARM");
          mqttClient.print("ALARM");
          mqttClient.endMessage();
          Serial.println();
          digitalWrite(D_PIN3, 1);
        } else {
          Serial.println("Muted");
          //Serial.println(MUTE_STATE);
          turnoffalarm();
        }
      }
      break;
    case WINDOW_CLOSED:
      turnoffalarm();
      break;
  }

  if (state_ticker + 1000 < millis()) {
    String cur_state = "";
    if (states == WINDOW_OPEN) {
      cur_state = "OPEN";
    } else {
      cur_state = "CLOSED";
    }
    blinkState = !blinkState;
    digitalWrite(LED_BUILTIN, blinkState);
    Serial.print(msg);

    Serial.println(cur_state);
    mqttClient.beginMessage("WINDOW_STATE");
    mqttClient.print(cur_state);
    mqttClient.endMessage();
    Serial.println();

    state_ticker = millis();
  }
}
