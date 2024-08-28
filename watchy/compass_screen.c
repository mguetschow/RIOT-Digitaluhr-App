#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <string.h>
#include <time.h>
#include <byteorder.h>

#include <lvgl/lvgl.h>

#include <screens.h>

#include "watchy.h"
#include "magneto.h"


static lv_obj_t *course_l;
static lv_obj_t *course_deg_l;
static lv_obj_t *calibrate_b;


static void update_compass(void)
{
    char lstr[32];
    int16_t c=watch_state.magnetometer_state.course;
    char *dir;

    snprintf(lstr, 7, "%d", c);
    lv_label_set_text(course_deg_l, lstr);

    dir = "NaN";
    c -= 338;

    if (c < 0)
        c = 360 + c;

    if (c < 45)
        dir ="N";
    else if (c < 90)
        dir = "NE";
    else if (c < 135)
        dir = "E";
    else if (c < 180)
        dir = "SE";
    else if (c < 225)
        dir = "S";
    else if (c < 270)
        dir = "SW";
    else if (c < 315)
        dir = "W";
    else if (c < 360)
        dir = "NW";

    //snprintf(lstr, 3, "%s", dir);
    lv_label_set_text(course_l, dir);

    watchy_event_queue_add(EV_UPDATE_DISPLAY);
}

static void calibrate_event_handler(lv_event_t * e)
{
    lv_obj_t *chkbx = (lv_obj_t *)lv_event_get_user_data(e);

    if (lv_obj_get_state(chkbx) & LV_STATE_CHECKED) {
        magneto_calibrate(true);
    } else {
        magneto_calibrate(false);
    }
}


extern lv_font_t SourceSansProBold72_num_4bpp;
static lv_obj_t *create_compass_screen(void)
{
    lv_obj_t *lv_compass_screen;
    //lv_obj_t *tl;

    lv_compass_screen = lv_obj_create(NULL);

    course_l=lv_label_create(lv_compass_screen);
    lv_obj_set_style_text_color(course_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(course_l, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_pos(course_l, (LV_HOR_RES_MAX/2)-15, (LV_VER_RES_MAX/2)-40);
    //lv_obj_center(course_l);

    course_deg_l=lv_label_create(lv_compass_screen);
    lv_obj_set_style_text_color(course_deg_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(course_deg_l, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_pos(course_deg_l, (LV_HOR_RES_MAX/2)-25, (LV_VER_RES_MAX/2));
    //lv_obj_center(course_deg_l);

    calibrate_b = lv_checkbox_create(lv_compass_screen);
    lv_obj_set_style_text_color(calibrate_b, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(calibrate_b, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_checkbox_set_text(calibrate_b, "CAL");
    if (watch_state.magnetometer_state.calibrate)
        lv_obj_add_state(calibrate_b, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(calibrate_b, LV_STATE_CHECKED);
    lv_obj_set_size(calibrate_b, 35, 35);
    lv_obj_set_pos(calibrate_b, 22, 120);
    lv_obj_add_event_cb(calibrate_b, calibrate_event_handler, LV_EVENT_CLICKED, calibrate_b);

    update_compass();

    watch_state.magnetometer_state.active = true;
    watchy_request_ms_event();

    return lv_compass_screen;
}

static void cleanup_compass_screen(void)
{
    watch_state.magnetometer_state.active = false;
    watchy_release_ms_event();
}

static bool compass_event_trigger(watchy_event_t event)
{
    switch(event) {
        case EV_SEC10_TICK:
            update_compass();
            break;
        default:
            return false;
            break;
    };

    return true;
}

struct screen compass_screen = {
    .left = &gnss_screen, // left
    .right = NULL, // right
    .top = &weather_now_screen, // top
    .bottom = NULL, // bottom
    .create = &create_compass_screen,
    .cleanup = &cleanup_compass_screen,
    .event_trigger = &compass_event_trigger,
};
