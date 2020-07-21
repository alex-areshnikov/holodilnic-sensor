#include "DHTesp.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <Config.h>

DHTesp dhtTop;
DHTesp dhtBottom;

WiFiClient esp_client;
PubSubClient mqtt_client(esp_client);

void mqttReconnect() {
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      
      delay(5000);
    }
  }
}

void processMqtt() {
  if (!mqtt_client.connected()) {
    mqttReconnect();
  }
  
  mqtt_client.loop();
}

void initializeBoard() {
  Serial.begin(115200);  
}

void initializeSensor() {
  dhtTop.setup(SENSOR_TOP_DATA_PIN, DHTesp::DHT22);
  dhtBottom.setup(SENSOR_BOTTOM_DATA_PIN, DHTesp::DHT22);
} 

void initializeWifi() {
  delay(10);
    
  Serial.print("\nConnecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);  // station mode
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.print("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeMqtt() {
  mqtt_client.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
}
 
void setup() {
  initializeBoard();
  initializeSensor();
    
  initializeWifi();
  initializeMqtt(); 
}

void loop() {
  float humidityTop = dhtTop.getHumidity();
  float temperatureTop = dhtTop.getTemperature();

  float humidityBottom = dhtBottom.getHumidity();
  float temperatureBottom = dhtBottom.getTemperature();
  
  char buffer[64];

  processMqtt();

  sprintf(buffer, "{\"humidity_top\":%.2f,\"temp_top\":%.2f,\"humidity_bottom\":%.2f,\"temp_bottom\":%.2f}", humidityTop, temperatureTop, humidityBottom, temperatureBottom);  

  //Serial.println(buffer);
  mqtt_client.publish(MQTT_HOLODILNIC_TOPIC, buffer);

  delay(5000);
}