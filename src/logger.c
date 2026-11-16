#define _POSIX_C_SOURCE 200809L

#include "logger.h"
#include <stdio.h>

static FILE *log_file = NULL;

static void format_time(time_t ts, char *buf, size_t len) {
    struct tm tm_info;
    if (localtime_r(&ts, &tm_info) != NULL) {
        strftime(buf, len, "%H:%M:%S", &tm_info);
    } else {
        snprintf(buf, len, "00:00:00");
    }
}

bool logger_init(const char *filepath) {
    if (!filepath) {
        return true;
    }
    log_file = fopen(filepath, "a");
    if (!log_file) {
        perror("Failed to open log file");
        return false;
    }
    return true;
}

void logger_log_telemetry(const TelemetryData *data) {
    if (!data) return;

    char time_buf[16];
    format_time(data->timestamp, time_buf, sizeof(time_buf));

    printf("[%s] Device: %s\n", time_buf, data->device_id);
    printf("  Temperature: %.2f C\n", data->temperature);
    printf("  Humidity:    %.2f%%\n", data->humidity);
    printf("  Battery:     %d%%\n\n", data->battery);
    fflush(stdout);

    if (log_file) {
        fprintf(log_file, "[%s] Device: %s | Temp: %.2f C | Hum: %.2f%% | Bat: %d%%\n",
                time_buf, data->device_id, data->temperature, data->humidity, data->battery);
        fflush(log_file);
    }
}

void logger_log_alert(const char *device_id, const char *alert_type, double value, time_t timestamp) {
    if (!device_id || !alert_type) return;

    char time_buf[16];
    format_time(timestamp, time_buf, sizeof(time_buf));

    printf("[%s] [ALERT] Device: %s | Type: %s | Value: %.2f\n\n",
           time_buf, device_id, alert_type, value);
    fflush(stdout);

    if (log_file) {
        fprintf(log_file, "[%s] [ALERT] Device: %s | Type: %s | Value: %.2f\n",
                time_buf, device_id, alert_type, value);
        fflush(log_file);
    }
}

void logger_log_error(const char *msg) {
    if (!msg) return;
    fprintf(stderr, "[ERROR] %s\n", msg);
    if (log_file) {
        fprintf(log_file, "[ERROR] %s\n", msg);
        fflush(log_file);
    }
}

void logger_close(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}
