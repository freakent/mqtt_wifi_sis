#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;

void initializeTemperature() {
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");
}

void processTemperature(){
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
  publishTemperature(temp.temperature, humidity.relative_humidity);
}


void publishTemperature(float temp, float humi) {
  Serial.println(); Serial.print("MQTT Connection status: "); Serial.print(mqttClient.connected());
  Serial.print(", WiFi status: "); Serial.println(WiFi.status());

  mqttClient.beginMessage("W"+temperatureTopic+"Temperature");
  mqttClient.print("{ \"value\":");
  mqttClient.print(temp);
  mqttClient.print("}");
  mqttClient.endMessage();    
  Serial.print("publish temperature ");
  Serial.print(temp);
  Serial.print(" to temperatureTopic: "); Serial.println("W"+temperatureTopic+"Temperature"); 

  mqttClient.beginMessage("W"+temperatureTopic+"Humidity");
  mqttClient.print("{ \"value\":");
  mqttClient.print(humi);
  mqttClient.print("}");
  mqttClient.endMessage();    
  Serial.print("publish humidity ");
  Serial.print(humi);
  Serial.print(" to temperatureTopic: "); Serial.println("W"+temperatureTopic+"Humidity"); 
  Serial.println();
}