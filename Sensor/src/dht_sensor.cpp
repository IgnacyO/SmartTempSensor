#include "dht_sensor.h"
#include <Arduino.h>
#include <DHT.h>
#include <DHT_U.h>

extern int sensorError;

float fGetTemperature(void *pvSensor)
{
    DHT_Unified *xDhtSensor = static_cast<DHT_Unified *>(pvSensor);
    sensors_event_t event;
    xDhtSensor->temperature().getEvent(&event);
    if (isnan(event.temperature))
    {
        Serial.println(F("Error reading temperature!"));
        sensorError = 3; // Error reading temperature
        return NAN;
    }
    else
    {
        Serial.print(F("Temperature: "));
        Serial.print(event.temperature);
        Serial.println(F("°C"));
        return event.temperature;
    }
}
float fGetHumidity(void *pvSensor)
{
    DHT_Unified *xDhtSensor = static_cast<DHT_Unified *>(pvSensor);
    sensors_event_t event;
    xDhtSensor->humidity().getEvent(&event);
    if (isnan(event.relative_humidity))
    {
        Serial.println(F("Error reading humidity!"));
        sensorError = 3; // Error reading humidity
        return NAN;
    }
    else
    {
        Serial.print(F("Humidity: "));
        Serial.print(event.relative_humidity);
        Serial.println(F("°C"));
        return event.relative_humidity;
    }
}
float fGetQuality(void *pvSensor)
{
    return 1.0;
}
int getError(void *pvSensor)
{
    return 0;
}
void *pvInit(void *pvArgs)
{

    dht_sensor_params_t params;
    if (pvArgs == nullptr)
    {
        params.pin = DHT_PIN_DEFAULT;
        params.dht_no = DHT_NO_DEFAULT;
    }
    else
    {
        params = *static_cast<dht_sensor_params_t *>(pvArgs);
    }
    void *pvSensor = new DHT_Unified(params.pin, params.dht_no);
    if (pvSensor == NULL)
    {
        sensorError = 1; // Memory allocation failed
        return NULL;
    }
    return pvSensor;
}
void vBegin(void *pvSensor)
{
    if (pvSensor == NULL)
    {
        sensorError = 2; // Invalid sensor pointer
        return;
    }
    DHT_Unified *dhtSensor = static_cast<DHT_Unified *>(pvSensor);
    dhtSensor->begin();
}

char *pcGetSensorName(void)
{
    return DHT_NO_DEFAULT_STR;
}

void vDeinit(void *pvSensor)
{
}