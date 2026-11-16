#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <mosquitto.h>

#include "device.h"
#include "parser.h"
#include "logger.h"

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT 1883
#define TELEMETRY_TOPIC "iot/devices/+/telemetry"

#define TEMP_THRESHOLD 30.0
#define HUMIDITY_THRESHOLD 80.0
#define BATTERY_THRESHOLD 20

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    keep_running = 0;
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s [-H HOST] [-P PORT] [-o FILE] [-h]\n", prog_name);
    printf("Options:\n");
    printf("  -H HOST   MQTT broker hostname or IP (default: %s)\n", DEFAULT_HOST);
    printf("  -P PORT   MQTT broker port (default: %d)\n", DEFAULT_PORT);
    printf("  -o FILE   Optional log file path\n");
    printf("  -h        Display this help message\n");
}

static void publish_alert(struct mosquitto *mosq, const char *device_id,
                          const char *alert_type, double value, time_t timestamp) {
    char topic[128];
    snprintf(topic, sizeof(topic), "iot/devices/%s/alert", device_id);

    char *payload = build_alert_json(device_id, alert_type, value, timestamp);
    if (!payload) {
        logger_log_error("Failed to allocate alert JSON payload");
        return;
    }

    int rc = mosquitto_publish(mosq, NULL, topic, (int)strlen(payload), payload, 0, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "Failed to publish alert to %s: %s",
                 topic, mosquitto_strerror(rc));
        logger_log_error(err_buf);
    } else {
        logger_log_alert(device_id, alert_type, value, timestamp);
    }

    free(payload);
}

static void check_thresholds(struct mosquitto *mosq, const TelemetryData *data) {
    if (data->temperature > TEMP_THRESHOLD) {
        publish_alert(mosq, data->device_id, "HIGH_TEMPERATURE", data->temperature, data->timestamp);
    }
    if (data->humidity > HUMIDITY_THRESHOLD) {
        publish_alert(mosq, data->device_id, "HIGH_HUMIDITY", data->humidity, data->timestamp);
    }
    if (data->battery < BATTERY_THRESHOLD) {
        publish_alert(mosq, data->device_id, "LOW_BATTERY", (double)data->battery, data->timestamp);
    }
}

static void on_connect(struct mosquitto *mosq, void *userdata, int rc) {
    (void)userdata;
    if (rc == 0) {
        printf("[INFO] Connected to MQTT broker. Subscribing to '%s'...\n", TELEMETRY_TOPIC);
        int sub_rc = mosquitto_subscribe(mosq, NULL, TELEMETRY_TOPIC, 0);
        if (sub_rc != MOSQ_ERR_SUCCESS) {
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf), "Subscription failed: %s", mosquitto_strerror(sub_rc));
            logger_log_error(err_buf);
        }
    } else {
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "Broker connection refused: %s", mosquitto_connack_string(rc));
        logger_log_error(err_buf);
    }
}

static void on_disconnect(struct mosquitto *mosq, void *userdata, int rc) {
    (void)mosq;
    (void)userdata;
    if (rc != 0) {
        fprintf(stderr, "[WARN] Unexpected disconnection from broker (rc=%d). Reconnecting...\n", rc);
    } else {
        printf("[INFO] Disconnected cleanly from MQTT broker.\n");
    }
}

static void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    DeviceRegistry *registry = (DeviceRegistry *)userdata;

    if (!msg || !msg->payload || msg->payloadlen <= 0) {
        logger_log_error("Received empty MQTT message payload");
        return;
    }

    char *payload_str = malloc((size_t)msg->payloadlen + 1);
    if (!payload_str) {
        logger_log_error("Memory allocation failed for incoming message");
        return;
    }
    memcpy(payload_str, msg->payload, (size_t)msg->payloadlen);
    payload_str[msg->payloadlen] = '\0';

    TelemetryData data;
    if (!parse_telemetry_json(payload_str, &data)) {
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "Malformed or invalid telemetry JSON on topic '%s'",
                 msg->topic ? msg->topic : "unknown");
        logger_log_error(err_buf);
        free(payload_str);
        return;
    }
    free(payload_str);

    if (!registry_upsert(registry, data.device_id, data.temperature,
                         data.humidity, data.battery, data.timestamp)) {
        logger_log_error("Failed to update device registry in memory");
    }

    logger_log_telemetry(&data);
    check_thresholds(mosq, &data);
}

int main(int argc, char *argv[]) {
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    const char *log_path = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "H:P:o:h")) != -1) {
        switch (opt) {
            case 'H':
                host = optarg;
                break;
            case 'P':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'o':
                log_path = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (!logger_init(log_path)) {
        return EXIT_FAILURE;
    }

    DeviceRegistry registry;
    registry_init(&registry);

    mosquitto_lib_init();

    struct mosquitto *mosq = mosquitto_new("iot_monitor_c_client", true, &registry);
    if (!mosq) {
        logger_log_error("Failed to create mosquitto client instance");
        registry_free(&registry);
        logger_close();
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_message_callback_set(mosq, on_message);
    mosquitto_reconnect_delay_set(mosq, 1, 10, true);

    printf("[INFO] Connecting to MQTT broker at %s:%d...\n", host, port);
    int rc = mosquitto_connect_async(mosq, host, port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "Initial connect failed: %s", mosquitto_strerror(rc));
        logger_log_error(err_buf);
        mosquitto_destroy(mosq);
        registry_free(&registry);
        logger_close();
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    rc = mosquitto_loop_start(mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "Failed to start mosquitto loop: %s", mosquitto_strerror(rc));
        logger_log_error(err_buf);
        mosquitto_destroy(mosq);
        registry_free(&registry);
        logger_close();
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    while (keep_running) {
        sleep(1);
    }

    printf("\n[INFO] Shutting down gracefully...\n");
    mosquitto_disconnect(mosq);
    mosquitto_loop_stop(mosq, false);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    registry_free(&registry);
    logger_close();

    printf("[INFO] Cleanup complete. Exited.\n");
    return EXIT_SUCCESS;
}
