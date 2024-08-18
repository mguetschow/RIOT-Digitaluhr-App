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
#include <vfs.h>
#include <vfs_util.h>
#include <fcntl.h>
#include <errno.h>
#include <tiny_strerror.h>
#include <ztimer.h>

#define ENABLE_DEBUG 1
#include "debug.h"

#include "watchy.h"
#include "watchy_events.h"
#include "kx023-1025.h"
#include "magneto.h"
#include "gnss.h"
#include "gatt-adv.h"
#include "weatherstation.h"


static char line_buf[SHELL_DEFAULT_BUFSIZE];
#define VFS_PATH_LEN 32
static char _vfs_path[VFS_PATH_LEN] = "/nvm0";

static void print_time(const struct tm *time)
{
        printf("%04d-%02d-%02d %02d:%02d:%02d\n",
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

        printf("5 sec to power off\n");
        ztimer_sleep(ZTIMER_MSEC, 5000);
        board_power_off();

        return 0;
}

bool get_power_stat(power_supply_stat_t *pwr);
static int _cmd_bat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    get_power_stat(&watch_state.pwr_stat);

    printf("%dmV\n", watch_state.pwr_stat.battery_mvolt);
    printf("%d%%\n", watch_state.pwr_stat.battery_percent);
    printf("%sext power\n", watch_state.pwr_stat.charger_present ? "" : "no ");
    printf("%scharging\n", watch_state.pwr_stat.charge_complete ? "not " : "");

    return 0;
}

static int _cmd_bl(int argc, char **argv)
{
    uint16_t bright;

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: %s brightness (0-100)\n", argv[0]);

        return 0;
    }
    if (argc >= 2) {
        bright = (uint16_t)atoi(argv[1]);
        if (bright) {
            pwm_set(PWM_DEV(0), 0, bright);
            pwm_poweron(PWM_DEV(0));
            watch_state.display_brightness = bright;
        } else {
            pwm_set(PWM_DEV(0), 0, 0);
            pwm_poweroff(PWM_DEV(0));
        }
    } else
    	printf("%d\n", watch_state.display_brightness);

    return 0;
}

static int _cmd_vib(int argc, char **argv)
{
    uint16_t bright;

    // 20-30 is totally enough
    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: %s intens (0-100)\n", argv[0]);

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

        printf("%d.%d C @ %ld.%ld hPa\n", temperature/100, temperature%100, pressure/100, pressure%100);

        return 0;
}

static int _cmd_acc(int argc, char **argv)
{
    if (argc==1 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        printf("usage: %s [on|off|get]\n", argv[0]);
        return 0;
    }
    if (strncmp(argv[1], "on", 2) == 0) {
        if (kx023_activate() == 0)
            printf("accel on\n");
        else
            printf("accel fail\n");
    }
    if (strncmp(argv[1], "off", 3) == 0) {
        if (kx023_activate() == 0)
            printf("accel off\n");
        else
            printf("accel fail\n");
    }
    if (strncmp(argv[1], "get", 3) == 0) {
        int16_t x,y,z;
        kx023_read_accel(&x, &y, &z);
        printf("x=%d y=%d z=%d\n", x,y,z);
    }
    return 0;
}

static int _cmd_hrm(int argc, char **argv)
{
    if (argc==1 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        printf("usage: %s [on|off|get]\n", argv[0]);
        return 0;
    }
    if (strncmp(argv[1], "on", 2) == 0) {
        gpio_set(HRM_PWR);
    }
    if (strncmp(argv[1], "off", 3) == 0) {
        gpio_clear(HRM_PWR);
    }
    if (strncmp(argv[1], "get", 3) == 0) {
        printf("not implemented yet\n");;
    }

    return 0;
}

static int _cmd_mag(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    if (argc==1 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        printf("usage: %s [on|off|calibrate|get]\n", argv[0]);
        return 0;
    }

    if (strncmp(argv[1], "calibrate", 9) == 0) {
        watch_state.magnetometer_state.calibrate = !watch_state.magnetometer_state.calibrate;
        printf("calibration %s\n", watch_state.magnetometer_state.calibrate ? "enabled" : "disabled");
    }
    if (strncmp(argv[1], "on", 2) == 0) {
        watch_state.magnetometer_state.active = true;
    }
    if (strncmp(argv[1], "off", 3) == 0) {
        watch_state.magnetometer_state.active = false;
    }
    if (strncmp(argv[1], "get", 3) == 0) {
    	printf("x=%d y=%d z=%d\n", watch_state.magnetometer_state.x, watch_state.magnetometer_state.y, watch_state.magnetometer_state.z);
    	printf("c=%d\n", watch_state.magnetometer_state.course);
    }

    return 0;
}

static int _cmd_info(int argc, char **argv)
{
	uint8_t nr=0;

    if (argc<3 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        printf("usage: %s <1/2> <text>\n", argv[0]);
        return 1;
    }

	nr=atoi(argv[1]);
	if (nr < 1 || nr > 2)
		return 1;
	if (nr==1)
		strncpy(watch_state.info1, argv[2], 32);
	if (nr==2)
		strncpy(watch_state.info2, argv[2], 32);
    watchy_event_queue_add(EV_INFO_NOTE);
    //thread_wakeup(watch_state.event_thread_pid);
    return 0;
}

static int _cmd_pwd(int argc, char **argv)
{
    (void) argc;
    (void) argv;

	printf("%s\n", _vfs_path);

	return 0;
}

static int _cmd_cd(int argc, char **argv)
{
	vfs_DIR vdir;
	char tpath[VFS_PATH_LEN];

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: %s [path]\n", argv[0]);
        return 0;
    }
    if (argc ==1) {
    	strcpy(tpath, "/nvm0");
    }

	if (argv[1][0] == '/') {
		strcpy(tpath, argv[1]);
	} else {
		// relative path name
		strcpy(tpath, _vfs_path);
		strcat(tpath, "/");
		strcat(tpath, argv[1]);
	}
	vfs_normalize_path(tpath, tpath, strlen(tpath) + 1);
	if (vfs_opendir(&vdir, tpath) == 0) {
		vfs_closedir(&vdir);
		strncpy(_vfs_path, tpath, VFS_PATH_LEN);
		return 0;
	} else
		printf("cd: %s: No such file or directory\n", argv[1]);

	return 1;
}

static int _cmd_ls(int argc, char **argv)
{
	vfs_DIR vdir;
	char *lspath = _vfs_path;
	bool llist = false;

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: %s [-l] [path]\n", argv[0]);
        return 0;
    } else if (argc >= 2) {
	    if ((memcmp(argv[1], "-l", 2) == 0))
    		llist = true;
    }

	if (vfs_opendir(&vdir, lspath) == 0) {
		vfs_dirent_t dirent;
		struct stat stat;
		char path_name[2 * (VFS_NAME_MAX + 1)];
		while (vfs_readdir(&vdir, &dirent) > 0) {
			snprintf(path_name, sizeof(path_name), "%s/%s", _vfs_path, dirent.d_name);
			vfs_stat(path_name, &stat);
			if (stat.st_mode & S_IFDIR) {
				if (llist)
					printf("%lx\t%ld\t%s\n", stat.st_mode, stat.st_size, dirent.d_name);					
				else
					printf("%s/\t", dirent.d_name);
			} else {
				if (llist)
					printf("%lx\t%ld\t%s\n", stat.st_mode, stat.st_size, dirent.d_name);					
				else
					printf("%s\t", dirent.d_name);					
			}
		}
		printf("\n");
		vfs_closedir(&vdir);
		return 0;
	} else
		printf("ls: %s: No such file or directory\n", argv[1]);

	return 0;
}

static int _cmd_cat(int argc, char **argv)
{
	char tpath[VFS_PATH_LEN];
	struct stat stat;

    if (argc==1 || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        printf("usage: %s <file>\n", argv[0]);
        return 0;
    }

	if (argv[1][0] == '/') {
		strcpy(tpath, argv[1]);
	} else {
		// relative path name
		strcpy(tpath, _vfs_path);
		strcat(tpath, "/");
		strcat(tpath, argv[1]);
	}
	vfs_normalize_path(tpath, tpath, strlen(tpath) + 1);

	if (vfs_stat(tpath, &stat) != 0) {
		printf("cat: %s: No such file or directory\n", argv[1]);
		return 1;
	}
	if (stat.st_mode & S_IFDIR) {
		printf("cat: %s: Is a directory\n", argv[1]);
		return 1;
	}
	int fd = vfs_open(tpath, O_RDONLY, 0);
	if (fd < 0) {
		printf("Error opening file \"%s\": %s\n", tpath, tiny_strerror(fd));
		//printf("Error opening file \"%s\"\n", tpath);
		return 3;
	}
	{
		char buf[40];
		memset(buf,0,40);
		while (vfs_read(fd, buf, 40) > 0) {
			printf("%s", buf);
			memset(buf,0,40);
		}
	}
	vfs_close(fd);
	printf("\n");

	return 0;
}

static int _cmd_mkdir(int argc, char **argv)
{
	char tpath[VFS_PATH_LEN];
	if (argc < 2) {
        printf("usage: %s <dir>\n", argv[0]);
		return 1;
	}

	if (argv[1][0] == '/') {
		strcpy(tpath, argv[1]);
	} else {
		// relative path name
		strcpy(tpath, _vfs_path);
		strcat(tpath, "/");
		strcat(tpath, argv[1]);
	}
	vfs_normalize_path(tpath, tpath, strlen(tpath) + 1);

	int res = vfs_mkdir(tpath, 0);
	if (res < 0) {
		printf("mkdir ERR: %s\n", tiny_strerror(res));
		return 2;
	}

	return 0;
}

static int _cmd_weather(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	struct weatherpoint *wp;
	char str[16];

	wp = weather_get_24();

	for (int i=0; i<24; i++) {
		printf("%02d\t", i);
		snprintf(str, 16, "%d.%d°C", wp[i].temp/100, (wp[i].temp%100)/10);
		printf("%s\t", str);
		snprintf(str, 16, "%ld.%ld hPa", wp[i].pressure/100, (wp[i].pressure%100)/10);
		printf("%s\n", str);
	}
	printf("trend: %d\n", weather_get_trend());

	return 0;
}

static const shell_command_t shell_commands[] = {
        { "acc", "accelerometer", _cmd_acc },
        { "bat", "get battery state", _cmd_bat },
        { "bl", "set LCD backlight brightness", _cmd_bl },
        { "cat", "output file to stdout", _cmd_cat },
        { "cd", "change directory", _cmd_cd },
        { "gnss", "turn on/off GNSS/GPS", _cmd_gnss },
        { "hrm", "turn on/off HRM", _cmd_hrm },
        { "info", "set info text", _cmd_info },
        { "ls", "list directory contents", _cmd_ls },
        { "mag", "read mag once", _cmd_mag },
        { "mkdir", "create directory", _cmd_mkdir },
        { "off", "power off device", _cmd_off },
        { "pr", "get athmo pressure", _cmd_atm_pressure },
        { "pwd", "get current path", _cmd_pwd },
        { "time", "print dttick", _cmd_time },
        { "vib", "set vibration", _cmd_vib },
        { "weather", "query weatehr data", _cmd_weather },
        { NULL, NULL, NULL }
};


void *shell_thread(void *arg)
{
    (void) arg;

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return NULL;
}

