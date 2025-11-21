#include <ArduinoMqttClient.h>
#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
#include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
#include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_GIGA) || defined(ARDUINO_OPTA)
#include <WiFi.h>
#elif defined(ARDUINO_PORTENTA_C33)
#include <WiFiC3.h>
#elif defined(ARDUINO_UNOR4_WIFI)
#include <WiFiS3.h>
#endif

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

#define LED_PIN 10
#define BUZZER_PIN 9
#define BUTTON_PIN 2

#define DEBUG_MODE false

const char state_topic[] = "WINDOW_STATE";
char alarm_topic[] = "WINDOW_ALARM";
const String W_OPEN = "OPEN";
const String W_CLOSED = "CLOSED";
const String W_ALARM = "ALARM";

String lastReceivedMessage = "";

bool inAlarmMode = false;

enum States { WINDOW_OPEN,
              WINDOW_CLOSED };
States state;

unsigned long second_ticker;
unsigned long buttonLastPressed = 0;

States prev_state = WINDOW_OPEN;

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate
//    flashed in the WiFi module.

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "192.168.37.4";
int port = 1883;

const long interval = 1000;

int count = 0;

void setup() {
  second_ticker = millis();


  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(10000);
  }

  Serial.println("You're connected to the network");
  Serial.println();

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  // mqttClient.setId("clientId");

  // You can provide a username and password for authentication
  // mqttClient.setUsernamePassword("username", "password");

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1)
      ;
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  Serial.println("Subscribing to topics...");
  mqttClient.subscribe(state_topic);
  mqttClient.subscribe(alarm_topic);

  Serial.println("Waiting for messages on topics... ");
  Serial.println();

  //set initial state
  state = WINDOW_CLOSED;
}


void sendMQTTMessage(char* state_topic, char* msg) {
  Serial.print("Sending message to topic: ");
  Serial.println(state_topic);
  Serial.print(" ");
  Serial.print(msg);

  mqttClient.beginMessage(state_topic);
  mqttClient.print(msg);
  mqttClient.endMessage();

  Serial.println();
}

String receiveMQTTMessage() {
  int messageSize = mqttClient.parseMessage();
  String message = "";
  if (messageSize > 0) {
    String msg_topic = mqttClient.messageTopic();
    //Serial.print("\nReceived a message with state_topic '" + msg_topic + "', length " + String(messageSize) + " bytes:\n");

    while (mqttClient.available()) {
      message += (char)mqttClient.read();
    }
    //Serial.println(message);
  }
  return message;
}

int checkButtonPressed() {
  return digitalRead(BUTTON_PIN);
}

void toggleLED(bool state) {
  if (state == false) {
    analogWrite(LED_PIN, 0);
  } else {
    analogWrite(LED_PIN, 255);
  }
}

void soundAlert() {
  Serial.println("Sounding Alert");
  analogWrite(BUZZER_PIN, 255);
}

void turnOffAlert() {
  analogWrite(BUZZER_PIN, 0);
}

void loop() {
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  String msg = "";
  msg = receiveMQTTMessage();


  if (msg.length() > 0) {
    if (msg != lastReceivedMessage) {
      Serial.println();
      Serial.println(msg);
      Serial.println();
    }
    if (msg == W_OPEN) {
      state = WINDOW_OPEN;
    } else if (msg == W_CLOSED) {
      turnOffAlert();
      state = WINDOW_CLOSED;
    } else if (msg == W_ALARM) {
      inAlarmMode = true;
    }
    lastReceivedMessage = msg;
  } else {
    if (second_ticker + 2000 < millis()) {
      // print
      Serial.print(".");
      second_ticker = millis();
    }
  }

  switch (state) {
    case (WINDOW_OPEN):
      {
        toggleLED(true);
        if (inAlarmMode) {
          soundAlert();
          inAlarmMode = false;
        }

        int pressed = checkButtonPressed();
        //Serial.print("BTN PRESSED ");
        //Serial.println(pressed);
        if (pressed == 0 && buttonLastPressed + 350 < millis()) {
          sendMQTTMessage(alarm_topic, "MUTE");
          buttonLastPressed = millis();
        }
        break;
      }
    case (WINDOW_CLOSED):
      {
        inAlarmMode = false;
        toggleLED(false);
        break;
      }
  }
  if (DEBUG_MODE == true) {
    if ((second_ticker + 1000 < millis()) || (state != prev_state)) {
      Serial.print("State: ");
      Serial.println(state);
      second_ticker = millis();
      prev_state = state;
    }
  }
}
