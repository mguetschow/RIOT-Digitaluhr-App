#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <string.h>
#include <time.h>

#include "board.h"
#include "periph/pwm.h"

#include "shell.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#include "watchy.h"

static char line_buf[SHELL_DEFAULT_BUFSIZE];

static void print_time(const struct tm *time)
{
        DEBUG("%04d-%02d-%02d %02d:%02d:%02d\n",
                time->tm_year + TM_YEAR_OFFSET,
                time->tm_mon + 1,
                time->tm_mday,
                time->tm_hour,
                time->tm_min,
                time->tm_sec);
}


static int _cmd_time(int argc, char **argv)
{
        (void) argc;
        (void) argv;

        print_time(&watch_state.clock);

return 0;
}

static int _cmd_off(int argc, char **argv)
{
        (void) argc;
        (void) argv;

        board_power_off();

        return 0;
}

bool get_power_stat(power_supply_stat_t *pwr);
static int _cmd_bat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    get_power_stat(&watch_state.pwr_stat);

    DEBUG("%dmV\n", watch_state.pwr_stat.battery_mvolt);
    DEBUG("%d%%\n", watch_state.pwr_stat.battery_percent);
    DEBUG("%sext power\n", watch_state.pwr_stat.charger_present ? "" : "no ");
    DEBUG("%scharging\n", watch_state.pwr_stat.charge_complete ? "not " : "");

    return 0;
}

static int _cmd_bl(int argc, char **argv)
{
    uint16_t bright;

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        DEBUG("usage: %s brightness (0-100)\n", argv[0]);

        return 0;
    }
    if (argc >= 2) {
        bright = (uint16_t)atoi(argv[1]);
        if (bright) {
            pwm_set(PWM_DEV(0), 0, bright);
            pwm_poweron(PWM_DEV(0));
        } else {
            pwm_set(PWM_DEV(0), 0, 0);
            pwm_poweroff(PWM_DEV(0));
        }
    }

    return 0;
}

static int _cmd_vib(int argc, char **argv)
{
    uint16_t bright;

    // 20-30 is totally enough
    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        DEBUG("usage: %s intens (0-100)\n", argv[0]);

        return 0;
    }
    if (argc >= 2) {
        bright = (uint16_t)atoi(argv[1]);
        if (bright) {
            pwm_set(PWM_DEV(2), 0, bright);
            pwm_poweron(PWM_DEV(2));
        } else {
            pwm_set(PWM_DEV(2), 0, 0);
            pwm_poweroff(PWM_DEV(2));
        }
    }

    return 0;
}

void gnss_power_control(bool pwr);

static int _cmd_gnss(int argc, char **argv)
{
        (void) argc;
        (void) argv;

        if (watch_state.gnss_pwr) {
                puts("turning GNSS off");
                gnss_power_control(false);
        } else {
                puts("turning GNSS on");
                gnss_power_control(true);
        }

        return 0;
}

static const shell_command_t shell_commands[] = {
        { "bat", "get battery state", _cmd_bat },
        { "bl", "set LCD backlight brightness", _cmd_bl },
        { "gnss", "turn on/off GNSS/GPS", _cmd_gnss },
        { "off", "power off device", _cmd_off },
        { "time", "print dttick", _cmd_time },
        { "vib", "set vibration", _cmd_vib },
        { NULL, NULL, NULL }
};


void *shell_thread(void *arg)
{
    (void) arg;

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return NULL;
}

