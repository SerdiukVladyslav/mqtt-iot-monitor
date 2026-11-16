#ifndef DEVICE_H
#define DEVICE_H

#include <time.h>
#include <stddef.h>

#define MAX_DEVICE_ID_LEN 64

typedef struct {
    char device_id[MAX_DEVICE_ID_LEN];
    double temperature;
    double humidity;
    int battery;
    time_t last_timestamp;
} DeviceInfo;

typedef struct {
    DeviceInfo *items;
    size_t count;
    size_t capacity;
} DeviceRegistry;

void registry_init(DeviceRegistry *reg);
DeviceInfo *registry_upsert(DeviceRegistry *reg, const char *device_id,
                            double temperature, double humidity,
                            int battery, time_t timestamp);
void registry_free(DeviceRegistry *reg);

#endif // DEVICE_H
