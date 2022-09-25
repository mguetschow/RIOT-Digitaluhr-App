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
#include "weatherstation.h"


static lv_meter_indicator_t * indic;
static lv_obj_t *meter;
static lv_obj_t *tlabel;
static lv_obj_t *plabel;

static void update_weather_now(void)
{
    struct weatherpoint *wp;
    char lstr[32];

    wp=weather_get_current();
    // lv_meter_set_indicator_value(meter, indic, (wp->pressure/100)+(230/10));
    lv_meter_set_indicator_value(meter, indic, (wp->pressure/100)+(watch_state.gnss_state.height/10));

    snprintf(lstr, 31, "#000000 %d.%d°C# ", wp->temp/100, (wp->temp%100)/10);
    lv_label_set_text(tlabel, lstr);

    snprintf(lstr, 31, "#000000 %ld.%ld hPa# ", wp->pressure/100, (wp->pressure%100)/10);
    lv_label_set_text(plabel, lstr);

    watchy_event_queue_add(EV_UPDATE_DISPLAY);
}

static lv_obj_t *create_weather_now_screen(void)
{
    lv_obj_t *lv_weather_now_screen;

    lv_weather_now_screen = lv_obj_create(NULL);

    meter = lv_meter_create(lv_weather_now_screen);
    lv_obj_center(meter);
    lv_obj_set_size(meter, 175, 175);

    /*Add a scale first*/
    lv_meter_scale_t * scale = lv_meter_add_scale(meter);
    //lv_meter_set_scale_ticks(meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_ticks(meter, scale, 3, 2, 10, lv_color_black());
    //lv_meter_set_scale_major_ticks(meter, scale, 8, 4, 15, lv_color_black(), 10);

#if 0
    /*Add a blue arc to the start*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /*Make the tick lines blue at the start of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 0);
    lv_meter_set_indicator_end_value(meter, indic, 20);

    /*Add a red arc to the end*/
    indic = lv_meter_add_arc(meter, scale, 3, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);

    /*Make the tick lines red at the end of the scale*/
    indic = lv_meter_add_scale_lines(meter, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false, 0);
    lv_meter_set_indicator_start_value(meter, indic, 80);
    lv_meter_set_indicator_end_value(meter, indic, 100);
#endif

    // a scale, total angle 300°, 120° turned
    // top center 1013.25 hPa, standard pressure at sea level
    #define PR_STD 1013
    // we define a +/- range the scale shall cover
    #define PR_RANGE 20
    // lv_meter_set_scale_range(meter, scale, PR_STD-PR_RANGE, PR_STD+PR_RANGE, 300, 120);
    lv_meter_set_scale_range(meter, scale, PR_STD-PR_RANGE, PR_STD+PR_RANGE, 180, 180);
    /*Add a needle line indicator*/
    indic = lv_meter_add_needle_line(meter, scale, 4, lv_color_black(), -10);

    plabel = lv_label_create(lv_weather_now_screen);
    lv_label_set_recolor(plabel, true);
    lv_obj_set_style_text_font(plabel, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    lv_obj_set_pos(plabel, (176/2)-36, 105);

    tlabel = lv_label_create(lv_weather_now_screen);
    lv_label_set_recolor(tlabel, true);
    lv_obj_set_style_text_font(tlabel, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_pos(tlabel, (176/2)-35, 125);

    update_weather_now();

    return lv_weather_now_screen;
}

static lv_obj_t *create_weather_24_screen(void)
{
    lv_obj_t *lv_weather_24_screen;
    lv_obj_t *label;

    lv_weather_24_screen = lv_obj_create(NULL);

    label = lv_label_create(lv_weather_24_screen);
    lv_label_set_recolor(label, true);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_pos(label, 10, 90);
    lv_label_set_text(label, "#ffffff To be done.# ");

    return lv_weather_24_screen;
}

static bool weather_now_event_trigger(watchy_event_t event)
{
    if (event  == EV_MIN_TICK)
      update_weather_now();

    return false;
}

static bool weather_24_event_trigger(watchy_event_t event)
{
    if (event  == EV_MIN_TICK) {
      
    }
    return false;
}

struct screen weather_now_screen = {
    .left = NULL, // left
    .right = &weather_24_screen, // right
    .top = &main_screen, // top
    .bottom = &gnss_screen, // bottom
    .create = &create_weather_now_screen,
    .cleanup = NULL,
    .event_trigger = &weather_now_event_trigger,
};

struct screen weather_24_screen = {
    .left = &weather_now_screen, // left
    .right = NULL, // right
    .top = &main_screen, // top
    .bottom = &gnss_screen, // bottom
    .create = &create_weather_24_screen,
    .cleanup = NULL,
    .event_trigger = &weather_24_event_trigger,
};

