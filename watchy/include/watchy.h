#ifndef _watchy_h
#define _watchy_h

#include <stdint.h>
#include <time.h>

#define TM_YEAR_OFFSET      (1900)

typedef struct {
    bool charger_present;
    bool charge_complete;
    uint16_t battery_mvolt;
    uint8_t battery_percent;
} power_supply_stat_t;

typedef struct {
    bool fix_valid;
    bool time_valid;
    bool date_valid;
    uint8_t sats_in_view;
    uint8_t sats_in_fix;
    float lat;
    float lon;
    int16_t height;
    int16_t speed;
    int16_t course;
    int16_t dilution;
} gnss_state_t;

typedef enum {
    TOUCH_G_NONE = 0,
    TOUCH_G_CLICK,
    TOUCH_G_LONG_PRESS,
    TOUCH_G_SWP_UP,
    TOUCH_G_SWP_DOWN,
    TOUCH_G_SWP_LEFT,
    TOUCH_G_SWP_RIGHT,
} touch_gesture_t;

typedef enum {
    BT_OFF = 0,
    BT_ON,
    BT_CONN
} bluetooth_stat_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    bool clicked;
    touch_gesture_t gesture;
} touchscreen_state_t;

typedef struct {
    bool active;
    int16_t x;
    int16_t y;
    int16_t z;
} accel_state_t;

typedef struct {
    bool active;
    uint16_t x;
    uint16_t y;
    uint16_t z;
} magnetometer_state_t;

typedef struct {
    bool active;
} hrm_state_t;

typedef struct {
    kernel_pid_t shell_thread_pid;
    kernel_pid_t event_thread_pid;
    uint32_t rtc_time;
    struct tm clock;			// the clock - it's a watch!
    uint8_t display_timeout;		// how long until sleep display
    power_supply_stat_t pwr_stat;	// current power supply state
    bool gnss_pwr;			// if GNSS is powered up or not
    bluetooth_stat_t bluetooth_pwr;			// if Bluetooth is powered
    gnss_state_t gnss_state;		// GNSS/GPS sensor state
    touchscreen_state_t touch_state;	// touchscreen state (last x/y etc.)
    accel_state_t accel_state;
    magnetometer_state_t magnetometer_state;
    hrm_state_t hrm_state;
    bool notification;			// toggles '!' icon in top row
    char info[64];			// two line info text on bottom of main screen
} watchy_state_t;

extern watchy_state_t watch_state;

#endif
