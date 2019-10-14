#ifndef ESP32_XBEE_STATUS_LED_H
#define ESP32_XBEE_STATUS_LED_H

typedef enum {
    STATUS_LED_STATIC = 0,
    STATUS_LED_FADE,
    STATUS_LED_BLINK
} status_led_flashing_mode_t;

typedef struct status_led_color_t *status_led_handle_t;
struct status_led_color_t {
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    status_led_flashing_mode_t flashing_mode;
    uint32_t interval;
    uint32_t duration;
    uint8_t expire;

    bool remove;
    bool active;

    status_led_handle_t prev;
    status_led_handle_t next;
};

status_led_handle_t status_led_add(uint32_t rgba, status_led_flashing_mode_t flashing_mode, uint32_t interval, uint32_t duration, uint8_t expire);
void status_led_remove(status_led_handle_t color);
void status_led_init();

#endif //ESP32_XBEE_STATUS_LED_H