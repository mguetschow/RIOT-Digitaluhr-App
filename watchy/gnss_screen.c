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


static lv_obj_t *sats_l;
static lv_obj_t *lat_l;
static lv_obj_t *lon_l;
static lv_obj_t *height_l;
static lv_obj_t *speed_l;
static lv_obj_t *course_l;
static lv_obj_t *dilution_l;

#define LABEL_HEIGHT 20
#define LABEL_OFFSET_Y 20
#define LABEL_OFFSET_X 80

static void update_gnss(void)
{
    char lstr[32];

    snprintf(lstr, 31, "%s %d/%d# ", watch_state.gnss_state.fix_valid ? "#00ff00" : "#ffffff", watch_state.gnss_state.sats_in_fix, watch_state.gnss_state.sats_in_view);
    lv_label_set_text(sats_l, lstr);

    snprintf(lstr, 31, "%4.4f", watch_state.gnss_state.lat);
    lv_label_set_text(lat_l, lstr);

    snprintf(lstr, 31, "%4.4f", watch_state.gnss_state.lon);
    lv_label_set_text(lon_l, lstr);

    snprintf(lstr, 31, "%d", watch_state.gnss_state.height);
    lv_label_set_text(height_l, lstr);

    snprintf(lstr, 31, "%d", watch_state.gnss_state.speed);
    lv_label_set_text(speed_l, lstr);

    snprintf(lstr, 31, "%d", watch_state.gnss_state.course);
    lv_label_set_text(course_l, lstr);

    snprintf(lstr, 31, "%d", watch_state.gnss_state.dilution);
    lv_label_set_text(dilution_l, lstr);

    watchy_event_queue_add(EV_UPDATE_DISPLAY);
}

static lv_obj_t *create_gnss_screen(void)
{
    lv_obj_t *lv_gnss_screen;
    lv_obj_t *tl;

    lv_gnss_screen = lv_obj_create(NULL);

    tl=lv_label_create(lv_gnss_screen);
    lv_obj_set_style_text_color(tl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(tl, 0, LABEL_OFFSET_Y + 0 * LABEL_HEIGHT);
    lv_label_set_text(tl, "Sats:");

    sats_l=lv_label_create(lv_gnss_screen);
    lv_label_set_recolor(sats_l, true);
    lv_obj_set_style_text_color(sats_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(sats_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(sats_l, LABEL_OFFSET_X, LABEL_OFFSET_Y + 0 * LABEL_HEIGHT);

    tl=lv_label_create(lv_gnss_screen);
    lv_obj_set_style_text_color(tl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(tl, 0, LABEL_OFFSET_Y + 1 * LABEL_HEIGHT);
    lv_label_set_text(tl, "Lat:");

    lat_l=lv_label_create(lv_gnss_screen);
    // lv_label_set_recolor(lat_l, true);
    lv_obj_set_style_text_color(lat_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lat_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(lat_l, LABEL_OFFSET_X, LABEL_OFFSET_Y + 1 * LABEL_HEIGHT);

    tl=lv_label_create(lv_gnss_screen);
    lv_obj_set_style_text_color(tl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(tl, 0, LABEL_OFFSET_Y + 2 * LABEL_HEIGHT);
    lv_label_set_text(tl, "Lon:");

    lon_l=lv_label_create(lv_gnss_screen);
    // lv_label_set_recolor(lon_l, true);
    lv_obj_set_style_text_color(lon_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lon_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(lon_l, LABEL_OFFSET_X, LABEL_OFFSET_Y + 2 * LABEL_HEIGHT);

    tl=lv_label_create(lv_gnss_screen);
    lv_obj_set_style_text_color(tl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(tl, 0, LABEL_OFFSET_Y + 3 * LABEL_HEIGHT);
    lv_label_set_text(tl, "Height:");

    height_l=lv_label_create(lv_gnss_screen);
    // lv_label_set_recolor(height_l, true);
    lv_obj_set_style_text_color(height_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(height_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(height_l, LABEL_OFFSET_X, LABEL_OFFSET_Y + 3 * LABEL_HEIGHT);

    tl=lv_label_create(lv_gnss_screen);
    lv_obj_set_style_text_color(tl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(tl, 0, LABEL_OFFSET_Y + 4 * LABEL_HEIGHT);
    lv_label_set_text(tl, "Speed:");

    speed_l=lv_label_create(lv_gnss_screen);
    // lv_label_set_recolor(speed_l, true);
    lv_obj_set_style_text_color(speed_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(speed_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(speed_l, LABEL_OFFSET_X, LABEL_OFFSET_Y + 4 * LABEL_HEIGHT);

    tl=lv_label_create(lv_gnss_screen);
    lv_obj_set_style_text_color(tl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(tl, 0, LABEL_OFFSET_Y + 5 * LABEL_HEIGHT);
    lv_label_set_text(tl, "Course:");

    course_l=lv_label_create(lv_gnss_screen);
    // lv_label_set_recolor(course_l, true);
    lv_obj_set_style_text_color(course_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(course_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(course_l, LABEL_OFFSET_X, LABEL_OFFSET_Y + 5 * LABEL_HEIGHT);

    tl=lv_label_create(lv_gnss_screen);
    lv_obj_set_style_text_color(tl, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(tl, 0, LABEL_OFFSET_Y + 6 * LABEL_HEIGHT);
    lv_label_set_text(tl, "Dilution:");

    dilution_l=lv_label_create(lv_gnss_screen);
    // lv_label_set_recolor(dilution_l, true);
    lv_obj_set_style_text_color(dilution_l, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(dilution_l, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(dilution_l, LABEL_OFFSET_X, LABEL_OFFSET_Y + 6 * LABEL_HEIGHT);


    update_gnss();

    return lv_gnss_screen;
}

static bool gnss_event_trigger(watchy_event_t event)
{
    switch(event) {
        case EV_GNSS:
            update_gnss();
            break;
        default:
            return false;
            break;
    };

    return true;
}

struct screen gnss_screen = {
    .left = NULL, // left
    .right = NULL, // right
    .top = &weather_now_screen, // top
    .bottom = NULL, // bottom
    .create = &create_gnss_screen,
    .cleanup = NULL,
    .event_trigger = &gnss_event_trigger,
};
