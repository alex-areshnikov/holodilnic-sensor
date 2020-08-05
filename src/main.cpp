#include "DHTesp.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include <PubSubClient.h>

#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <Config.h>

DHTesp dhtTop;
DHTesp dhtBottom;

WiFiClient esp_client;
PubSubClient mqtt_client(esp_client);

float humidityTop, temperatureTop, humidityBottom, temperatureBottom;

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
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  digitalWrite(FAN_CONTROL_PIN, LOW);
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

void initializeOTA() {
  Serial.print("Initialize OTA... ");
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);

  // No authentication by default
  // ArduinoOTA.setPassword(OTA_PWD);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
}

void processSensors() {
  humidityTop = dhtTop.getHumidity();
  temperatureTop = dhtTop.getTemperature();
  humidityBottom = dhtBottom.getHumidity();
  temperatureBottom = dhtBottom.getTemperature();
}

void reportSensorsValues() {
  char buffer[64];
  sprintf(buffer, "{\"humidity_top\":%.2f,\"temp_top\":%.2f,\"humidity_bottom\":%.2f,\"temp_bottom\":%.2f}", humidityTop, temperatureTop, humidityBottom, temperatureBottom);  
  //Serial.println(buffer);
  mqtt_client.publish(MQTT_HOLODILNIC_TOPIC, buffer);
}

void processFan() {
  uint8_t fanState = ((((humidityTop + humidityBottom) / 2) > FAN_HUMIDITY_THRESHOLD) ? LOW : HIGH);
  digitalWrite(FAN_CONTROL_PIN, fanState);
}

void processOTA() {
  ArduinoOTA.handle();
}
 
void setup() {
  initializeBoard();
  initializeSensor();
    
  initializeWifi();
  initializeMqtt(); 

  initializeOTA();
}

void loop() {
  processSensors();
  reportSensorsValues();

  processFan();
  processMqtt();
  processOTA();

  delay(5000);
}