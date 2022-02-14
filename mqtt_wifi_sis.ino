/*
 This sketch connects to a Victron Energy GX device via MQTT
 and performs multiple duties:
   * Temperature and humidty sensor using AHT20 sensor
   * Remote mode switching of Multiplus inverter/charger
 
 It has been tested and deployed on Arduino Nano IOT 33.

 It has a dependency on the MQTT device service available here: https://github.com/freakent/dbus-mqtt-devices.

 Created January 2022 by Martin Jarvis (FreakEnt/Gone-sailing)

 To Do:
 1) Set the client ID using the WiFi MAC address
 2) Change payload of status message to allow for multiple services of the same type
 */
#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h" 

const char VERSION[] = "v1.0"; 
const boolean headless = true; //useful for debugging 

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const char ssid[] = SECRET_SSID;        // your network SSID (name)
const char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)
const char broker[] = MQTT_SERVER;      // "venus.local"; // "test.mosquitto.org";
int        port     = MQTT_SERVER_PORT;

const int buttonPin = 2;     // the number of the pushbutton pin
const int ledPin =  13;      // the number of the LED pin

const char clientId[] = "fe001"; // Do not include characters .-: only use underscores _
const char serviceId[] = "t1"; // any string to uniquely identify the service on this device

const char portalIDTopic[]  = "N/+/system/0/Serial"; // Topic to retrieve the VRM Portal ID for the Venus device
const char keepaliveTopicTemplate[] = "R/<portal ID>/keepalive"; // Topic to send keepalive request to
const char keepalivePayload[] = "[\"vebus/+/Mode\", \"temperature/+/#\"]"; // Choose which topics you want to keepalive (see dbus-mqtt)
const int  keepaliveInterval = 300000; // How often to publish the keep alive request (see dbus-mqtt)
const char deviceStatusTopicTemplate[] = "device/<client ID>/Status"; // Topic to send client status to and to regstister to the dbus-mqtt-device service
const char deviceInstanceTopicTemplate[] = "device/<client ID>/DeviceInstance"; // Topic to subscribe to so the dbus-mqtt-device service will provide an instance id to use when publishing to dbus-mqtt 
const char vebusModeTopicTemplate[] = "/<portal ID>/vebus/257/Mode"; // dbus-mqtt topic for handling the multiplus mode (inverter/charger onn/off), is prefixed with N, R or W at runtime
const char temperatureTopicTemplate[] = "/<portal ID>/temperature/<instance ID>/"; // dbus-mqtt topic for handling Temperator sensors, is prefixed with N or W at runtime
const int  tempReadingInterval = 120000; // How often to publish a temperature reading

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
int status = WL_IDLE_STATUS;     // the WiFi radio's status

String portalID = "";
int instanceID = -1;


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

      // Handle Keepalive (see Victron dbus-mqtt) 
      if (millis() - lastKeepaliveTime >= keepaliveInterval) {
        lastKeepaliveTime = millis();       // save the last time a keepalive message was sent
        publishKeepalive();
      }

      if (instanceID > 0) {

        // Handle temperatures
        if ((millis() - lastTempReadingTime >= tempReadingInterval) || (lastTempReadingTime == 0)) {
          byte mac[6];
          WiFi.macAddress(mac);
          Serial.print("MAC address: ");
          printMacAddress(mac);
          lastTempReadingTime = millis();
          processTemperature();
        }
      }
    }

    mqttClient.poll();
  }

}

/* * * * * * * * * * * * * * * * *
 *   o n M q t t M e s s a g e 
 * * * * * * * * * * * * * * * * */
void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message on topic [");
  Serial.print(mqttClient.messageTopic());
  Serial.print("], length ");
  Serial.print(messageSize);
  Serial.println(" bytes");

  String topic = String(mqttClient.messageTopic());
  
  // Handle VRM Portal ID messages from dbus-mqtt
  if (topic.endsWith("/system/0/Serial") && messageSize > 0) {  // VRM Portal Id
  
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

      // subscribe to a Mode topic on the Vebus
      mqttClient.subscribe(vebusModeTopic("N"), 2);
      Serial.print("subscribe vebusModeTopic: "); Serial.println(vebusModeTopic("N")); Serial.println();
      // send a read request to dbus-mqtt
      mqttClient.beginMessage(vebusModeTopic("R"));
      mqttClient.print("");
      mqttClient.endMessage();    
      Serial.print("publish Read Request 'R' to vebusModeTopic: "); Serial.println(vebusModeTopic("R")); Serial.println();

    }
    
    return;
  }

  // Handle DeviceInstance messages from dbus-mqtt-devices
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
    
    instanceID = doc[serviceId];
    Serial.print("DeviceInstance: "); Serial.println(instanceID); Serial.println();

    return;
  }

   // Unexpected message so use the Stream interface to print the contents
  while (mqttClient.available()) {
    Serial.print((char)mqttClient.read());
  }
  Serial.println();

  Serial.println();
}


