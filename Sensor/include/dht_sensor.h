#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define DHT_PIN_DEFAULT 5
#define DHT_NO_DEFAULT DHT11
#define DHT_NO_DEFAULT_STR TOSTRING(DHT_NO_DEFAULT)

typedef struct
{
    int pin;
    int dht_no;
} dht_sensor_params_t;

#endif