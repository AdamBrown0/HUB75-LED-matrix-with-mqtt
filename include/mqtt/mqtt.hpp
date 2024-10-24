#ifndef MQTT_HPP
#define MQTT_HPP

#include <AsyncMqttClient.h>
#include <WiFi.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}

extern AsyncMqttClient mqttClient;
extern TimerHandle_t mqttReconnectTimer;
extern TimerHandle_t wifiReconnectTimer;

void connectToWifi();
void connectToMqtt();
void WiFiEvent(WiFiEvent_t event);

void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttSubscribe(const uint16_t& packetId, const uint8_t& qos);
void onMqttUnsubscribe(const uint16_t& packetId);
void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties properties, const size_t& len,
                   const size_t& index, const size_t& total);
void onMqttPublish(const uint16_t& packetId);

#endif // MQTT_HPP