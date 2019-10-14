/*
 * This file is part of the ESP32-XBee distribution (https://github.com/nebkat/esp32-xbee).
 * Copyright (c) 2019 Nebojsa Cvetkovic.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <esp_log.h>
#include <string.h>
#include <nmea.h>

#include "bluetooth.h"
#include "uart.h"
#include "config.h"
#include "socket_server.h"
#include "tasks.h"

static const char *TAG = "UART";

#define UART_PORT_DEFAULT UART_NUM_0
#define UART_TX_PIN_DEFAULT UART_PIN_NO_CHANGE
#define UART_RX_PIN_DEFAULT UART_PIN_NO_CHANGE
#define UART_RTS_PIN_DEFAULT UART_PIN_NO_CHANGE
#define UART_CTS_PIN_DEFAULT UART_PIN_NO_CHANGE
#define UART_BAUD_RATE_DEFAULT 115200
#define UART_DATA_BITS_DEFAULT UART_DATA_8_BITS
#define UART_PARITY_DEFAULT UART_PARITY_DISABLE
#define UART_STOP_BITS_DEFAULT UART_STOP_BITS_1

ESP_EVENT_DEFINE_BASE(UART_EVENTS);

void uart_register_handler(esp_event_handler_t event_handler) {
    ESP_ERROR_CHECK(esp_event_handler_register(UART_EVENTS, 0, event_handler, NULL));
}

static int uart_port = -1;
static bool uart_log_forward = false;

void uart_init() {
    uart_log_forward = config_get_bool1(CONF_ITEM(KEY_CONFIG_UART_LOG_FORWARD));

    uart_port = config_get_u8(CONF_ITEM(KEY_CONFIG_UART_NUM));

    uart_hw_flowcontrol_t flow_ctrl;
    bool flow_ctrl_rts = config_get_bool1(CONF_ITEM(KEY_CONFIG_UART_FLOW_CTRL_RTS));
    bool flow_ctrl_cts = config_get_bool1(CONF_ITEM(KEY_CONFIG_UART_FLOW_CTRL_CTS));
    if (flow_ctrl_rts && flow_ctrl_cts) {
        flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
    } else if (flow_ctrl_rts) {
        flow_ctrl = UART_HW_FLOWCTRL_RTS;
    } else if (flow_ctrl_cts) {
        flow_ctrl = UART_HW_FLOWCTRL_CTS;
    } else {
        flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    }

    uart_config_t uart_config = {
            .baud_rate = config_get_u32(CONF_ITEM(KEY_CONFIG_UART_BAUD_RATE)),
            .data_bits = config_get_u8(CONF_ITEM(KEY_CONFIG_UART_DATA_BITS)),
            .parity = config_get_u8(CONF_ITEM(KEY_CONFIG_UART_PARITY)),
            .stop_bits = config_get_u8(CONF_ITEM(KEY_CONFIG_UART_STOP_BITS)),
            .flow_ctrl = flow_ctrl
    };
    ESP_ERROR_CHECK(uart_param_config(uart_port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
            uart_port,
            config_get_i8(CONF_ITEM(KEY_CONFIG_UART_TX_PIN)),
            config_get_i8(CONF_ITEM(KEY_CONFIG_UART_RX_PIN)),
            config_get_i8(CONF_ITEM(KEY_CONFIG_UART_RTS_PIN)),
            config_get_i8(CONF_ITEM(KEY_CONFIG_UART_CTS_PIN))
    ));
    ESP_ERROR_CHECK(uart_driver_install(uart_port, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 0, NULL, 0));

    xTaskCreate(uart_task, "uart_task", 16384, NULL, TASK_PRIORITY_UART, NULL);
}

void uart_task(void *ctx) {
    uart_data_t *data = (uart_data_t *) malloc(sizeof(uart_data_t));
    while (true) {
        data->len = uart_read_bytes(uart_port, data->buffer, UART_BUFFER_SIZE, pdMS_TO_TICKS(50));
        if (data->len < 0) {
            ESP_LOGE(TAG, "Error reading from UART");
        } else if (data->len == 0) {
            continue;
        }

        esp_event_post(UART_EVENTS, 0, data, data->len + sizeof(data->len), portMAX_DELAY);
    }
}

int uart_log(char *buffer, size_t len) {
    if (!uart_log_forward) return 0;
    return uart_write(buffer, len);
}

int uart_nmea(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *nmea;
    nmea_vasprintf(&nmea, fmt, args);
    int l = uart_write(nmea, strlen(nmea));
    free(nmea);

    va_end(args);

    return l;
}

int uart_write(char *buffer, size_t len) {
    if (uart_port < 0) return 0;
    return uart_write_bytes(uart_port, buffer, len);
}