#ifndef _watchy_h
#define _watchy_h

#include <stdint.h>
#include <time.h>

#define TM_YEAR_OFFSET      (1900)

#define ALERT_TYPE_SIMPLE	0x00
#define ALERT_TYPE_EMAIL	0x01
#define ALERT_TYPE_NEWS		0x02
#define ALERT_TYPE_CALL		0x03
#define ALERT_TYPE_MISSED_CALL	0x04
#define ALERT_TYPE_SMS_MMS	0x05
#define ALERT_TYPE_VOICE_MAIL	0x06
#define ALERT_TYPE_SCHEDULE	0x07
#define ALERT_TYPE_HIGH_PRIORITY	0x08
#define ALERT_TYPE_INSTANT_MESSAGE	0x09
#define ALERT_TYPE_SERVICE_SPECIFIC_1	0xfb
#define ALERT_TYPE_SERVICE_SPECIFIC_2	0xfc
#define ALERT_TYPE_SERVICE_SPECIFIC_3	0xfd
#define ALERT_TYPE_SERVICE_SPECIFIC_4	0xfe
#define ALERT_TYPE_SERVICE_SPECIFIC_5	0xff

typedef struct {
    struct tm when;
    uint8_t type;
    uint8_t num_new;
    char *text;
} alert_t;

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
	bool calibrate;
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t course;
} magnetometer_state_t;

typedef struct {
	bool active;
} hrm_state_t;

typedef enum {
	VIB_NONE = 0,
	VIB_SHORT = 2,
	VIB_MID = 4,
	VIB_LONG = 6,
} vib_pattern_t;

typedef struct {
	bool active;
	uint8_t level; // 0-100
	uint8_t duration; // 10ths of seconds, max. 25.5 sec
	vib_pattern_t pattern;
} vib_state_t;

typedef struct {
	kernel_pid_t shell_thread_pid;
	kernel_pid_t event_thread_pid;
	uint32_t rtc_time;					// seconds since epoch, the main RTC counter
	struct tm clock;					// the clock - it's a watch!
	int8_t timez;						// time zone offset in hours from UTC
	uint8_t display_timeout;			// how long until sleep (backlight) display
	uint8_t display_lock_timeout;			// how long until sleep (backlight) display
	uint8_t display_brightness;			// display brightness level, 0-100
	uint8_t ms_event_use_count;
	power_supply_stat_t pwr_stat;		// current power supply state
	bool gnss_pwr;						// if GNSS is powered up or not
	bluetooth_stat_t bluetooth_pwr;			// if Bluetooth is powered
	gnss_state_t gnss_state;				// GNSS/GPS sensor state
	touchscreen_state_t touch_state;		// touchscreen state (last x/y etc.)
	accel_state_t accel_state;
	magnetometer_state_t magnetometer_state;
	hrm_state_t hrm_state;
	vib_state_t vib_state;
	bool notification;					// toggles '!' icon in top row
	char info1[32];						// two line info text on bottom of main screen
	char info2[32];
} watchy_state_t;

extern watchy_state_t watch_state;

uint8_t watchy_request_ms_event(void);
uint8_t watchy_release_ms_event(void);
#endif
