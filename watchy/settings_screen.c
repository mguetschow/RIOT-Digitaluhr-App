#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <board.h>
#include <periph/pwm.h>
#include <string.h>
#include <time.h>
#include <byteorder.h>

#include <lvgl/lvgl.h>

#include <screens.h>

#include "watchy.h"


#if 0
static char tz = +1;
static bool dst = true;
#endif

static void okbtn_event_handler(lv_event_t * e)
{
    (void)e;

    cur_screen = &main_screen;
    cur_screen->scr = cur_screen->create();
    lv_scr_load_anim(cur_screen->scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
    watchy_event_queue_add(EV_UPDATE_DISPLAY);
}

static void dstbx_event_handler(lv_event_t * e)
{
    lv_obj_t *chkbx = (lv_obj_t *)lv_event_get_user_data(e);

    if (lv_obj_get_state(chkbx) & LV_STATE_CHECKED) {
        watch_state.clock.tm_isdst = true;
    } else {
        watch_state.clock.tm_isdst = false;
    }
}

static void lvlpbtn_event_handler(lv_event_t * e)
{
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    char lstr[6];

    if (watch_state.display_brightness < 100) {
        watch_state.display_brightness += 10;
        snprintf(lstr, 6, "%d", watch_state.display_brightness);
        lv_label_set_text(lbl, lstr);
        pwm_set(PWM_DEV(0), 0, watch_state.display_brightness);
    }
}

static void lvlnbtn_event_handler(lv_event_t * e)
{
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    char lstr[6];

    if (watch_state.display_brightness >= 10) {
        watch_state.display_brightness -= 10;
        snprintf(lstr, 6, "%d", watch_state.display_brightness);
        lv_label_set_text(lbl, lstr);
        pwm_set(PWM_DEV(0), 0, watch_state.display_brightness);
    }
}

static void tzpbtn_event_handler(lv_event_t * e)
{
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    char lstr[6];

    if (watch_state.timez < 12) {
        watch_state.timez += 1;
        snprintf(lstr, 6, "%+d", watch_state.timez);
        lv_label_set_text(lbl, lstr);
    }
}

static void tznbtn_event_handler(lv_event_t * e)
{
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    char lstr[6];

    if (watch_state.timez > -12) {
        watch_state.timez -= 1;
        snprintf(lstr, 6, "%+d", watch_state.timez);
        lv_label_set_text(lbl, lstr);
    }
}

lv_obj_t *create_settings_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *lvl_p, *lvl_n, *tz_p, *tz_n;
    lv_obj_t *llbl, *tzlbl, *dstbx;
    lv_obj_t *okbtn, *tobj;
    char lstr[6];

    tobj = lv_label_create(scr);
    lv_obj_set_style_text_font(tobj, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_pos(tobj, 1, 0);
    lv_label_set_text(tobj, LV_SYMBOL_EYE_OPEN);

    llbl = lv_label_create(scr);
    lv_obj_set_style_text_font(llbl, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    snprintf(lstr, 6, "%d", watch_state.display_brightness);
    lv_label_set_text(llbl, lstr);
    lv_obj_set_pos(llbl, 85, 3);

    lvl_p = lv_btn_create(scr);
    tobj = lv_label_create(lvl_p);
    lv_obj_set_style_text_font(tobj, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_label_set_text(tobj, LV_SYMBOL_PLUS);
    lv_obj_center(tobj);
    lv_obj_set_size(lvl_p, 35, 35);
    lv_obj_set_pos(lvl_p, 135, 0);
    lv_obj_add_event_cb(lvl_p, lvlpbtn_event_handler, LV_EVENT_CLICKED, llbl);

    lvl_n = lv_btn_create(scr);
    tobj = lv_label_create(lvl_n);
    lv_obj_set_style_text_font(tobj, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_label_set_text(tobj, LV_SYMBOL_MINUS);
    lv_obj_center(tobj);
    lv_obj_set_size(lvl_n, 35, 35);
    lv_obj_set_pos(lvl_n, 40,0);
    lv_obj_add_event_cb(lvl_n, lvlnbtn_event_handler, LV_EVENT_CLICKED, llbl);

    tobj = lv_label_create(scr);
    lv_obj_set_style_text_font(tobj, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_pos(tobj, 1, 50);
    lv_label_set_text(tobj, "TZ");

    tzlbl = lv_label_create(scr);
    lv_obj_set_style_text_font(tzlbl, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    snprintf(lstr, 6, "%+d", watch_state.timez);
    lv_label_set_text(tzlbl, lstr);
    lv_obj_set_pos(tzlbl, 85, 53);

    tz_p = lv_btn_create(scr);
    tobj = lv_label_create(tz_p);
    lv_obj_set_style_text_font(tobj, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_label_set_text(tobj, LV_SYMBOL_PLUS);
    lv_obj_center(tobj);
    lv_obj_set_size(tz_p, 35, 35);
    lv_obj_set_pos(tz_p, 135, 50);
    lv_obj_add_event_cb(tz_p, tzpbtn_event_handler, LV_EVENT_CLICKED, tzlbl);

    tz_n = lv_btn_create(scr);
    tobj = lv_label_create(tz_n);
    lv_obj_set_style_text_font(tobj, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_label_set_text(tobj, LV_SYMBOL_MINUS);
    lv_obj_center(tobj);
    lv_obj_set_size(tz_n, 35, 35);
    lv_obj_set_pos(tz_n, 40, 50);
    lv_obj_add_event_cb(tz_n, tznbtn_event_handler, LV_EVENT_CLICKED, tzlbl);

#if 0
    tobj = lv_label_create(scr);
    lv_obj_set_pos(tobj, 80, 100);
    lv_label_set_text(tobj, "DST");
#endif
    dstbx = lv_checkbox_create(scr);
    lv_obj_set_style_text_color(dstbx, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(dstbx, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_checkbox_set_text(dstbx, "DST");
    if (watch_state.clock.tm_isdst)
        lv_obj_add_state(dstbx, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(dstbx, LV_STATE_CHECKED);
    lv_obj_set_size(dstbx, 35, 35);
    lv_obj_set_pos(dstbx, 42, 100);
    lv_obj_add_event_cb(dstbx, dstbx_event_handler, LV_EVENT_CLICKED, dstbx);

#if 0
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_palette_main(LV_PALETTE_GREEN));
#endif
    okbtn = lv_btn_create(scr);
    //lv_obj_add_style(okbtn, &style_btn, 0);
    tobj = lv_label_create(okbtn);
    lv_obj_set_style_text_font(tobj, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_label_set_recolor(tobj, true);
    lv_label_set_text(tobj, "#00ff00 " LV_SYMBOL_OK "# ");
    lv_obj_center(tobj);
    lv_obj_set_size(okbtn, 35, 35);
    lv_obj_set_pos(okbtn, 140, 140);
    lv_obj_add_event_cb(okbtn, okbtn_event_handler, LV_EVENT_CLICKED, NULL);

    return scr;
}

static bool settings_event_trigger(watchy_event_t event)
{
    (void)event;

    return false;
}

struct screen settings_screen = {
    .left = NULL, // left
    .right = NULL, // right
    .top = NULL, // top
    .bottom = NULL, // bottom
    .create = &create_settings_screen,
    .cleanup = NULL,
    .event_trigger = &settings_event_trigger,
};
