#include "dht_sensor.h"
#include "isensor.h"
#include <Arduino.h>
#include <DHT.h>
#include <DHT_U.h>

int sensorError = SENSOR_OK;

const char *pcSensorErrorToString(int code)
{
    switch (code)
    {
    case SENSOR_OK:
        return "OK";
    case SENSOR_INIT_FAILED:
        return "INIT_FAILED";
    case SENSOR_INVALID_HANDLE:
        return "INVALID_HANDLE";
    case SENSOR_READ_FAILED:
        return "READ_FAILED";
    case SENSOR_INVALID_CONFIG:
        return "INVALID_CONFIG";
    default:
        return "UNKNOWN";
    }
}

static void vLogSensorFailure(const char *operation, int errorCode)
{
    Serial.printf("[SENSOR] %s failed: %s (%d)\n", operation, pcSensorErrorToString(errorCode), errorCode);
}

float fGetTemperature(void *pvSensor)
{
    if (pvSensor == nullptr)
    {
        sensorError = SENSOR_INVALID_HANDLE;
        vLogSensorFailure("Temperature read", sensorError);
        return NAN;
    }

    DHT_Unified *xDhtSensor = static_cast<DHT_Unified *>(pvSensor);
    sensors_event_t event;
    xDhtSensor->temperature().getEvent(&event);

    if (isnan(event.temperature))
    {
        sensorError = SENSOR_READ_FAILED;
        vLogSensorFailure("Temperature read", sensorError);
        return NAN;
    }

    sensorError = SENSOR_OK;
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    return event.temperature;
}

float fGetHumidity(void *pvSensor)
{
    if (pvSensor == nullptr)
    {
        sensorError = SENSOR_INVALID_HANDLE;
        vLogSensorFailure("Humidity read", sensorError);
        return NAN;
    }

    DHT_Unified *xDhtSensor = static_cast<DHT_Unified *>(pvSensor);
    sensors_event_t event;
    xDhtSensor->humidity().getEvent(&event);

    if (isnan(event.relative_humidity))
    {
        sensorError = SENSOR_READ_FAILED;
        vLogSensorFailure("Humidity read", sensorError);
        return NAN;
    }

    sensorError = SENSOR_OK;
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
    return event.relative_humidity;
}

float fGetQuality(void *pvSensor)
{
    (void)pvSensor;
    return 1.0;
}

int getError(void *pvSensor)
{
    (void)pvSensor;
    return sensorError;
}

void *pvInit(void *pvArgs)
{
    sensorError = SENSOR_OK;

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

    if (params.pin < 0 || params.dht_no <= 0)
    {
        sensorError = SENSOR_INVALID_CONFIG;
        vLogSensorFailure("Sensor config", sensorError);
        return nullptr;
    }

    void *pvSensor = new DHT_Unified(params.pin, params.dht_no);
    if (pvSensor == nullptr)
    {
        sensorError = SENSOR_INIT_FAILED;
        vLogSensorFailure("Sensor allocation", sensorError);
        return nullptr;
    }

    Serial.printf("[SENSOR] Initialized DHT sensor on GPIO %d (%s)\n", params.pin, DHT_NO_DEFAULT_STR);
    return pvSensor;
}

void vBegin(void *pvSensor)
{
    if (pvSensor == nullptr)
    {
        sensorError = SENSOR_INVALID_HANDLE;
        vLogSensorFailure("Sensor begin", sensorError);
        return;
    }

    DHT_Unified *dhtSensor = static_cast<DHT_Unified *>(pvSensor);
    dhtSensor->begin();
    sensorError = SENSOR_OK;
    Serial.printf("[SENSOR] DHT begin completed successfully.\n");
}

char *pcGetSensorName(void)
{
    return DHT_NO_DEFAULT_STR;
}

void vDeinit(void *pvSensor)
{
    if (pvSensor == nullptr)
    {
        return;
    }

    delete static_cast<DHT_Unified *>(pvSensor);
    sensorError = SENSOR_OK;
}