#ifndef _watchy_events_h
#define _watchy_events_h

#include <stdint.h>

//
// event flags get set by callback handlers (like ISR)
// get queued and then processed by the main event
// handler loop / thread
//
typedef enum {
    EV_NONE = 0,
    EV_MSEC_TICK,	// miliseconds timer tick occurred
    EV_SEC_TICK,	// seconds timer tick occurred
    EV_MIN_TICK,	// once per minute timer tick occurred
    EV_HOUR_TICK,	// once per full hour timer tick occurred
    EV_TOUCH,		// tochscreen interaction
    EV_BUTTON,		// push button press/release
    EV_ACCEL,		// accelerometer / inertial sensor
    EV_MAGNETOMETER,	// magnetometer sensor
    EV_HRM,		// heart rate monitor sensor
    EV_GNSS,		// GNSS/GPS
    EV_ATMOSPHERE,	// athmosphere sensor (pressure)
    EV_DISPLAY_TIMEOUT,	// display timeout (backlight off, disp off etc.)
    EV_POWER_CHANGE,	// ext power (dis-)connected or other power event
    EV_UPDATE_DISPLAY,	// display content changed, needs update
    EV_INFO_NOTE,	// notification or information update
    EV_BT_CONN,		// Bluetooth conection event (connect/disconnect)
    EV_BT_NUS,		// the Bluetooth NUS service has something to say
} watchy_event_t;

#define WATCHY_EVENT_QUEUE_LENGTH 8

typedef struct {
    watchy_event_t events[WATCHY_EVENT_QUEUE_LENGTH];
    uint8_t length;
    uint8_t first;
    uint8_t last;
} watchy_event_queue_t;

// add an event to the end of the queue,
// returns new queue length
uint8_t watchy_event_queue_add(watchy_event_t event);

// gets the event fomr the start of the queue
// and advances the start to the next event
watchy_event_t watchy_event_queue_get(void);

// get the current number of events in the queue
// 0 if empty
uint8_t watchy_event_queue_length(void);


#endif
