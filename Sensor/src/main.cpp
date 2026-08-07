#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <DHT.h>
#include "isensor.h"

// FreeRTOS includes
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

// Macros
#ifndef WIFI_SSID
#define WIFI_SSID "some_wifi"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "some_pass"
#endif
#define WIFI_CONNECT_TIMEOUT_MS 5000

#ifndef MQTT_HOST
#ifdef MQTT_IP
#define MQTT_HOST MQTT_IP
#else
#define MQTT_HOST "192.168.1.10"
#endif
#endif
#ifndef MQTT_PORT
#define MQTT_PORT "1883"
#endif
#define MQTT_CONNECT_TIMEOUT_MS 5000

#define MAIN_TASK_TIMEOUT_MS 10000
#define LED_PIN 8
#define LED_ON LOW
#define LED_OFF HIGH

// User data types
enum ProgramState
{
  Idle,
  Connecting,
  Publishing
} volatile eProgState;

typedef int8_t wifi_conn_ret_t;
typedef int8_t mqtt_conn_ret_t;

// Globals
AsyncMqttClient xMqttClient;
TaskHandle_t xMainTaskHandle;
void *pvSensor;

// Task and callback functions
void vMainTask(void *pv);

// State specific functions
wifi_conn_ret_t xWifiConnect(void);
mqtt_conn_ret_t xMqttConnect(void);

inline void vBlinkLedFor(u16_t usPeriodMs)
{
  digitalWrite(LED_PIN, LED_ON);
  vTaskDelay(pdMS_TO_TICKS(usPeriodMs / 2));
  digitalWrite(LED_PIN, LED_OFF);
  vTaskDelay(pdMS_TO_TICKS(usPeriodMs / 2));
}

void onMqttConnect(bool sessionPresent)
{
  Serial.println("[MQTT] Connection established successfully.");
  Serial.printf("[MQTT] Session present: %s\n", sessionPresent ? "yes" : "no");
  xMqttClient.publish("test", 0, true, "test 1");
  // Serial.println("Publishing at QoS 0");
  // uint16_t packetIdPub1 = xMqttClient.publish("test/lol", 1, true, "test 2");
  // Serial.print("Publishing at QoS 1, packetId: ");
  // Serial.println(packetIdPub1);
  // uint16_t packetIdPub2 = xMqttClient.publish("test/lol", 2, true, "test 3");
  // Serial.print("Publishing at QoS 2, packetId: ");
  // Serial.println(packetIdPub2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("[MQTT] Lost connection to broker.");
  Serial.printf("[MQTT] Disconnect reason: %d\n", reason);
}

void onMqttPublish(uint16_t packetId)
{
  Serial.println("[MQTT] Publish acknowledged.");
  Serial.printf("[MQTT] Packet ID: %u\n", packetId);
}

void setup()
{
  Serial.begin(9600);
  delay(1000);
  Serial.println();
  Serial.println("========================================");
  Serial.println("SmartTempSensor starting up...");

  pvSensor = pvInit(nullptr);
  if (sensorError)
  {
    for (;;)
      ;
  }

  Serial.printf("[SYS] Using WiFi SSID: %s\n", WIFI_SSID);

  IPAddress mqttHost;
  if (!mqttHost.fromString(MQTT_HOST))
  {
    Serial.printf("[SYS] Invalid MQTT host '%s'. Falling back to default.\n", MQTT_HOST);
    mqttHost = IPAddress(192, 168, 1, 10);
  }

  uint16_t mqttPort = (uint16_t)atoi(MQTT_PORT);
  if (mqttPort == 0)
  {
    Serial.printf("[SYS] Invalid MQTT port '%s'. Falling back to default 1883.\n", MQTT_PORT);
    mqttPort = 1883;
  }

  Serial.printf("[SYS] MQTT broker: %s:%u\n", MQTT_HOST, mqttPort);
  Serial.println("========================================");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  eProgState = Idle;

  delay(5000);

  if (xTaskCreate(vMainTask, "mainTask", 3072, NULL, tskIDLE_PRIORITY, &xMainTaskHandle) != pdPASS)
  {
    for (;;)
      ;
  }
  xMqttClient.onConnect(onMqttConnect);
  xMqttClient.onDisconnect(onMqttDisconnect);
  xMqttClient.onPublish(onMqttPublish);
  xMqttClient.setServer(mqttHost, mqttPort);
}

void loop()
{
  switch (eProgState)
  {
  case Idle:
    break;
  case Connecting:
    vBlinkLedFor(200);
    break;
  case Publishing:
    if (digitalRead(LED_PIN))
    {
      digitalWrite(LED_PIN, LED_ON);
    }
  default:
    // vBlinkLedFor(1000);
    break;
  }
}

void vMainTask(void *pv)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MAIN_TASK_TIMEOUT_MS));
    if (!xMqttClient.connected())
    {
      Serial.println("[MAIN] MQTT client is not connected. Starting reconnect sequence...");
      eProgState = Connecting;
      if (!WiFi.isConnected())
      {
        Serial.println("[MAIN] WiFi is not connected. Attempting WiFi connection...");
        if (xWifiConnect())
        {
          Serial.println("[MAIN] WiFi connection attempt failed.");
          goto task_return;
        }
      }
      else
      {
        Serial.println("[MAIN] WiFi is already connected; proceeding to MQTT.");
      }
      if (xMqttConnect())
      {
        Serial.println("[MAIN] MQTT connection attempt failed.");
        goto task_return;
      }
    }
    else
    {
      Serial.println("[MAIN] MQTT client is already connected. No reconnect needed.");
    }

    Serial.printf("Temp: %f\n", fGetTemperature(pvSensor));
    // do rest
  task_return:
    eProgState = Idle;
  }
}

wifi_conn_ret_t xWifiConnect(void)
{
  Serial.printf("[WiFi] Attempting to connect to SSID '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t startTime = millis();
  int lastStatus = -1;
  while (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS)
  {
    int currentStatus = WiFi.status();
    if (currentStatus != lastStatus)
    {
      Serial.printf("[WiFi] Status update: %d\n", currentStatus);
      lastStatus = currentStatus;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("[WiFi] Connected successfully.");
      Serial.print("[WiFi] Assigned IP address: ");
      Serial.println(WiFi.localIP());
      return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  Serial.printf("[WiFi] Connection failed after %lu ms. Final status: %d\n", millis() - startTime, WiFi.status());
  return -1;
}

mqtt_conn_ret_t xMqttConnect(void)
{
  Serial.printf("[MQTT] Attempting connection to broker using configured host/port...\n");
  xMqttClient.connect();
  uint32_t startTime = millis();
  while (millis() - startTime < MQTT_CONNECT_TIMEOUT_MS)
  {
    if (xMqttClient.connected())
    {
      Serial.println("[MQTT] Connected successfully.");
      return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  Serial.printf("[MQTT] Connection failed after %lu ms.\n", millis() - startTime);

  return -1;
}