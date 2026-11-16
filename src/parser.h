#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <time.h>
#include "device.h"

typedef struct {
    char device_id[MAX_DEVICE_ID_LEN];
    double temperature;
    double humidity;
    int battery;
    time_t timestamp;
} TelemetryData;

bool parse_telemetry_json(const char *json_str, TelemetryData *out_data);
char *build_alert_json(const char *device_id, const char *alert_type, double value, time_t timestamp);

#endif // PARSER_H
