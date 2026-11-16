#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include "parser.h"

bool logger_init(const char *filepath);
void logger_log_telemetry(const TelemetryData *data);
void logger_log_alert(const char *device_id, const char *alert_type, double value, time_t timestamp);
void logger_log_error(const char *msg);
void logger_close(void);

#endif // LOGGER_H
