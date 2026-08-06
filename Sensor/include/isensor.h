#ifndef ISENSOR_H
#define ISENSOR_H
extern int sensorError;
float fGetTemperature(void *pvSensor);
float fGetHumidity(void *pvSensor);
float fGetQuality(void *pvSensor);
int getError(void *pvSensor);
void *pvInit(void *pvArgs);
void vBegin(void *pvSensor);
void vDeinit(void *pvSensor);
#endif