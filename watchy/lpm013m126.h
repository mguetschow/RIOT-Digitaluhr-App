#ifndef _LPM013M126_H
#define _LPM013M126_H

//
// init the display and what else is needed,
// returns 0 if OK, !=0 otherwise
//
int lpm013m126_init(void);

void lpm013m126_clear(void);
void lpm013m126_noupdate(void);
void lpm013m126_invert(void);
int lpm013m126_write(uint8_t *buf, int len);
int lpm013m126_write_lines(uint8_t *lines, uint8_t start, uint16_t cnt);

#endif
