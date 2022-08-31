#include <stdio.h>
#include <string.h>

#include <lvgl/lvgl.h>

#include <screens.h>

#include "watchy.h"

static lv_obj_t *top_left_icons, *top_middle_icons, *top_right_icons, *clockl, *datel;

#define MAX_LABEL_LEN 128
static void update_screen(void)
{
     char label_text[MAX_LABEL_LEN];

     // date and time
     snprintf(label_text, MAX_LABEL_LEN-1, "%02d:%02d", watch_state.clock.tm_hour, watch_state.clock.tm_min);
     lv_label_set_text(clockl, label_text);
     snprintf(label_text, MAX_LABEL_LEN-1, "%d.%d.", watch_state.clock.tm_mday, watch_state.clock.tm_mon+1);
     lv_label_set_text(datel, label_text);

     // icons in the top left
     memset(label_text, 0, MAX_LABEL_LEN);
     if (watch_state.bluetooth_pwr)
       strncat(label_text, "#ffffff " LV_SYMBOL_BLUETOOTH "# ", MAX_LABEL_LEN-1);
#if 0
     else
       strncat(label_text, "#808080 " LV_SYMBOL_BLUETOOTH "# ", MAX_LABEL_LEN-1);
#endif

     if (watch_state.gnss_pwr) {
       char satnum[16];
           snprintf(satnum, 15, "%d/%d", watch_state.gnss_state.sats_in_fix, watch_state.gnss_state.sats_in_view);
           if (watch_state.gnss_state.fix_valid) {
               strncat(label_text,"#00ff00 " LV_SYMBOL_GPS "# ", MAX_LABEL_LEN-1);
           } else {
               strncat(label_text,"#ffffff " LV_SYMBOL_GPS "# ", MAX_LABEL_LEN-1);
           }
           strncat(label_text, satnum, MAX_LABEL_LEN-1);
     }
     lv_label_set_text(top_left_icons, label_text);

     // icons in the top middle
     memset(label_text, 0, MAX_LABEL_LEN);
     strncat(label_text, "#ffffff " LV_SYMBOL_WARNING "# ", MAX_LABEL_LEN-1);
     lv_label_set_text(top_middle_icons, label_text);
     

     // icons in the top right
     strcpy(label_text, watch_state.pwr_stat.charger_present ? LV_SYMBOL_CHARGE : " ");
     if (watch_state.pwr_stat.battery_percent > 80)
        strcat(label_text, LV_SYMBOL_BATTERY_FULL);
     else if (watch_state.pwr_stat.battery_percent > 60)
        strcat(label_text, LV_SYMBOL_BATTERY_3);
     else if (watch_state.pwr_stat.battery_percent > 40)
        strcat(label_text, LV_SYMBOL_BATTERY_3);
     else if (watch_state.pwr_stat.battery_percent > 20)
        strcat(label_text, LV_SYMBOL_BATTERY_1);
     else
        strcat(label_text, "#ff0000 " LV_SYMBOL_BATTERY_EMPTY "#");

     lv_label_set_text(top_right_icons, label_text);

     watchy_event_queue_add(EV_UPDATE_DISPLAY);
}

extern lv_font_t SourceSansProBold72_num_4bpp;
extern lv_font_t SourceSansProSemiBold36_num_4bpp;

static lv_obj_t *create_main_screen(void)
{
     lv_obj_t *scr=lv_obj_create(NULL);

     top_left_icons=lv_label_create(scr);
     lv_label_set_recolor(top_left_icons, true);
     lv_obj_set_style_text_font(top_left_icons, &lv_font_montserrat_12, LV_STATE_DEFAULT);
     lv_obj_set_pos(top_left_icons, 1, 1);

     top_middle_icons=lv_label_create(scr);
     lv_label_set_recolor(top_middle_icons, true);
     lv_obj_set_style_text_font(top_middle_icons, &lv_font_montserrat_12, LV_STATE_DEFAULT);
     lv_obj_set_pos(top_middle_icons, (LV_HOR_RES/2)-7, 1);

     top_right_icons=lv_label_create(scr);
     lv_obj_set_style_text_font(top_right_icons, &lv_font_montserrat_12, LV_STATE_DEFAULT);
     lv_obj_set_style_text_color(top_right_icons, lv_color_white(), LV_STATE_DEFAULT);
     lv_obj_set_pos(top_right_icons, LV_HOR_RES_MAX-24, 1);

     clockl=lv_label_create(scr);
     lv_obj_set_style_text_color(clockl, lv_color_white(), LV_STATE_DEFAULT);
     lv_obj_set_style_text_font(clockl, &SourceSansProBold72_num_4bpp, LV_STATE_DEFAULT);
     lv_obj_center(clockl);

     datel=lv_label_create(scr);
     lv_obj_set_style_text_color(datel, lv_color_white(), LV_STATE_DEFAULT);
     lv_obj_set_style_text_font(datel, &SourceSansProSemiBold36_num_4bpp, LV_STATE_DEFAULT);
     lv_obj_set_pos(datel, (LV_HOR_RES_MAX / 2)-30, (LV_VER_RES_MAX/2)+26);

     // give all dynamic elements an update
     update_screen();

     return scr;
}

static void cleanup_main_screen(void)
{
     // nothing to do here (yet)
}

static bool event_trigger(watchy_event_t event)
{
     switch(event) {
       case EV_SEC_TICK:
         if (watch_state.gnss_pwr && ((watch_state.clock.tm_sec % 10)==0)) {
           update_screen();
         }
         break;
       case EV_MIN_TICK:
       case EV_POWER_CHANGE:
         update_screen();
         break;
       default:
         return false;
         break;
     }
     return true;
}

struct screen main_screen = {
    .left = NULL, // left
    .right = NULL, // right
    .top = &quick_settings_screen, // top
    .bottom = &weather_now_screen, // bottom
    .create = &create_main_screen,
    .cleanup = &cleanup_main_screen,
    .event_trigger = &event_trigger,
};
