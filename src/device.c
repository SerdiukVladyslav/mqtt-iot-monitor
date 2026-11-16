#include "device.h"
#include <stdlib.h>
#include <string.h>

void registry_init(DeviceRegistry *reg) {
    if (!reg) return;
    reg->items = NULL;
    reg->count = 0;
    reg->capacity = 0;
}

DeviceInfo *registry_upsert(DeviceRegistry *reg, const char *device_id,
                            double temperature, double humidity,
                            int battery, time_t timestamp) {
    if (!reg || !device_id) return NULL;

    for (size_t i = 0; i < reg->count; i++) {
        if (strncmp(reg->items[i].device_id, device_id, MAX_DEVICE_ID_LEN) == 0) {
            reg->items[i].temperature = temperature;
            reg->items[i].humidity = humidity;
            reg->items[i].battery = battery;
            reg->items[i].last_timestamp = timestamp;
            return &reg->items[i];
        }
    }

    if (reg->count >= reg->capacity) {
        size_t new_cap = (reg->capacity == 0) ? 8 : reg->capacity * 2;
        DeviceInfo *new_items = realloc(reg->items, new_cap * sizeof(DeviceInfo));
        if (!new_items) {
            return NULL;
        }
        reg->items = new_items;
        reg->capacity = new_cap;
    }

    DeviceInfo *dev = &reg->items[reg->count++];
    memset(dev, 0, sizeof(DeviceInfo));
    strncpy(dev->device_id, device_id, MAX_DEVICE_ID_LEN - 1);
    dev->device_id[MAX_DEVICE_ID_LEN - 1] = '\0';
    dev->temperature = temperature;
    dev->humidity = humidity;
    dev->battery = battery;
    dev->last_timestamp = timestamp;

    return dev;
}

void registry_free(DeviceRegistry *reg) {
    if (!reg) return;
    free(reg->items);
    reg->items = NULL;
    reg->count = 0;
    reg->capacity = 0;
}
