#include "watchy_events.h"

static watchy_event_queue_t ev_queue = { .length=0, .first=0, .last=0 };

uint8_t watchy_event_queue_add(watchy_event_t event)
{
    ev_queue.events[ev_queue.last] = event;
    ev_queue.last++;
    ev_queue.last %= WATCHY_EVENT_QUEUE_LENGTH;
    ev_queue.length++;

    return ev_queue.length;
}

watchy_event_t watchy_event_queue_get(void)
{
    watchy_event_t ev;

    if (ev_queue.length == 0)
        return EV_NONE;

    ev = ev_queue.events[ev_queue.first];
    ev_queue.first++;
    ev_queue.first %= WATCHY_EVENT_QUEUE_LENGTH;
    ev_queue.length--;

    return ev;
}

uint8_t watchy_event_queue_length(void)
{
    return ev_queue.length;
}
