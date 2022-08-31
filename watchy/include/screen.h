#ifndef _SCREEN_H
#define _SCREEN_H

#include <watchy.h>
#include <watchy_events.h>

struct screen {
    // the screens next to this one
    struct screen *left;   // screen left of this screen
    struct screen *right;  // screen right of this screen
    struct screen *top;    // screen to the top of this screen
    struct screen *bottom; // screen to the bottom of this screen
    // handle to this screen
    lv_obj_t *scr;
    // function to create the screen
    lv_obj_t *(*create)(void);
    // function to clean up screen internal things, called before screen is switched
    void (*cleanup)(void);
    bool (*event_trigger)(watchy_event_t event);
};

#endif
