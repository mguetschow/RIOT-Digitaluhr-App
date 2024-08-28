#ifndef _WEATHERSTATION_H
#define _WEATHERSTATION_H

struct weatherpoint {
     int16_t temp;
     uint32_t pressure;
#ifdef HAS_RH_SENSOR
     uint16_t rh;
#endif
};


void weather_update_readings(struct tm *clock);
struct weatherpoint *weather_get_current(void);
struct weatherpoint *weather_get_24(void);
int weather_get_trend(void);
void weatherstation_init(uint32_t init_pressure);

#endif
