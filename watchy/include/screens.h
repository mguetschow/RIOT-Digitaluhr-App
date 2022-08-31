#ifndef _SCREENS_H
#define _SCREENS_H

#include <stdint.h>
#include <lvgl/lvgl.h>
#include <lvgl_riot.h>

#include <screen.h>

#include "main_screen.h"
#include "quick_settings_screen.h"
#include "weather_screen.h"

extern struct screen *cur_screen;

void screens_handle_event(watchy_event_t ev);
void screens_init(void);

#endif
