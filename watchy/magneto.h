
#ifndef _MAGNETO_H
#define _MAGNETO_H

int magneto_init(void);
bool magneto_trigger(void);
bool magneto_set_state(bool state);
bool magneto_calibrate(bool state);

#endif
