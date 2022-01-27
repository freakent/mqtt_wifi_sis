String deviceStatusTopic() {
  String topic =  String(deviceStatusTopicTemplate);
  topic.replace("<client ID>", clientId);
  return topic;
}

DynamicJsonDocument deviceStatusPayload(int connected) {
  DynamicJsonDocument doc(1024);
  doc["clientId"] = clientId;
  doc["connected"] = connected;
  JsonArray services = doc.createNestedArray("services");
  services.add("Temperature");
  return doc;
}

String deviceInstanceTopic() {
  String topic =  String(deviceInstanceTopicTemplate);
  topic.replace("<client ID>", clientId);
  return topic;
}

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
  Serial.print("Subscribing to Decice Instance topic: "); Serial.println(deviceInstanceTopic());
  mqttClient.subscribe(deviceInstanceTopic(), 2);   

  mqttClient.beginMessage(deviceStatusTopic(), false, 1);
  serializeJson(deviceStatusPayload(1), mqttClient);
  mqttClient.endMessage();    

  Serial.println("You're connected to the MQTT broker!"); Serial.println();

  Serial.print("Subscribing to Portal ID topic: "); Serial.println(portalIDTopic);
  mqttClient.subscribe(portalIDTopic, 2);   // subscribe to the main Portal ID topic

  // topics can be unsubscribed using:
  // mqttClient.unsubscribe(topic);
}


