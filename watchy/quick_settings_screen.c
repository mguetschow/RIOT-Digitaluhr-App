#include <stdio.h>
#include <string.h>

#include <lvgl/lvgl.h>
#include <nimble_autoadv.h>

#define ENABLE_DEBUG 0
#include "debug.h"

#include <screens.h>

#include "watchy.h"

extern lv_font_t SourceSansProSemiBold14_4bpp;

static void msgbox_event_cb(lv_event_t * e)
{
	lv_obj_t * obj = lv_event_get_current_target(e);
	lv_obj_t *mbox = lv_event_get_user_data(e);

	DEBUG("Button %s clicked\n", lv_msgbox_get_active_btn_text(obj));

	if (strcmp("OK", lv_msgbox_get_active_btn_text(obj))==0)
		board_power_off();

	lv_msgbox_close(mbox);
}

void power_off_dialog(lv_obj_t *par)
{
	static const char * btns[] = {"#000000 OK# ", "#000000 Cancel# ", ""};
    lv_obj_t *mbox1 = lv_msgbox_create(par, "Power Off", "Are you sure?", btns, false);
	lv_obj_t *btns_o;

	lv_obj_set_style_bg_color(mbox1, lv_color_black(), LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(mbox1, &lv_font_montserrat_16, LV_STATE_DEFAULT);

	btns_o = lv_msgbox_get_btns(mbox1);
	lv_btnmatrix_set_btn_ctrl_all(btns_o, LV_BTNMATRIX_CTRL_RECOLOR);
	lv_obj_set_style_text_color(btns_o, lv_color_black(), LV_STATE_DEFAULT);

    lv_obj_add_event_cb(mbox1, msgbox_event_cb, LV_EVENT_VALUE_CHANGED, mbox1);
    lv_obj_center(mbox1);
}

void gnss_power_control(bool pwr);

static void settings_button_handler(lv_event_t * e)
{
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t * obj = lv_event_get_target(e);

        if (code == LV_EVENT_VALUE_CHANGED) {
                uint16_t id = lv_btnmatrix_get_selected_btn(obj);
                const char *txt = lv_btnmatrix_get_btn_text(obj, id);

                DEBUG("%d: '%s' was pressed\n", id, txt);
                switch (id) {
                        case 0:
                                if (watch_state.bluetooth_pwr == BT_OFF) {
                                  watch_state.bluetooth_pwr = BT_ON;
                                  nimble_autoadv_start(NULL);
                                } else {
                                  nimble_autoadv_stop();
                                  watch_state.bluetooth_pwr = BT_OFF;
                                }
                                break;
                        case 1:
                                watch_state.gnss_pwr = !watch_state.gnss_pwr;
                                gnss_power_control(watch_state.gnss_pwr);
                                break;
                        case 2:
                                break;
                        case 3:
                                power_off_dialog(lv_obj_get_parent(obj));
                                break;
                        default:
                                break;
                }
        }
}

static const char *btnm_map[] = {LV_SYMBOL_BLUETOOTH, LV_SYMBOL_GPS, "\n",
                                  LV_SYMBOL_SETTINGS, LV_SYMBOL_POWER, ""};

static lv_obj_t *create_quick_settings_screen(void)
{
    lv_obj_t *second_screen;
    //lv_obj_t *label;

    second_screen = lv_obj_create(NULL);

    lv_obj_t *btnm1 = lv_btnmatrix_create(second_screen);
    lv_obj_set_size(btnm1, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    lv_obj_set_style_bg_color(btnm1, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnm1, lv_color_make(0,0,0xff), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnm1, lv_color_make(0,0xff,0), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(btnm1, &lv_font_montserrat_14, LV_STATE_DEFAULT);
//    lv_obj_set_style_text_font(btnm1, &lv_font_montserrat_36, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btnm1, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_btnmatrix_set_map(btnm1, btnm_map);

    lv_btnmatrix_set_btn_ctrl(btnm1, 0, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(btnm1, 0, watch_state.bluetooth_pwr ? LV_BTNMATRIX_CTRL_CHECKED : 0);

    lv_btnmatrix_set_btn_ctrl(btnm1, 1, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(btnm1, 1, watch_state.gnss_pwr ? LV_BTNMATRIX_CTRL_CHECKED : 0);

    lv_obj_align(btnm1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btnm1, settings_button_handler, LV_EVENT_ALL, NULL);

    return second_screen;
}

static bool event_trigger(watchy_event_t event)
{
    (void)event;
    return false;
}

struct screen quick_settings_screen = {
    .left = NULL, // left
    .right = NULL, // right
    .top = NULL, // top
    .bottom = &main_screen, // bottom
    // .scr = NULL, // this scr
    .create = &create_quick_settings_screen,
    .cleanup = NULL,
    .event_trigger = &event_trigger,
};
