#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <string.h>

#include <lvgl/lvgl.h>

#define ENABLE_DEBUG 0
#include <debug.h>

#include <screens.h>

#define SCREEN_TRANSITION_TIME	500 //300

struct screen *cur_screen=NULL;

static void screens_handle_input_event(watchy_event_t event)
{
    struct screen *prev_scr;

    if (cur_screen==NULL || watch_state.touch_state.gesture==TOUCH_G_NONE)
      return;

    prev_scr = cur_screen;

    if (event == EV_TOUCH) {
      DEBUG("scr iev %d\n", watch_state.touch_state.gesture);
      switch (watch_state.touch_state.gesture) {
        case TOUCH_G_SWP_DOWN:
          // swipe down reveals screen from the top
          if (cur_screen->top!=NULL) {
            cur_screen=cur_screen->top;
            cur_screen->scr=cur_screen->create();
            lv_scr_load_anim(cur_screen->scr, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, SCREEN_TRANSITION_TIME, 0, true);
            watchy_event_queue_add(EV_UPDATE_DISPLAY);
          }
          break;

        case TOUCH_G_SWP_UP:
          // swipe up reveals screen from the bottom
          if (cur_screen->bottom!=NULL) {
            cur_screen=cur_screen->bottom;
            cur_screen->scr=cur_screen->create();
            lv_scr_load_anim(cur_screen->scr, LV_SCR_LOAD_ANIM_MOVE_TOP, SCREEN_TRANSITION_TIME, 0, true);
            watchy_event_queue_add(EV_UPDATE_DISPLAY);
          }
          break;

        case TOUCH_G_SWP_LEFT:
          // swipe left reveals screen from the right
          if (cur_screen->right!=NULL) {
            cur_screen=cur_screen->right;
            cur_screen->scr=cur_screen->create();
            lv_scr_load_anim(cur_screen->scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, SCREEN_TRANSITION_TIME, 0, true);
            watchy_event_queue_add(EV_UPDATE_DISPLAY);
          }
          break;

        case TOUCH_G_SWP_RIGHT:
          // swipe right reveals screen from the left
          if (cur_screen->left!=NULL) {
            cur_screen=cur_screen->left;
            cur_screen->scr=cur_screen->create();
            lv_scr_load_anim(cur_screen->scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, SCREEN_TRANSITION_TIME, 0, true);
            watchy_event_queue_add(EV_UPDATE_DISPLAY);
          }
          break;

        default:
          if (cur_screen != NULL && cur_screen->event_trigger != NULL)
            cur_screen->event_trigger(event);
          break;
      };
    } else if (event == EV_BUTTON) {
      cur_screen=&main_screen;
      cur_screen->scr=cur_screen->create();
      lv_scr_load_anim(cur_screen->scr, LV_SCR_LOAD_ANIM_NONE, SCREEN_TRANSITION_TIME, 0, true);
      watchy_event_queue_add(EV_UPDATE_DISPLAY);
    }
    if (cur_screen != prev_scr) {
      if (prev_scr->cleanup != NULL)
        prev_scr->cleanup();
    }
}

void screens_handle_event(watchy_event_t ev)
{
  // first check for swipe events which transition screens
  // everything else gets pushed to the screen itself
  if (ev == EV_TOUCH || ev == EV_BUTTON)
      screens_handle_input_event(ev);
  else
      if (cur_screen != NULL && cur_screen->event_trigger != NULL)
          cur_screen->event_trigger(ev);
}

void screens_init(void)
{
  cur_screen = &main_screen;
  cur_screen->scr = cur_screen->create();
  lv_disp_load_scr(cur_screen->scr);
  watchy_event_queue_add(EV_UPDATE_DISPLAY);
}
