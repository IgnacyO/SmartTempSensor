#include <Arduino.h>
#include <WiFi.h>

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
#define WIFI_CONNECT_TRIES_COUNT 3
#define WIFI_CONNECT_TIMEOUT_MS 1000

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

enum ConnectionState
{
  WifiConnected,
  MTTQConnected,
  Disconnected
} volatile eConnState;

enum WifiStateFlag : uint32_t
{
  Wifi_PendingFlag = 0,
  Wifi_FinishedFlag = 0b001,
  Wifi_SuccessFlag = 0b010,
  Wifi_FailFlag = 0b100
};

typedef int8_t wifi_conn_ret_t;

// Globals
TaskHandle_t xMainTaskHandle;
volatile uint32_t ulWifiStateFlag;

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

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    ulWifiStateFlag = Wifi_FinishedFlag | Wifi_SuccessFlag;
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    ulWifiStateFlag = Wifi_FinishedFlag | Wifi_FailFlag;
    break;
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println();
  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  eProgState = Idle;
  eConnState = Disconnected;

  delay(5000);

  if (xTaskCreate(vMainTask, "mainTask", 3072, NULL, tskIDLE_PRIORITY, &xMainTaskHandle) != pdPASS)
  {
    for (;;)
      ;
  }
  WiFi.onEvent(WiFiEvent);
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

void vMainTask(TimerHandle_t xTimer)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MAIN_TASK_TIMEOUT_MS));
    if (eConnState != MTTQConnected)
    {
      eProgState = Connecting;
      if (eConnState != WifiConnected)
      {
        if (xWifiConnect())
        {
          goto task_return;
        }
      }
      // connect to MTTQ
    }
    // do rest
  task_return:
    eProgState = Idle;
  }
}

wifi_conn_ret_t xWifiConnect(void)
{
  ulWifiStateFlag = Wifi_PendingFlag;
  for (uint8_t tries = 0; tries < WIFI_CONNECT_TRIES_COUNT; tries++)
  {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t startTime = millis();
    while (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS)
    {
      if (ulWifiStateFlag & (Wifi_SuccessFlag | Wifi_FinishedFlag))
      {
        eConnState = WifiConnected;
        return 0;
      }
      else if (ulWifiStateFlag & (Wifi_FailFlag | Wifi_FinishedFlag))
      {
        eConnState = Disconnected;
        return -1;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  eConnState = Disconnected;
  return -1;
}