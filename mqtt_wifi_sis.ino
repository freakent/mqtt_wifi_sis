/*
 This example connects to an unencrypted WiFi network.
 Then it prints the MAC address of the WiFi module,
 the IP address obtained, and other network details.

 created 13 July 2010
 by dlf (Metodo2 srl)
 modified 31 May 2012
 by Tom Igoe
 */
#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h" 

const boolean headless = true;

// constants won't change. They're used here to set pin numbers:
const int buttonPin = 2;     // the number of the pushbutton pin
const int ledPin =  13;      // the number of the LED pin

const char broker[] = "192.168.1.139"; // "venus.local"; // "test.mosquitto.org";
int        port     = 1883;
const char portalIDTopic[]  = "N/+/system/0/Serial"; // "arduino/simple";
const char clientId[] = "arduino-freakent";
int deviceInstance;

String keepaliveTopic = "R/<portal ID>/keepalive";
String keepalivePayload = "[\"vebus/+/Mode\", \"temperature/+/#\"]";
const char deviceStatusTopicTemplate[] = "device/<client ID>/Status";
const char deviceInstanceTopicTemplate[] = "device/<client ID>/DeviceInstance";
const int keepaliveInterval = 300000; //45000;

String vebusModeTopic = "/<portal ID>/vebus/257/Mode";
String temperatureTopic = "/<portal ID>/temperature/300/";
const int tempReadingInterval = 120000;

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the WiFi radio's status

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

String portalID = "";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastKeepaliveTime = 0; // the last time a keepalive message was sent
unsigned long lastTempReadingTime = 0; // the last time a keepalive message was sent
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

int buttonState;         // variable for reading the pushbutton status
int buttonReading;
int lastButtonReading = LOW;

/* * * * * * *
 * S E T U P 
 * * * * * * */
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial && !headless) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Started.");

  initialiseWifi();

  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);

  initializeTemperature();
}

/* * * * * * *
 * L O O P 
 * * * * * * */
void loop() {

  checkButton();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("WiFi is not connected is lost, will attempt to reconnect in 30 seconds. WiFi status:"); Serial.println(WiFi.status());
    delay(30000);
    initialiseWifi();
  }

  if (!mqttClient.connected()) {
    initialiseMQTT();
  } else {

    if (portalID != "") {
      if (millis() - lastKeepaliveTime >= keepaliveInterval) {
        lastKeepaliveTime = millis();       // save the last time a message was sent
        publishKeepalive();
      }

      if ((millis() - lastTempReadingTime >= tempReadingInterval) || (lastTempReadingTime == 0)) {
        byte mac[6];
        WiFi.macAddress(mac);
        Serial.print("MAC address: ");
        printMacAddress(mac);
        lastTempReadingTime = millis();
        processTemperature();
      }
    }

    mqttClient.poll();
  }
  // check the network connection once every 10 seconds:
  //delay(10000);
  //printCurrentNet();
  
  //delay(500);
}

/* * * * * * * * * * * * * * *
 *   o n M q t t M e s s a g e 
 * * * * * * * * * * * * * * */
void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message on topic [");
  Serial.print(mqttClient.messageTopic());
  Serial.print("], length ");
  Serial.print(messageSize);
  Serial.println(" bytes");

  String topic = String(mqttClient.messageTopic());
  
  if (topic.endsWith("/system/0/Serial") && messageSize > 0) {
  
    DynamicJsonDocument doc(messageSize+10); // {"value": "dca6328d3ea7"} see https://arduinojson.org/v6/assistant/
    DeserializationError err = deserializeJson(doc, mqttClient);
    if (err) {
      Serial.print("Error reading Portal ID from JSON; ");
      Serial.println(err.c_str());
      Serial.println();
      return;
    }
    doc.shrinkToFit();
    
    String newportalID = doc["value"].as<String>();
    if (portalID != newportalID) {
      portalID = newportalID;
      Serial.print("portalID: "); Serial.println(portalID); Serial.println();

      // subscribe to a topic
      vebusModeTopic.replace("<portal ID>", portalID);
      mqttClient.subscribe("N"+vebusModeTopic, 2);
      Serial.print("subscribe vebusModeTopic: "); Serial.println("N"+vebusModeTopic); Serial.println();

      mqttClient.beginMessage("R"+vebusModeTopic);
      mqttClient.print("");
      mqttClient.endMessage();    
      Serial.print("publish Read Request 'R' to vebusModeTopic: "); Serial.println("R"+vebusModeTopic); Serial.println();

      // Force keepalive
      keepaliveTopic.replace("<portal ID>", portalID);
      publishKeepalive();

      temperatureTopic.replace("<portal ID>", portalID);
  //  } else {
  //    Serial.println("Ignoring duplicate portal id");
    }
    
    return;
  }

  if (topic == deviceInstanceTopic() && messageSize > 0) {
    DynamicJsonDocument doc(messageSize+10); // {"value": 100} see https://arduinojson.org/v6/assistant/
    DeserializationError err = deserializeJson(doc, mqttClient);
    if (err) {
      Serial.print("Error reading device instance JSON; ");
      Serial.println(err.c_str());
      Serial.println();
      return;
    }
    doc.shrinkToFit();
    
    deviceInstance = doc["Temperature"];
    Serial.print("DeviceInstance: "); Serial.println(deviceInstance); Serial.println();

    return;
  }

   // use the Stream interface to print the contents
  while (mqttClient.available()) {
    Serial.print((char)mqttClient.read());
  }
  Serial.println();

  Serial.println();
}

/* * * * * * *
 *  
 * * * * * * */
void publishKeepalive() {
  if (portalID != "") {
    Serial.print("publish to keepaliveTopic: ");
    Serial.println(keepaliveTopic);
    mqttClient.beginMessage(keepaliveTopic);
    mqttClient.print(keepalivePayload);
    mqttClient.endMessage();
  }
}

