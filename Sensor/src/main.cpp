#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>

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

// Task and callback functions
void vMainTask(void *pv);

// State specific functions
wifi_conn_ret_t xWifiConnect(void);

inline void vBlinkLedFor(u16_t usPeriodMs)
{
  digitalWrite(LED_PIN, LED_ON);
  vTaskDelay(pdMS_TO_TICKS(usPeriodMs / 2));
  digitalWrite(LED_PIN, LED_OFF);
  vTaskDelay(pdMS_TO_TICKS(usPeriodMs / 2));
}

void setup()
{
  Serial.begin(9600);
  Serial.println();
  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  eProgState = Idle;

  delay(5000);

  if (xTaskCreate(vMainTask, "mainTask", 3072, NULL, tskIDLE_PRIORITY, &xMainTaskHandle) != pdPASS)
  {
    for (;;)
      ;
  }
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
      eProgState = Connecting;
      if (!WiFi.isConnected())
      {
        if (xWifiConnect())
        {
          goto task_return;
        }
      }
      // connect to mqtt
    }
    // do rest
  task_return:
    eProgState = Idle;
  }
}

wifi_conn_ret_t xWifiConnect(void)
{
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t startTime = millis();
  while (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return -1;
}