#ifndef _GNSS_H_
#define _GNSS_H_

#define NMEA_LINE_BUF_LEN 80

void handle_gnss_event(char *nmea_line, watchy_state_t *watch_state);


#endif
