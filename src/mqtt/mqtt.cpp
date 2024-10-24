#include "mqtt/config.h"

#include <WiFi.h>
#include <AsyncMqttClient.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}

#include "mqtt/mqtt.hpp"

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.setCredentials("mosquitto", "1234");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    connectToMqtt();
    break;

  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT whilst reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);

  default:
    break;
  }
}

void printSeperationLine() {
  Serial.println("-----------------------------------------");
}

void onMqttConnect(bool sessionPresent) {
  const char *topics[] = {
    "esp32/power_switch",
    "esp32/brightness",
    "esp32/mode",
  };
  const int numTopics = sizeof(topics) / sizeof(topics[0]);

  Serial.printf("Connected to MQTT broker: %s, port: %d\n", MQTT_HOST, MQTT_PORT);
  printSeperationLine();
  Serial.printf("Session present: %d\n", sessionPresent);

  for (int i = 0; i < numTopics; ++i) {
    uint16_t packetIdSub = mqttClient.subscribe(topics[i], 2);
    Serial.printf("Subscribing to topic: %s at QoS 2, packetId: %u\n", topics[i], packetIdSub);
  }

  printSeperationLine();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  (void)reason;

  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(const uint16_t& packetId, const uint8_t& qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.printf("  packetId: %u", packetId);
  Serial.printf("  qos: %u\n", qos);
}

void onMqttUnsubscribe(const uint16_t& packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.printf("  packetId: %u\n", packetId);
}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties properties, const size_t& len, 
                   const size_t& index, const size_t& total) {

  char *payloadCopy = new char[len + 1];
  memcpy(payloadCopy, payload, len);
  payloadCopy[len] = '\0';

  Serial.println("Publish received.");
  Serial.printf("  topic: %s", topic);
  Serial.printf("  qos: %u", properties.qos);
  Serial.printf("  dup: %d", properties.dup);
  Serial.printf("  retain: %d", properties.retain);
  Serial.printf("  len: %zu", len);
  Serial.printf("  index: %zu", index);
  Serial.printf("  total: %zu", total);
  Serial.printf("  payload: %s\n", payloadCopy);
}

void onMqttPublish(const uint16_t& packetId) {
  Serial.println("Publish acknowledged.");
  Serial.printf("  packetId: %u\n", packetId);
}