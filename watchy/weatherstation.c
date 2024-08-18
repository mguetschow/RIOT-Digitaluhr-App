#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <stdint.h>
#include <time.h>

#include <bmx280.h>
extern bmx280_t bmx280_dev;

#define ENABLE_DEBUG 1
#include "debug.h"

#include "weatherstation.h"

#define HEIGHT 230
#define NORMAL 101325	// standard normal pressure 1013.25 at sea level

static struct weatherpoint historic[24];
static struct weatherpoint current;
static int trend=0;

struct weatherpoint *weather_get_current(void)
{
	return &current;
}

struct weatherpoint *weather_get_24(void)
{
	return historic;
}

int weather_get_trend(void)
{
	trend = 0;
	for (int i=0; i<24; i++) {
		if ((historic[i].pressure != 0) && (historic[i+1].pressure) != 0) {
			trend += (int)((int)(historic[(i+1)%24].pressure / 100) - (int)(historic[i].pressure / 100));
		}
	}
	return trend;
}

// this should get called once a minute
void weather_update_readings(struct tm *clock)
{
	current.temp = bmx280_read_temperature(&bmx280_dev);
	current.pressure = bmx280_read_pressure(&bmx280_dev);

	//DEBUG("ws %d.%d C @ %ld.%ld hPa\n", current.temp/100, current.temp%100, current.pressure/100, current.pressure%100);
	if (clock->tm_hour < 24) {
		if (clock->tm_min != 0) { // gliding average
			if ((historic[clock->tm_hour].temp != 0) && (historic[clock->tm_hour].pressure != 0)) {
				historic[clock->tm_hour].temp += current.temp;
				historic[clock->tm_hour].temp /= 2;
				historic[clock->tm_hour].pressure += current.pressure;
				historic[clock->tm_hour].pressure /= 2;
			} else {
				historic[clock->tm_hour].temp = current.temp;
				historic[clock->tm_hour].pressure = current.pressure;
			}
		} else { // top of the hour start new averaging
			historic[clock->tm_hour].temp = current.temp;
			historic[clock->tm_hour].pressure = current.pressure;
		}
	}
}


void weatherstation_init(void)
{
	int i;

	for (i=0; i<24; i++) {
		historic[i].temp = 20;
		//historic[i].pressure = 100000 + (i*100);
		historic[i].pressure = NORMAL;
#ifdef HAS_RH_SENSOR
		historic[i].rh = 0;
#endif
	}
	current.temp=0;
	current.pressure=0;
#ifdef HAS_RH_SENSOR
	current.rh=0;
#endif
}

