#include "mqtt/config.h"

#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <LittleFS.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}

#include "mqtt/mqtt.hpp"

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

File file;
String filename;
bool receivingFile = false;
int expectedChunks = 0;
int receivedChunks = 0;

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
    "file/meta",
    "file/chunk",
    "file/eof",
    "filesystem/print",
    "filesystem/delete_file"
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

void deleteFile(char *path) {
  if (LittleFS.exists(path)) {
    if (LittleFS.remove(path)) {
      Serial.printf("File %s deleted successfully\n", path);
    } else if (LittleFS.rmdir(path)) {
      Serial.printf("Directory %s deleted successfully\n", path);
    } else {
      Serial.printf("Failed to delete file %s\n", path);
    }
  } else {
    Serial.printf("File %s does not exist\n", path);
  }
}

void listFiles(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    for (int i = 0; i < numTabs; i++) {
      Serial.print("  | "); // add tabs for subdirectory levels
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      listFiles(entry, numTabs + 1); // recursive call for subdirectories
    } else {
      // files have sizes, directories do not
      Serial.print(" | Size: ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties properties, const size_t& len, 
                   const size_t& index, const size_t& total) {

  char *payloadCopy;
  if (!strcmp(topic, "file/chunk") == 0) {
    payloadCopy = new char[len + 1];
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
  // Read file
  if (strcmp(topic, "file/meta") == 0) {
    String meta = String(payload).substring(0, len);
    int commaIndex = meta.indexOf(',');
    filename = "/" + meta.substring(0, commaIndex);
    expectedChunks = meta.substring(commaIndex + 1).toInt();

    Serial.println("Received metadata: ");
    Serial.println("Filename: " + filename);
    Serial.println("Total chunks: " + String(expectedChunks));

    // Open file for writing
    file = LittleFS.open(filename, "w");
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
    receivingFile = true;
    receivedChunks = 0;
  } else if (strcmp(topic, "file/chunk") == 0 && receivingFile) {
    file.write((const uint8_t*)payload, len);
    receivedChunks++;

    // Serial.println("Received chunk " + String(receivedChunks) + "/" + String(expectedChunks));

    if (receivedChunks >= expectedChunks) {
      file.close();
      receivingFile = false;
      Serial.println("File transfer complete. File saved as: " + filename);
    }
    
  } else if (strcmp(topic, "file/eof")) {
    // Serial.println("Received EOF signal");
    if (receivingFile) {
      file.close();
      receivingFile = false;
      Serial.println("File transmission complete");
    }
  }

  if (strcmp(topic, "filesystem/print") == 0) {
    File root = LittleFS.open("/");
    if (!root) {
      Serial.println("Failed to open root directory");
      return;
    }

    if (!root.isDirectory()) {
      Serial.println("Root is not a directory");
      return;
    }


    listFiles(root, 0);
    Serial.print("Storage used: ");
    Serial.print(LittleFS.usedBytes());
    Serial.print(" / ");
    Serial.println(LittleFS.totalBytes());
  } else if (strcmp(topic, "filesystem/delete_file") == 0) {
    deleteFile(payloadCopy);
  } 

}


void onMqttPublish(const uint16_t& packetId) {
  Serial.println("Publish acknowledged.");
  Serial.printf("  packetId: %u\n", packetId);
}