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
    TOUCH_G_SWP_UP,
    TOUCH_G_SWP_DOWN,
    TOUCH_G_SWP_LEFT,
    TOUCH_G_SWP_RIGHT,
} touch_gesture_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    bool clicked;
    touch_gesture_t gesture;
} touchscreen_state_t;

typedef struct {
    struct tm clock;			// the clock - it's a watch!
    uint8_t display_timeout;		// how long until sleep display
    power_supply_stat_t pwr_stat;	// current power supply state
    bool gnss_pwr;			// if GNSS is powered up or not
    bool bluetooth_pwr;			// if Bluetooth is powered
    gnss_state_t gnss_state;		// GNSS/GPS sensor state
    touchscreen_state_t touch_state;	// touchscreen state (last x/y etc.)
} watchy_state_t;

#endif
