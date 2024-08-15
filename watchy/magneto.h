
#ifndef _MAGNETO_H
#define _MAGNETO_H

int magneto_init(void);
int magneto_read(uint16_t *x, uint16_t *y, uint16_t *z);
int magneto_course(uint16_t x, uint16_t y, uint16_t z);
#endif
