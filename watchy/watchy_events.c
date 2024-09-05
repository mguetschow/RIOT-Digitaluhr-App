#include <thread.h>
#include "watchy.h"
#include "watchy_events.h"

static watchy_event_queue_t ev_queue = { .length=0, .first=0, .last=0 };

// will add an event to the event queue and return the new queue length
uint8_t watchy_event_queue_add(watchy_event_t event)
{
    // if the queue is full, bail out
    if (ev_queue.length == WATCHY_EVENT_QUEUE_LENGTH)
        return ev_queue.length;

    // for debouncing and preventing overruns,
    // if this event type is already in the queue, dont't queue it again
    for (uint8_t i=0; i<ev_queue.length; i++) {
        if (ev_queue.events[(ev_queue.first+i) % WATCHY_EVENT_QUEUE_LENGTH] == event)
            return ev_queue.length;
    }

    ev_queue.events[ev_queue.last] = event;
    ev_queue.last++;
    ev_queue.last %= WATCHY_EVENT_QUEUE_LENGTH;
    ev_queue.length++;

    thread_wakeup(watch_state.event_thread_pid);

    return ev_queue.length;
}

// will return the next event fro the queue and remove it from the queue
// or returns EV_NONE if there isn't one in the queue anymore
watchy_event_t watchy_event_queue_get(void)
{
    watchy_event_t ev;

    if (ev_queue.length == 0)
        return EV_NONE;

    ev = ev_queue.events[ev_queue.first];
    ev_queue.events[ev_queue.first] = EV_NONE;
    ev_queue.first++;
    ev_queue.first %= WATCHY_EVENT_QUEUE_LENGTH;
    ev_queue.length--;

    return ev;
}

// returns the event queue length, does not change the queue
uint8_t watchy_event_queue_length(void)
{
    return ev_queue.length;
}
