void initialiseMQTT() {
  
  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId(clientId);

  // You can provide a username and password for authentication
  // mqttClient.setUsernamePassword("username", "password");

  // Set last will and testament
  mqttClient.beginWill(deviceStatusTopic(), false, 1);
  serializeJson(deviceStatusPayload(0), mqttClient);
  mqttClient.endWill();

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);


  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);
  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.print(mqttClient.connectError());
    Serial.print(", Wifi status = ");
    Serial.println(WiFi.status());

    delay(30000);
    return;
  }
  Serial.println("You're connected to the MQTT broker!"); Serial.println();

  Serial.print("Subscribing to Portal ID topic: "); Serial.println(portalIDTopic);
  mqttClient.subscribe(portalIDTopic, 2);   // subscribe to the main Portal ID topic

  Serial.print("Subscribing to Device Instance topic: "); Serial.println(deviceInstanceTopic());
  mqttClient.subscribe(deviceInstanceTopic(), 2);   

  mqttClient.beginMessage(deviceStatusTopic(), false, 1);
  serializeJson(deviceStatusPayload(1), mqttClient);
  mqttClient.endMessage();    

  // topics can be unsubscribed using:
  // mqttClient.unsubscribe(topic);
}

/* * * * * * *
 *  
 * * * * * * */
void publishKeepalive() {
  if (portalID != "") {
    Serial.print("publish to keepaliveTopic: ");
    Serial.println(keepaliveTopic());
    mqttClient.beginMessage(keepaliveTopic());
    mqttClient.print(keepalivePayload);
    mqttClient.endMessage();
  }
}

String keepaliveTopic() {
  String topic =  String(keepaliveTopicTemplate);
  topic.replace("<portal ID>", portalID);
  return topic;
}

String deviceStatusTopic() {
  String topic =  String(deviceStatusTopicTemplate);
  topic.replace("<client ID>", clientId);
  return topic;
}

DynamicJsonDocument deviceStatusPayload(int connected) {
  DynamicJsonDocument doc(1024);
  doc["clientId"] = clientId;
  doc["connected"] = connected;
  doc["version"] = VERSION;
  JsonObject services = doc.createNestedObject("services");
  services[serviceId] = "temperature"; // Lowercase service type
  return doc;
}

String deviceInstanceTopic() {
  String topic =  String(deviceInstanceTopicTemplate);
  topic.replace("<client ID>", clientId);
  return topic;
}

String vebusModeTopic(String mode) {
  String topic =  String(vebusModeTopicTemplate);
  topic.replace("<portal ID>", portalID);
  return topic;
}

String temperatureTopic(String mode, String suffix) {
  String topic =  mode + String(temperatureTopicTemplate) + suffix;
  topic.replace("<portal ID>", portalID);
  topic.replace("<instance ID>", String(instanceID));
  return topic;
}


