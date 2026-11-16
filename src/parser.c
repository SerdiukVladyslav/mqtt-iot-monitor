#include "parser.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <math.h>

bool parse_telemetry_json(const char *json_str, TelemetryData *out_data) {
    if (!json_str || !out_data) {
        return false;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    cJSON *dev_id = cJSON_GetObjectItemCaseSensitive(root, "device_id");
    cJSON *temp = cJSON_GetObjectItemCaseSensitive(root, "temperature");
    cJSON *hum = cJSON_GetObjectItemCaseSensitive(root, "humidity");
    cJSON *bat = cJSON_GetObjectItemCaseSensitive(root, "battery");
    cJSON *ts = cJSON_GetObjectItemCaseSensitive(root, "timestamp");

    bool valid = true;

    if (!cJSON_IsString(dev_id) || !dev_id->valuestring || dev_id->valuestring[0] == '\0') {
        valid = false;
    }
    if (!cJSON_IsNumber(temp) || isnan(temp->valuedouble) || isinf(temp->valuedouble)) {
        valid = false;
    }
    if (!cJSON_IsNumber(hum) || isnan(hum->valuedouble) || isinf(hum->valuedouble) ||
        hum->valuedouble < 0.0 || hum->valuedouble > 100.0) {
        valid = false;
    }
    if (!cJSON_IsNumber(bat) || bat->valueint < 0 || bat->valueint > 100) {
        valid = false;
    }
    if (!cJSON_IsNumber(ts) || ts->valuedouble < 0) {
        valid = false;
    }

    if (valid) {
        memset(out_data, 0, sizeof(TelemetryData));
        strncpy(out_data->device_id, dev_id->valuestring, MAX_DEVICE_ID_LEN - 1);
        out_data->device_id[MAX_DEVICE_ID_LEN - 1] = '\0';
        out_data->temperature = temp->valuedouble;
        out_data->humidity = hum->valuedouble;
        out_data->battery = bat->valueint;
        out_data->timestamp = (time_t)ts->valuedouble;
    }

    cJSON_Delete(root);
    return valid;
}

char *build_alert_json(const char *device_id, const char *alert_type, double value, time_t timestamp) {
    if (!device_id || !alert_type) {
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "alert", alert_type);
    cJSON_AddNumberToObject(root, "value", round(value * 100.0) / 100.0);
    cJSON_AddNumberToObject(root, "timestamp", (double)timestamp);

    char *rendered = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return rendered;
}
