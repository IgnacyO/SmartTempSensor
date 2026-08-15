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

#define MQTT_PORT 1883
#endif
#define MQTT_CONNECT_TIMEOUT_MS 5000

#define MAIN_TASK_TIMEOUT_MS 10000
#define LED_PIN 8
#define LED_ON LOW
#define LED_OFF HIGH
#define SENSOR_TOPIC "test"

// User data types
enum ProgramState
{
  Idle,
  ConnectingWiFi,
  ConnectingMqtt,
  Publishing
} volatile eProgState;

typedef int8_t wifi_conn_ret_t;
typedef int8_t mqtt_conn_ret_t;

// Globals
AsyncMqttClient xMqttClient;
TaskHandle_t xMainTaskHandle;
void *pvSensor;
volatile bool xWifiConnected = false;
volatile bool xMqttConnected = false;

// Task and callback functions
void vMainTask(void *pv);

// State specific functions
void vStartWifi(void);
void vStartMqtt(void);

void vHandleWiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    xWifiConnected = true;
    Serial.print("[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());
    vStartMqtt();
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    xWifiConnected = false;
    xMqttConnected = false;
    Serial.println("[WiFi] Disconnected.");
    eProgState = ConnectingWiFi;
    break;
  default:
    break;
  }
}

inline void vBlinkLedFor(u16_t usPeriodMs)
{
  digitalWrite(LED_PIN, LED_ON);
  vTaskDelay(pdMS_TO_TICKS(usPeriodMs / 2));
  digitalWrite(LED_PIN, LED_OFF);
  vTaskDelay(pdMS_TO_TICKS(usPeriodMs / 2));
}

void onMqttConnect(bool sessionPresent)
{
  xMqttConnected = true;
  eProgState = Idle;
  Serial.println("[MQTT] Connection established successfully.");
  Serial.printf("[MQTT] Session present: %s\n", sessionPresent ? "yes" : "no");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  xMqttConnected = false;
  Serial.println("[MQTT] Lost connection to broker.");
  const char *reasonText = "UNKNOWN";
  switch (reason)
  {
  case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
    reasonText = "TCP_DISCONNECTED";
    break;
  case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
    reasonText = "MQTT_UNACCEPTABLE_PROTOCOL_VERSION";
    break;
  case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
    reasonText = "MQTT_IDENTIFIER_REJECTED";
    break;
  case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
    reasonText = "MQTT_SERVER_UNAVAILABLE";
    break;
  case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
    reasonText = "MQTT_MALFORMED_CREDENTIALS";
    break;
  case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
    reasonText = "MQTT_NOT_AUTHORIZED";
    break;
  case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
    reasonText = "ESP8266_NOT_ENOUGH_SPACE";
    break;
  case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
    reasonText = "TLS_BAD_FINGERPRINT";
    break;
  }
  Serial.printf("[MQTT] Disconnect reason: %s (%d)\n", reasonText, static_cast<int>(reason));
  if (xWifiConnected)
  {
    vStartMqtt();
  }
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

  Serial.printf("[SYS] MQTT broker: %s:%u\n", MQTT_HOST, MQTT_PORT);
  Serial.println("========================================");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  eProgState = Idle;

  delay(5000);

  xMqttClient.setServer(MQTT_HOST, (uint16_t)atoi(MQTT_PORT));
  xMqttClient.onConnect(onMqttConnect);
  xMqttClient.onDisconnect(onMqttDisconnect);
  xMqttClient.onPublish(onMqttPublish);

  WiFi.onEvent(vHandleWiFiEvent);
  WiFi.mode(WIFI_STA);

  if (xTaskCreate(vMainTask, "mainTask", 3072, NULL, tskIDLE_PRIORITY, &xMainTaskHandle) != pdPASS)
  {
    for (;;)
      ;
  }

  vStartWifi();
}

void loop()
{
  switch (eProgState)
  {
  case Idle:
    break;
  case ConnectingWiFi:
  case ConnectingMqtt:
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
  vTaskDelay(pdMS_TO_TICKS(5));
}

void vMainTask(void *pv)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MAIN_TASK_TIMEOUT_MS));
    if (!xWifiConnected)
    {
      eProgState = ConnectingWiFi;
      Serial.println("[MAIN] Waiting for WiFi connection...");
      continue;
    }

    if (!xMqttConnected)
    {
      eProgState = ConnectingMqtt;
      Serial.println("[MAIN] Waiting for MQTT connection...");
      vStartMqtt();
      continue;
    }

    eProgState = Publishing;
    float temperature = fGetTemperature(pvSensor);
    Serial.printf("Temp: %f\n", temperature);
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", temperature);
    xMqttClient.publish(SENSOR_TOPIC, 0, false, payload);

    eProgState = Idle;
  }
}

void vStartWifi(void)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    xWifiConnected = true;
    return;
  }

  Serial.printf("[WiFi] Connecting to SSID '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void vStartMqtt(void)
{
  if (!xWifiConnected)
  {
    return;
  }

  if (xMqttClient.connected())
  {
    xMqttConnected = true;
    return;
  }

  Serial.printf("[MQTT] Connecting to broker %s:%u...\n", MQTT_HOST, (uint16_t)atoi(MQTT_PORT));
  eProgState = ConnectingMqtt;
  xMqttClient.connect();
}