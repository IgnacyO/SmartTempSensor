#ifndef ISENSOR_H
#define ISENSOR_H

enum SensorErrorCode
{
    SENSOR_OK = 0,
    SENSOR_INIT_FAILED = 1,
    SENSOR_INVALID_HANDLE = 2,
    SENSOR_READ_FAILED = 3,
    SENSOR_INVALID_CONFIG = 4
};

extern int sensorError;
const char *pcSensorErrorToString(int code);
float fGetTemperature(void *pvSensor);
float fGetHumidity(void *pvSensor);
float fGetQuality(void *pvSensor);
char *pcGetSensorName(void);
int getError(void *pvSensor);
void *pvInit(void *pvArgs);
void vBegin(void *pvSensor);
void vDeinit(void *pvSensor);
void fGetReliability(void *pvSensor);
#endif