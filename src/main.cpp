#include "DHTesp.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include <PubSubClient.h>

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>

#include <Config.h>

DHTesp dhtTop;
DHTesp dhtBottom;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, UTC_MDT_OFFSET_SECONDS);

float humidityTop, temperatureTop, humidityBottom, temperatureBottom;
String fanOverride = "";

uint8_t counter_1 = 0;
uint16_t counter_2 = 0;

uint8_t fanStateCurrent = LOW;

void mqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      //mqttClient.subscribe(MQTT_HOLODILNIC_FAN_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {  
  // Do nothing
}

void processMqtt() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  
  mqttClient.loop();
}

String getFullFormattedTime() {
   time_t rawtime = timeClient.getEpochTime();
   struct tm * ti;
   ti = localtime (&rawtime);

   uint16_t year = ti->tm_year + 1900;
   String yearStr = String(year);

   uint8_t month = ti->tm_mon + 1;
   String monthStr = month < 10 ? "0" + String(month) : String(month);

   uint8_t day = ti->tm_mday;
   String dayStr = day < 10 ? "0" + String(day) : String(day);

   uint8_t hours = ti->tm_hour;
   String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

   uint8_t minutes = ti->tm_min;
   String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

   uint8_t seconds = ti->tm_sec;
   String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

   return yearStr + "-" + monthStr + "-" + dayStr + " " +
          hoursStr + ":" + minuteStr + ":" + secondStr;
}

void mqttLog(String msg) {
  char bufffer[64];

  timeClient.update();

  msg = ("[" + getFullFormattedTime() + "] " + msg);
  msg.toCharArray(bufffer, sizeof(bufffer));

  mqttClient.publish(MQTT_HOLODILNIC_LOGS_TOPIC, bufffer);
  mqttClient.loop();
}

void initializeBoard() {
  Serial.begin(115200);  
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  digitalWrite(FAN_CONTROL_PIN, fanStateCurrent);
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
  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  mqttClient.setCallback(mqttCallback); 
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
  mqttClient.publish(MQTT_HOLODILNIC_TOPIC, buffer);
  mqttClient.loop();
}

void processFan() {
  uint8_t fanState;

  if(counter_2-- == 0) {
    counter_2 = FIFTEEN_MIN_AT_100_MS_DELAY;
    return;
  } else if(counter_2 <= FIVE_MIN_AT_100_MS_DELAY) {
    fanState = LOW;
  } else {
    fanState = HIGH;
  }

  if(fanState != fanStateCurrent) {
    String fanStateStr = (fanState == HIGH ? "high" : "low");
    fanStateCurrent = fanState;
    digitalWrite(FAN_CONTROL_PIN, fanState);    
    mqttLog("Fan set to " + fanStateStr);
  }
}

void processOTA() {
  ArduinoOTA.handle();
}
 
void setup() {
  initializeBoard();
  initializeSensor();
    
  initializeWifi();
  initializeMqtt(); 

  timeClient.begin();

  initializeOTA();
}

void loop() {
  processMqtt();
  processFan();

  if(counter_1-- == 0) {
    counter_1 = FIVE_SEC_AT_100_MS_DELAY;
    processSensors();
    reportSensorsValues();

    processOTA();
  }

  delay(100);
}