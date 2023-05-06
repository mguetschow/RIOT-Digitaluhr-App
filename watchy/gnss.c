#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <tm.h>
#include <rtc_utils.h>

#include <periph/gpio.h>
#include <periph/uart.h>

#include "minmea.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "watchy.h"
#include "gnss.h"


void gnss_power_control(bool pwr)
{
        if (pwr) {
                watch_state.gnss_pwr = true;
                uart_poweron(UART_DEV(0));
                gpio_set(GPS_PWR);
        } else {
                watch_state.gnss_pwr = false;
                uart_poweroff(UART_DEV(0));
                gpio_clear(GPS_PWR);
                watch_state.gnss_state.fix_valid = false;
                watch_state.gnss_state.sats_in_fix = 0;
                watch_state.gnss_state.sats_in_view = 0;
        }
}


void handle_gnss_event(char *nmea_line, watchy_state_t *watch_state)
{
	enum minmea_sentence_id sentence_id;

	DEBUG("\n%d '%s'\n", minmea_sentence_id(nmea_line, false), nmea_line);

	sentence_id = minmea_sentence_id(nmea_line, false);

	switch (sentence_id) {
		case MINMEA_SENTENCE_ZDA: {
			struct minmea_sentence_zda frame;
			int res = minmea_parse_zda(&frame, nmea_line);
			if (!res) {
				DEBUG("GNSS: error parsing ZDA sentence\n");
			} else {
				DEBUG("ZDA %d.%d.%d %d:%d.%d\n", frame.date.day, frame.date.month, frame.date.year, frame.time.hours, frame.time.minutes, frame.time.seconds);
				if (frame.time.hours != -1) {
					// time is valid now
					watch_state->gnss_state.time_valid = true;
					// compensate for time zone and DST
					watch_state->clock.tm_hour = frame.time.hours + watch_state->timez + ((watch_state->clock.tm_isdst > 0) ? 1 : 0);
					watch_state->clock.tm_min = frame.time.minutes;
					watch_state->clock.tm_sec = frame.time.seconds;
					rtc_tm_normalize(&watch_state->clock);
					watch_state->rtc_time = rtc_mktime(&watch_state->clock);
				} else {
					watch_state->gnss_state.time_valid = false;
				}
				if (frame.date.day > 0) {
					watch_state->gnss_state.date_valid = true;
					watch_state->clock.tm_mday = frame.date.day;
					watch_state->clock.tm_mon = frame.date.month - 1;
					watch_state->clock.tm_year = frame.date.year - TM_YEAR_OFFSET;
					rtc_tm_normalize(&watch_state->clock);
					watch_state->rtc_time = rtc_mktime(&watch_state->clock);
				} else {
					watch_state->gnss_state.date_valid = false;
				}
			}
			break;
		}
		case MINMEA_SENTENCE_RMC: {
			DEBUG("GNSS: RMC\n");
			struct minmea_sentence_rmc frame;
			int res = minmea_parse_rmc(&frame, nmea_line);
			if (!res) {
				DEBUG("GNSS: error parsing RMC sentence\n");
			} else {
				if (watch_state->gnss_state.fix_valid) {
					watch_state->gnss_state.lat = minmea_tocoord(&frame.latitude);
					watch_state->gnss_state.lon = minmea_tocoord(&frame.longitude);
					watch_state->gnss_state.speed = (int16_t)minmea_tofloat(&frame.speed);
					watch_state->gnss_state.course = (int16_t)minmea_tofloat(&frame.course);
					DEBUG("GNSS: %3.2f %3.2f %d\n", watch_state->gnss_state.lat, watch_state->gnss_state.lon, watch_state->gnss_state.height);
				}
			}
			break;
		}
		case MINMEA_SENTENCE_GGA: {
			struct minmea_sentence_gga frame;
			int res = minmea_parse_gga(&frame, nmea_line);
			if (!res) {
				DEBUG("GNSS: error parsing GGA sentence\n");
			} else {
				DEBUG("GNSS: sats tracked  %d\n", frame.satellites_tracked);
				if (watch_state->gnss_state.fix_valid) {
					watch_state->gnss_state.lat = minmea_tocoord(&frame.latitude);
					watch_state->gnss_state.lon = minmea_tocoord(&frame.longitude);
					watch_state->gnss_state.height = (int16_t)minmea_tofloat(&frame.altitude);
					watch_state->gnss_state.sats_in_fix = frame.satellites_tracked;
					DEBUG("GNSS: %d %3.2f %3.2f %d\n", frame.satellites_tracked, watch_state->gnss_state.lat, watch_state->gnss_state.lon, watch_state->gnss_state.height);
				}
			}
			break;
		}
		case MINMEA_SENTENCE_GSA: {
			struct minmea_sentence_gsa frame;
			int res = minmea_parse_gsa(&frame, nmea_line);
			if (!res) {
				DEBUG("GNSS: error parsing GSA sentence\n");
			} else {
				DEBUG("GNSS: fix type  %d\n", frame.fix_type);
				if (frame.fix_type > 1) {
					watch_state->gnss_state.fix_valid = true;
				} else {
					watch_state->gnss_state.fix_valid = false;
					watch_state->gnss_state.sats_in_fix = 0;
				}
			}
			break;
		}
		case MINMEA_SENTENCE_GLL:
			break;
		case MINMEA_SENTENCE_GST:
			break;
		case MINMEA_SENTENCE_GSV: {
			struct minmea_sentence_gsv frame;
			int res = minmea_parse_gsv(&frame, nmea_line);
			if (!res) {
				DEBUG("GNSS: error parsing GSV sentence\n");
			} else {
				DEBUG("GNSS: %d sats in view  %d\n", frame.msg_nr, frame.total_sats);
				if (watch_state->gnss_state.fix_valid) {
					if (frame.total_sats >= watch_state->gnss_state.sats_in_fix)
						watch_state->gnss_state.sats_in_view = frame.total_sats;
				} else {
					if (frame.total_sats > 0)
						watch_state->gnss_state.sats_in_view = frame.total_sats;
				}
			}
			break;
		}
		case MINMEA_SENTENCE_VTG:
			break;
		case MINMEA_INVALID:
		case MINMEA_UNKNOWN:
			DEBUG("unhandled sentence: %d\n", sentence_id);
			break;
		default:
			break;
	}
	// clear line for new msgs
	memset(nmea_line, 0, NMEA_LINE_BUF_LEN);
}

