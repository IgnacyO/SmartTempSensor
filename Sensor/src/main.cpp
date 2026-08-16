#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "isensor.h"
#include "dht_sensor.h"

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
#define MQTT_RECONNECT_MIN_INTERVAL_MS 3000
#define MQTT_TOPIC_DOMAIN "test"
#define MQTT_PAYLOAD_BUFFER_SZ 2048

#define MAIN_TASK_TIMEOUT_MS 10000
#define LED_PIN 8
#define LED_ON LOW
#define LED_OFF HIGH

#ifndef SENSOR_LOCATION
#define SENSOR_LOCATION "backrooms"

#define MQTT_TOPIC MQTT_TOPIC_DOMAIN "/" SENSOR_LOCATION
#endif

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
volatile bool xMqttConnecting = false;
volatile bool xMqttConnectedEvent = false;
volatile bool xMqttDisconnectedEvent = false;
volatile int xLastMqttDisconnectReason = -1;
uint32_t u32LastMqttConnectAttemptMs = 0;
JsonDocument xMqttPayload;
char caPayloadBuffer[MQTT_PAYLOAD_BUFFER_SZ];

// Helper functions
inline int RWorNULL(JsonDocument *pxJsonDoc, void *pvSensor, float (*xReadingFn)(void *), const char *key)
{
  float reading = xReadingFn(pvSensor);
  if ((reading == 0 && sensorError == 0) || sensorError > 1)
  {
    (*pxJsonDoc)[key] = nullptr; // Writes null value instead of 0
    return 0;
  }
  (*pxJsonDoc)[key] = reading;
  return -1;
}

const char *pcMqttDisconnectReasonToString(AsyncMqttClientDisconnectReason reason)
{
  switch (reason)
  {
  case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
    return "TCP_DISCONNECTED";
  case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
    return "MQTT_UNACCEPTABLE_PROTOCOL_VERSION";
  case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
    return "MQTT_IDENTIFIER_REJECTED";
  case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
    return "MQTT_SERVER_UNAVAILABLE";
  case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
    return "MQTT_MALFORMED_CREDENTIALS";
  case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
    return "MQTT_NOT_AUTHORIZED";
  case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
    return "ESP8266_NOT_ENOUGH_SPACE";
  case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
    return "TLS_BAD_FINGERPRINT";
  default:
    return "UNKNOWN";
  }
}

const char *pcWiFiDisconnectReasonToString(uint8_t reason)
{
  switch (reason)
  {
  case 2:
    return "AUTH_EXPIRE";
  case 4:
    return "ASSOC_EXPIRE";
  case 15:
    return "4WAY_HANDSHAKE_TIMEOUT";
  case 201:
    return "NO_AP_FOUND";
  case 202:
    return "AUTH_FAIL";
  case 203:
    return "ASSOC_FAIL";
  case 204:
    return "HANDSHAKE_TIMEOUT";
  default:
    return "UNKNOWN";
  }
}

uint16_t u16GetMqttPort(void)
{
  return static_cast<uint16_t>(atoi(MQTT_PORT));
}

// Task and callback functions
void vMainTask(void *pv);

// State specific functions
void vStartWifi(void);
void vStartMqtt(void);

void vHandleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  switch (event)
  {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    xWifiConnected = true;
    Serial.print("[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());
    eProgState = ConnectingMqtt;
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    xWifiConnected = false;
    xMqttConnected = false;
    xMqttConnecting = false;
    Serial.printf("[WiFi] Disconnected. reason=%u (%s)\n", info.wifi_sta_disconnected.reason, pcWiFiDisconnectReasonToString(info.wifi_sta_disconnected.reason));
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
  xMqttConnecting = false;
  xMqttConnectedEvent = true;
  eProgState = Idle;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  xMqttConnected = false;
  xMqttConnecting = false;
  xLastMqttDisconnectReason = static_cast<int>(reason);
  xMqttDisconnectedEvent = true;
}

void onMqttPublish(uint16_t packetId)
{
  (void)packetId;
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
  vBegin(pvSensor);
  if (sensorError)
  {
    for (;;)
      ;
  }

  Serial.printf("[SYS] Using WiFi SSID: %s\n", WIFI_SSID);

  Serial.printf("[SYS] MQTT broker: %s:%u\n", MQTT_HOST, u16GetMqttPort());
  Serial.println("========================================");

  xMqttPayload["sensor"] = pcGetSensorName();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  eProgState = Idle;

  delay(5000);

  xMqttClient.setServer(MQTT_HOST, u16GetMqttPort());
  xMqttClient.onConnect(onMqttConnect);
  xMqttClient.onDisconnect(onMqttDisconnect);
  xMqttClient.onPublish(onMqttPublish);

  WiFi.onEvent(vHandleWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

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
    if (digitalRead(LED_PIN) == LED_ON)
    {
      digitalWrite(LED_PIN, LED_OFF);
    }
    break;
  case ConnectingWiFi:
  case ConnectingMqtt:
    vBlinkLedFor(200);
    break;
  case Publishing:
    if (digitalRead(LED_PIN) == LED_OFF)
    {
      digitalWrite(LED_PIN, LED_ON);
    }
    break;
  default:
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

    if (xMqttConnectedEvent)
    {
      xMqttConnectedEvent = false;
      Serial.println("[MQTT] Connection established successfully.");
    }

    if (xMqttDisconnectedEvent)
    {
      int reason = xLastMqttDisconnectReason;
      xMqttDisconnectedEvent = false;
      Serial.println("[MQTT] Lost connection to broker.");
      Serial.printf("[MQTT] Disconnect reason: %s (%d)\n", pcMqttDisconnectReasonToString(static_cast<AsyncMqttClientDisconnectReason>(reason)), reason);
    }

    if (!xWifiConnected)
    {
      eProgState = ConnectingWiFi;
      Serial.println("[MAIN] Waiting for WiFi connection...");
      vStartWifi();
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
    RWorNULL(&xMqttPayload, pvSensor, fGetTemperature, "temperature");
    RWorNULL(&xMqttPayload, pvSensor, fGetHumidity, "humidity");
    RWorNULL(&xMqttPayload, pvSensor, fGetQuality, "quality");
    serializeJson(xMqttPayload, caPayloadBuffer);
    xMqttClient.publish(MQTT_TOPIC, 0, false, caPayloadBuffer);
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
    xMqttConnecting = false;
    return;
  }

  if (xMqttConnecting)
  {
    return;
  }

  uint32_t u32NowMs = millis();
  if (u32NowMs - u32LastMqttConnectAttemptMs < MQTT_RECONNECT_MIN_INTERVAL_MS)
  {
    return;
  }

  u32LastMqttConnectAttemptMs = u32NowMs;
  xMqttConnecting = true;

  Serial.printf("[MQTT] Connecting to broker %s:%u...\n", MQTT_HOST, u16GetMqttPort());
  eProgState = ConnectingMqtt;
  xMqttClient.connect();
}