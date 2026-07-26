#include <Arduino.h>
#include <WiFi.h>

extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}

//#ifndef WIFI_SSID
#define WIFI_SSID "some_wifi"
//#endif
//#ifndef WIFI_PASS
#define WIFI_PASS "some_pass"
//#endif

enum {
  eIdle,
  eConnect2Wifi
};
TimerHandle_t xWifiReconnectTimer;

void vConnectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStart(xWifiReconnectTimer, 0);
        break;
    }
}

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println();

  delay(5000);
  xWifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(vConnectToWifi));
  delay(3000);
  Serial.println();
  WiFi.onEvent(WiFiEvent);
  vConnectToWifi();
}

void loop() {
  
}