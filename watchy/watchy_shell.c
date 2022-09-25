#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdio_rtt.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <board.h>
#include <periph/pwm.h>
#include <periph/gpio.h>

#include <bmx280.h>
extern bmx280_t bmx280_dev;

#include <shell.h>

#define ENABLE_DEBUG 1
#include "debug.h"

#include "watchy.h"
#include "watchy_events.h"
#include "kx023-1025.h"
#include "magneto.h"
#include "gnss.h"

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

static int _cmd_atm_pressure(int argc, char **argv)
{
        (void) argc;
        (void) argv;
        int16_t temperature = bmx280_read_temperature(&bmx280_dev);
        uint32_t pressure = bmx280_read_pressure(&bmx280_dev);

        DEBUG("%d.%d C @ %ld.%ld hPa\n", temperature/100, temperature%100, pressure/100, pressure%100);

        return 0;
}

static int _cmd_acc(int argc, char **argv)
{
    if (argc==1 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        DEBUG("usage: %s [on|off|get]\n", argv[0]);
        return 0;
    }
    if (strncmp(argv[1], "on", 2) == 0) {
        if (kx023_activate() == 0)
            DEBUG("accel on\n");
        else
            DEBUG("accel fail\n");
    }
    if (strncmp(argv[1], "off", 3) == 0) {
        if (kx023_activate() == 0)
            DEBUG("accel off\n");
        else
            DEBUG("accel fail\n");
    }
    if (strncmp(argv[1], "get", 3) == 0) {
        int16_t x,y,z;
        kx023_read_accel(&x, &y, &z);
        DEBUG("x=%d y=%d z=%d\n", x,y,z);
    }
    return 0;
}

static int _cmd_hrm(int argc, char **argv)
{
    if (argc==1 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        DEBUG("usage: %s [on|off|get]\n", argv[0]);
        return 0;
    }
    if (strncmp(argv[1], "on", 2) == 0) {
        gpio_set(HRM_PWR);
    }
    if (strncmp(argv[1], "off", 3) == 0) {
        gpio_clear(HRM_PWR);
    }

    return 0;
}

static int _cmd_mag(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    uint16_t x,y,z;
    int ret;

    ret = magneto_read(&x, &y, &z);
    DEBUG("r=0x%02x x=%d y=%d z=%d\n", ret, x, y, z);

    return 0;
}

static int _cmd_info(int argc, char **argv)
{
    if (argc==1 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        DEBUG("usage: %s [on|off|get]\n", argv[0]);
        return 0;
    }

    strncpy(watch_state.info, argv[1], 63);
    watchy_event_queue_add(EV_INFO_NOTE);
    //thread_wakeup(watch_state.event_thread_pid);
    return 0;
}

static const shell_command_t shell_commands[] = {
        { "acc", "accelerometer", _cmd_acc },
        { "bat", "get battery state", _cmd_bat },
        { "bl", "set LCD backlight brightness", _cmd_bl },
        { "gnss", "turn on/off GNSS/GPS", _cmd_gnss },
        { "hrm", "turn on/off HRM", _cmd_hrm },
        { "info", "set info text", _cmd_info },
        { "mag", "read mag once", _cmd_mag },
        { "off", "power off device", _cmd_off },
        { "pr", "get athmo pressure", _cmd_atm_pressure },
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

