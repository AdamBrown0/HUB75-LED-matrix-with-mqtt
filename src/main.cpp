#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <WiFi.h>

#include "mqtt/config.h"
#include "mqtt/mqtt.hpp"

void setup() {
  Serial.begin(9600);

  while (!Serial && millis() < 5000);

  Serial.printf("\nStarting HUB75-LED-matrix-with-mqtt on %s\n", ARDUINO_BOARD);

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, 
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
}

void loop() {
  // put your main code here, to run repeatedly:
}