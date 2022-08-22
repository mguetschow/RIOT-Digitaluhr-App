/*
 * Copyright (C) 2022 Nicole Faerber, <nicole.faerber@digitaluhr-manufactur.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <string.h>
#include <time.h>
#include "byteorder.h"

#include "shell.h"
//#include "shell_commands.h"

#include "clk.h"
#include "board.h"
#include "irq.h"
#include "periph_conf.h"
#include "periph/gpio.h"
#include "ringbuffer.h"
#include "timex.h"
#include "ztimer.h"
#include "ztimer/periodic.h"
#include "thread.h"
#include "periph/pm.h"
#include "periph/adc.h"
#include "periph/spi.h"
#include "periph/pwm.h"
#include "periph/i2c.h"
#include "periph/uart.h"
#include "cst816s.h"

#define BMX280_PARAM_I2C_DEV        I2C_DEV(2)
#define BMX280_PARAM_I2C_ADDR       (0x76)
#include "bmx280_params.h"
#include "bmx280.h"

#include "lvgl/lvgl.h"
#include "lvgl_riot.h"
#include "disp_dev.h"

#include "minmea.h"

#include "watchy.h"
#include "watchy_events.h"
#include "gnss.h"

#include "nimble_riot.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "net/bluetil/ad.h"

#include "nimble_autoadv.h"

#include "gatt-adv.h"

#define ENABLE_DEBUG 1
#include "debug.h"

// #include "rl2.h"

static char line_buf[SHELL_DEFAULT_BUFSIZE];
static uint32_t button_ev = false;
static char shell_thread_stack[THREAD_STACKSIZE_SMALL+THREAD_EXTRA_STACKSIZE_PRINTF];
static char event_thread_stack[THREAD_STACKSIZE_SMALL+THREAD_EXTRA_STACKSIZE_PRINTF];
static kernel_pid_t shell_thread_pid=0;
static kernel_pid_t event_thread_pid=0;
// static struct tm _my_time;

// static uint8_t bl_timeout=5;
//static power_supply_stat_t pwr_stat = { false, false, 0, 0 };

static watchy_state_t watch_state;

static lv_obj_t *lv_main_screen=NULL;
static lv_obj_t *lv_second_screen=NULL;

static char nmea_line[NMEA_LINE_BUF_LEN];

//static uint8_t pointer_last_x=0;
//static uint8_t pointer_last_y=0;
//static bool pointer_clicked=false;

#define DISPLAY_TIMEOUT 5

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

	ztimer_sleep(ZTIMER_MSEC, 5);
	board_power_off();

	return 0;
}

// %	V
// 100	4.2
// 95	4.15
// 90	4.11
// 85	4.08
// 80	4.02
// 75	3.98
// 70	3.95
// 65	3.91
// 60	3.87
// 55	3.85
// 50	3.84
// 45	3.82
// 40	3.8
// 35	3.79
// 30	3.77
// 25	3.75
// 20	3.73
// 15	3.71
// 10	3.69
// 5	3.61
// 0	3.27

#if 0
static const uint16_t bat_mv_percent[] = {
  4200, 100,
  4150, 95,	// 50
  4110, 90,	// 40
  4070, 85,	// 40
  4030, 80,	// 40
  3990, 75,	// 40
  3950, 70,	// 40
  3910, 65,	// 40
  3870, 60,	// 40
  3850, 55,	// 20
  3830, 50,	// 20
  3810, 45,	// 20
  3790, 40,	// 20
  3770, 35,	// 20
  3750, 30,	// 20
  3730, 25,	// 20
  3710, 20,	// 20
  3690, 15,	// 20
  3670, 10,	// 20
  3600, 5,	// 70
  3270, 0	// 20
};
#endif

// returns true if succeeded and values valid
// false otherwise
// ca. 3500mV - 4200mV ~= 0% - 100%
#define MIN_BAT_VOLT 3300
bool get_power_stat(power_supply_stat_t *pwr)
{
	int32_t bvolt;

	assert(pwr);

	bvolt=(int32_t)adc_sample(1, ADC_RES_12BIT);
	if (bvolt < 0)
		return false;
	bvolt= 3300 * (uint32_t)bvolt / (4095/4);
	pwr->battery_mvolt = (uint16_t)bvolt;
	//DEBUG("%d.%dV\n", (bvolt/1000), (bvolt%1000));

#if 1
	// linear approximation of battery % from full (4.2V) to
	// minimum (MIN_BAT_VOLT), at the low current that the device
	// draws the discharge curve is almost linear
	if (bvolt > MIN_BAT_VOLT) {
		pwr->battery_percent = (bvolt - MIN_BAT_VOLT) / ((4200 - MIN_BAT_VOLT) / 100);
		if (pwr->battery_percent > 100)
			pwr->battery_percent = 100;
	} else {
		pwr->battery_percent = 0;
	}
#else
	pwr->battery_percent = 0;
	for (uint8_t i=0; i<ARRAY_SIZE(bat_mv_percent); i+=2) {
		if (bvolt >= bat_mv_percent[i]) {
			pwr->battery_percent = bat_mv_percent[i+1];
			break;
		}
	}
#endif

	pwr->charger_present = !gpio_read(EXTPOWER_PRESENT);

	pwr->charge_complete = gpio_read(CHARGE_COMPLETE);

	return true;
}

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

void gnss_power_control(bool pwr)
{
	if (pwr) {
		watch_state.gnss_pwr = true;
		gpio_set(GPS_PWR);
	} else {
		watch_state.gnss_pwr = false;
		gpio_clear(GPS_PWR);
		watch_state.gnss_state.fix_valid = false;
		watch_state.gnss_state.sats_in_fix = 0;
		watch_state.gnss_state.sats_in_view = 0;
	}
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

static const shell_command_t shell_commands[] = {
	{ "bat", "get battery state", _cmd_bat },
	{ "bl", "set LCD backlight brightness", _cmd_bl },
	{ "gnss", "turn on/off GNSS/GPS", _cmd_gnss },
	{ "off", "power off device", _cmd_off },
	{ "time", "print dttick", _cmd_time },
	{ "vib", "set vibration", _cmd_vib },
	{ NULL, NULL, NULL }
};


#if 0
static lpm013m126_t _disp_dev;

static const lpm013m126_params_t lpm013m126_params = {
    .spi = SPI_DEV(0),
    .spi_clk = SPI_CLK_5MHZ,
    .spi_mode = SPI_MODE_0,
    .cs_pin = GPIO_PIN(0, 5),
    .extcom_pin = GPIO_PIN(0, 6),
    .disp_pin = GPIO_PIN(0, 7),
    .inverted = 0,
    .lines = 176U,
};

static void display_logo(lpm013m126_t *dev)
{
    int sline=0;

    for (sline=0; sline<176; sline+=16) {
        lpm013m126_write_lines(dev, &riot_logo_4bpp[sline*(176/2)], sline, 16);
    }
}
#endif

static cst816s_t _input_dev;
static cst816s_touch_data_t _tdata;

static const cst816s_params_t _cst816s_input_params = {
	.i2c_dev = I2C_DEV(0),
	.i2c_addr = TOUCH_I2C_ADDR,
	.irq = TOUCH_INT,
	.irq_flank = GPIO_FALLING,
	.reset = TOUCH_RESET,
};

static void touch_cb(void *arg)
{
	(void) arg;

	// can not read I2C from IRQ context, just set a flag
	watchy_event_queue_add(EV_TOUCH);
	thread_wakeup(event_thread_pid);
}

static void _push_button_cb(void *arg)
{
    (void) arg;
    static uint32_t last_time_pressed=0;

    // DEBUG("Button %s", gpio_read(BUTTON0) ? "released " : "pressed\n");
    if (gpio_read(BUTTON0)) {
        button_ev = ztimer_now(ZTIMER_MSEC) - last_time_pressed;
    } else {
        last_time_pressed = ztimer_now(ZTIMER_MSEC);
        button_ev=1;
    }
    watchy_event_queue_add(EV_BUTTON);
    thread_wakeup(event_thread_pid);
}

static void _ext_power_cb(void *arg)
{
    (void) arg;
    
    watchy_event_queue_add(EV_POWER_CHANGE);
    thread_wakeup(event_thread_pid);
}


void uart_rx_cb(void *arg, uint8_t data)
{
  (void) arg;

  if (data == '\n') {
      watchy_event_queue_add(EV_GNSS);
      thread_wakeup(event_thread_pid);
  } else {
      if (data > 0x1f) {
        if (strlen(nmea_line) < (NMEA_LINE_BUF_LEN-2))
          nmea_line[strlen(nmea_line)] = data;
        else
          memset(nmea_line, 0, NMEA_LINE_BUF_LEN);
      }
  }
}

#if 0
static void print_tdata(cst816s_touch_data_t *tdat)
{
            switch (tdat->action) {
                case CST816S_TOUCH_DOWN:
                    DEBUG("down ");
                    break;
                case CST816S_TOUCH_UP:
                    DEBUG("up ");
                    break;
                case CST816S_TOUCH_CONTACT:
                    DEBUG("contact ");
                    break;
                default:
                    break;
            };
            switch (tdat->gesture) {
                case CST816S_GESTURE_NONE:
                    DEBUG("gnone ");
                    break;
                case CST816S_GESTURE_SLIDE_DOWN:
                    DEBUG("gslide_dwn ");
                    break;
                case CST816S_GESTURE_SLIDE_UP:
                    DEBUG("gslide_up ");
                    break;
                case CST816S_GESTURE_SLIDE_LEFT:
                    DEBUG("gslide_left ");
                    break;
                case CST816S_GESTURE_SLIDE_RIGHT:
                    DEBUG("gslide_right ");
                    break;
                case CST816S_GESTURE_SINGLE_CLICK:
                    DEBUG("gsclick ");
                    break;
                case CST816S_GESTURE_DOUBLE_CLICK:
                    DEBUG("gdclick ");
                    break;
                case CST816S_GESTURE_LONG_PRESS:
                    DEBUG("glpress ");
                    break;
                default:
                    break;
            }
            DEBUG("%d,%d\n", tdat->x, tdat->y);
}
#endif

//extern lv_font_t SourceSansProRegular14_1bpp;
extern lv_font_t SourceSansProSemiBold14_4bpp;
extern lv_font_t SourceSansProSemiBold18_4bpp;
extern lv_font_t SourceSansProSemiBold36_num_4bpp;
extern lv_font_t SourceSansProSemiBold48_num_4bpp;
extern lv_font_t SourceSansProBold72_num_4bpp;

lv_obj_t *clock_label=NULL;
lv_obj_t *date_label=NULL;
lv_obj_t *bat_label=NULL;
lv_obj_t *icon_label=NULL;

static void msgbox_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_current_target(e);
    lv_obj_t *mbox = lv_event_get_user_data(e);

    DEBUG("Button %s clicked\n", lv_msgbox_get_active_btn_text(obj));

    if (strcmp("OK", lv_msgbox_get_active_btn_text(obj))==0)
      _cmd_off(0, NULL);

    lv_msgbox_close(mbox);
}

void power_off_dialog(lv_obj_t *par)
{
    static const char * btns[] = {"OK", "Cancel", ""};

    lv_obj_t * mbox1 = lv_msgbox_create(par, "Power Off", "Are you sure?", btns, false);
    lv_obj_set_style_bg_color(mbox1, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(mbox1, &SourceSansProSemiBold14_4bpp, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(mbox1, msgbox_event_cb, LV_EVENT_VALUE_CHANGED, mbox1);
    lv_obj_center(mbox1);
}

static const char *btnm_map[] = {LV_SYMBOL_BLUETOOTH, LV_SYMBOL_SETTINGS, "\n",
                                  LV_SYMBOL_EYE_OPEN, LV_SYMBOL_POWER, ""};

static void settings_button_handler(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);

	if (code == LV_EVENT_VALUE_CHANGED) {
		uint16_t id = lv_btnmatrix_get_selected_btn(obj);
		const char *txt = lv_btnmatrix_get_btn_text(obj, id);

		DEBUG("%d: '%s' was pressed\n", id, txt);
		switch (id) {
			case 0:
				watch_state.bluetooth_pwr = !watch_state.bluetooth_pwr;
				if (watch_state.bluetooth_pwr)
				  nimble_autoadv_start(NULL);
                                else
                                  nimble_autoadv_stop();
				break;
			case 1:
				watch_state.gnss_pwr = !watch_state.gnss_pwr;
				gnss_power_control(watch_state.gnss_pwr);
				break;
                        case 2:
                                break;
                        case 3:
                                power_off_dialog(lv_obj_get_parent(obj));
                                break;
			default:
				break;
		}
	}
}

static lv_obj_t *create_second_screen(void)
{
    lv_obj_t *second_screen;
    //lv_obj_t *label;

    second_screen = lv_obj_create(NULL);

    lv_obj_t *btnm1 = lv_btnmatrix_create(second_screen);
    lv_obj_set_size(btnm1, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    lv_obj_set_style_bg_color(btnm1, lv_color_black(), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnm1, lv_color_make(0,0,0xff), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnm1, lv_color_make(0,0xff,0), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(btnm1, &lv_font_montserrat_14, LV_STATE_DEFAULT);
//    lv_obj_set_style_text_font(btnm1, &lv_font_montserrat_36, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btnm1, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_btnmatrix_set_map(btnm1, btnm_map);

    lv_btnmatrix_set_btn_ctrl(btnm1, 0, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(btnm1, 0, watch_state.bluetooth_pwr ? LV_BTNMATRIX_CTRL_CHECKED : 0);

    lv_btnmatrix_set_btn_ctrl(btnm1, 1, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(btnm1, 1, watch_state.gnss_pwr ? LV_BTNMATRIX_CTRL_CHECKED : 0);

    lv_obj_align(btnm1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btnm1, settings_button_handler, LV_EVENT_ALL, NULL);

    return second_screen;
}

static lv_obj_t *create_main_screen(void)
{
    lv_obj_t *main_screen;

    main_screen = lv_obj_create(NULL);

    bat_label=lv_label_create(main_screen);
    lv_label_set_recolor(bat_label, true);
    lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(bat_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_pos(bat_label, 115, 1);

    icon_label=lv_label_create(main_screen);
    lv_label_set_recolor(icon_label, true);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_12, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(icon_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_pos(icon_label, 1, 1);

    clock_label=lv_label_create(main_screen);
    lv_label_set_recolor(clock_label, true);
    lv_obj_set_style_text_font(clock_label, &SourceSansProBold72_num_4bpp, LV_STATE_DEFAULT);
    lv_obj_set_pos(clock_label, 0, 50);

    date_label=lv_label_create(main_screen);
    lv_label_set_recolor(date_label, true);
    lv_obj_set_style_text_font(date_label, &SourceSansProSemiBold36_num_4bpp, LV_STATE_DEFAULT);
    lv_obj_set_pos(date_label, 50, 120);

    return main_screen;
}

#if 1
static bool _xdisplay_on = false;

static void xdisplay_off(void)
{
  if (_xdisplay_on) {
    // power off backlight
    pwm_poweroff(PWM_DEV(0));
    // power off EXTCOM PWM
    pwm_poweroff(PWM_DEV(1));
    _xdisplay_on = false;
  }
  gpio_toggle(LCD_EXTCOMIN);
}

static void xdisplay_on(void)
{
  if (!_xdisplay_on) {
    gpio_clear(LCD_EXTCOMIN);

    // power on EXTCOM PWM
    pwm_poweron(PWM_DEV(1));
    // power on backlight
    pwm_poweron(PWM_DEV(0));
    _xdisplay_on=true;
  }
}
#endif

void update_main_screen(void)
{
	get_power_stat(&watch_state.pwr_stat);
	if (lv_main_screen != NULL) {
		char lstr[32];

       //snprintf(lstr, 15, "%02d:%02d:%02d", _my_time.tm_hour, _my_time.tm_min, _my_time.tm_sec);
       //snprintf(lstr, 15, "%02d%s :#%02d", _my_time.tm_hour, (_my_time.tm_sec%2)?"#00ff00":"#ff0000", _my_time.tm_min);
       snprintf(lstr, 15, "%02d:%02d", watch_state.clock.tm_hour, watch_state.clock.tm_min);
       lv_label_set_text(clock_label, lstr);
       // DEBUG("%s\n", lstr);
       snprintf(lstr, 15, "%d.%d.", watch_state.clock.tm_mday, watch_state.clock.tm_mon+1);
       lv_label_set_text(date_label, lstr);

       snprintf(lstr, 15, "%d%% ", watch_state.pwr_stat.battery_percent);
       strcat(lstr, watch_state.pwr_stat.charger_present ? LV_SYMBOL_CHARGE " " : " ");
       if (watch_state.pwr_stat.battery_percent > 80)
           strcat(lstr, LV_SYMBOL_BATTERY_FULL);
       else if (watch_state.pwr_stat.battery_percent > 60)
           strcat(lstr, LV_SYMBOL_BATTERY_3);
       else if (watch_state.pwr_stat.battery_percent > 40)
           strcat(lstr, LV_SYMBOL_BATTERY_3);
       else if (watch_state.pwr_stat.battery_percent > 20)
           strcat(lstr, LV_SYMBOL_BATTERY_1);
       else
           strcat(lstr, "#ff0000 " LV_SYMBOL_BATTERY_EMPTY "#");
       lv_label_set_text(bat_label, lstr);

       memset(lstr, 0, 32);
       if (watch_state.bluetooth_pwr) {
         strncat(lstr,"#ffffff " LV_SYMBOL_BLUETOOTH "# ", 31);
       }
       if (watch_state.gnss_pwr) {
           char satnum[16];
           snprintf(satnum, 15, "%d/%d", watch_state.gnss_state.sats_in_fix, watch_state.gnss_state.sats_in_view);
           if (watch_state.gnss_state.fix_valid) {
               strncat(lstr,"#00ff00 " LV_SYMBOL_GPS "# ", 31);
           } else {
               strncat(lstr,"#ffffff " LV_SYMBOL_GPS "# ", 31);
           }
           strncat(lstr, satnum, 31);
       }
       lv_label_set_text(icon_label, lstr);
   }
}

void *event_thread(void *arg)
{
    (void) arg;
    watchy_event_t ev;
    bool wake_lvgl=false;
    static uint8_t bl_timeout=DISPLAY_TIMEOUT;
 
    while (true) {
      while (watchy_event_queue_length()) {
          // DEBUG("ev=%d\n", watchy_event_queue_length());
          ev=watchy_event_queue_get();
          switch (ev) {
              case EV_SEC_TICK:
                  if (watch_state.clock.tm_sec==0 || watch_state.gnss_pwr) {
                    update_main_screen();
                    wake_lvgl=true;
                  }
                  if (bl_timeout) {
                    bl_timeout--;
                  } else {
                    //lpm013m126_off();
                    xdisplay_off();
                  }
                  break;
              case EV_MSEC_TICK:
                  break;
              case EV_TOUCH:
                  cst816s_read(&_input_dev, &_tdata);
                  watch_state.touch_state.x = _tdata.x;
                  watch_state.touch_state.y = _tdata.y;
                  // print_tdata(&_tdata);
                  if (bl_timeout) {
                    if (_tdata.action == CST816S_TOUCH_UP) {
                      if (_tdata.gesture == CST816S_GESTURE_SLIDE_UP && lv_main_screen==NULL) {
                        lv_main_screen = create_main_screen();
                        update_main_screen();
                        lv_scr_load_anim(lv_main_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 250, 0, true);
                        lv_second_screen = NULL;
                      } else if (_tdata.gesture == CST816S_GESTURE_SLIDE_DOWN && lv_second_screen==NULL) {
                        lv_second_screen = create_second_screen();
                        lv_scr_load_anim(lv_second_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 250, 0, true);
                        lv_main_screen = NULL;
                        }
                    } else { // CST816S_TOUCH_DOWN
                      if (_tdata.gesture == CST816S_GESTURE_SINGLE_CLICK) {
                        watch_state.touch_state.clicked = true;
                        // DEBUG("click %d %d\n", pointer_last_x, pointer_last_y);
                      }
                    }
                    wake_lvgl=true;
                    bl_timeout=DISPLAY_TIMEOUT;
                  } else {
                    if (_tdata.gesture == CST816S_GESTURE_SINGLE_CLICK) {
                      //lpm013m126_on();
                      xdisplay_on();
                      bl_timeout=DISPLAY_TIMEOUT;
                    }
                  }
                  break;
              case EV_BUTTON:
                  DEBUG("btn\n");
                  bl_timeout=DISPLAY_TIMEOUT;
                  //lpm013m126_on();
                  xdisplay_on();
                  if (lv_main_screen==NULL) {
                      lv_main_screen = create_main_screen();
                      update_main_screen();
                      lv_scr_load_anim(lv_main_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 250, 0, true);
                      lv_second_screen = NULL;
                  }
                  break;
              case EV_ACCEL:
                  break;
              case EV_MAGNETOMETER:
                  break;
              case EV_GNSS:
              	  // DEBUG("g: %s\n", nmea_line);
                  handle_gnss_event(nmea_line, &watch_state);
                  break;
              case EV_ATMOSPHERE:
                  break;
              case EV_DISPLAY_TIMEOUT:
                  break;
              case EV_POWER_CHANGE:
                  update_main_screen();
                  bl_timeout=DISPLAY_TIMEOUT;
                  //lpm013m126_on();
                  xdisplay_on();
                  wake_lvgl = true;
                  break;
              default:
                  DEBUG("no event?\n");
                  break;
          }; // switch()
      } // while (ev)
      // once done with all events, do housekeeping
      if (wake_lvgl) {
          wake_lvgl = false;
          lvgl_wakeup();
      }
      thread_sleep();
    } // while(true)

    return NULL;
}

void *shell_thread(void *arg)
{
    (void) arg;

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return NULL;
}

static bool rtc_second_cb(void *arg)
{
   (void) arg;

   watch_state.clock.tm_sec++;
   if (watch_state.clock.tm_sec > 59) {
     watch_state.clock.tm_sec=0;
     watch_state.clock.tm_min++;
   }
   if (watch_state.clock.tm_min > 59) {
     watch_state.clock.tm_min=0;
     watch_state.clock.tm_hour++;
   }
   if (watch_state.clock.tm_hour > 23) {
     watch_state.clock.tm_hour=0;
     watch_state.clock.tm_mday++;
   }

   watchy_event_queue_add(EV_SEC_TICK);

   thread_wakeup(event_thread_pid);

   return true;
}

void lv_input_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  (void) drv;

  data->point.x = watch_state.touch_state.x;
  data->point.y = watch_state.touch_state.y;
  data->continue_reading = false;
  if (watch_state.touch_state.clicked) {
    watch_state.touch_state.clicked=false;
    data->state = LV_INDEV_STATE_PRESSED;
    // DEBUG("lvclk %d %d\n", data->point.x, data->point.y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED; 
    // DEBUG("lvclk rel\n");
  }
}


//static bmx280_t bmx280_dev;

int main(void)
{
	static ztimer_periodic_t timer;
	static lv_indev_drv_t indev_drv;

	memset(&watch_state, 0, sizeof(watchy_state_t));

	watch_state.clock.tm_year = 2022 - TM_YEAR_OFFSET;
	watch_state.clock.tm_mon = 2;
	watch_state.clock.tm_mday = 13;
	watch_state.clock.tm_hour = 1;
	watch_state.clock.tm_min = 0;
	watch_state.clock.tm_sec = 0;

	ztimer_periodic_init(ZTIMER_SEC, &timer, rtc_second_cb, NULL, 1);
	ztimer_periodic_start(&timer);

	watch_state.gnss_pwr = false;
	watch_state.bluetooth_pwr = false;

	// init LCD, display logo and enable backlight
	// lpm013m126_init(&_disp_dev, &lpm013m126_params);
	// display_logo(&_disp_dev);
	pwm_set(PWM_DEV(0), 0, 50);
	//pwm_poweron(PWM_DEV(0));
	xdisplay_on();

	gpio_init_int(BUTTON0, GPIO_IN_PU, GPIO_BOTH, _push_button_cb, NULL);
	gpio_init_int(EXTPOWER_PRESENT, GPIO_IN_PU, GPIO_BOTH, _ext_power_cb, NULL);

	if (cst816s_init(&_input_dev, &_cst816s_input_params, touch_cb, NULL) != CST816S_OK) {
		DEBUG("cst init fail\n");
	};

	memset(nmea_line, 0, NMEA_LINE_BUF_LEN);
	// GNSS/GPS UART, 9600baud default
	if (uart_init(UART_DEV(0), 9600, uart_rx_cb, NULL)) {
		DEBUG("error configuring 9600 baud\n");
	}

	event_thread_pid=thread_create(event_thread_stack, sizeof(event_thread_stack),
		THREAD_PRIORITY_IDLE - 1, THREAD_CREATE_STACKTEST,
		event_thread, NULL, "event_thread");
	DEBUG("eventthr=%d\n", event_thread_pid);

	shell_thread_pid=thread_create(shell_thread_stack, sizeof(shell_thread_stack),
		THREAD_PRIORITY_IDLE - 1, THREAD_CREATE_STACKTEST,
		shell_thread, NULL, "shell_thread");
	DEBUG("shellthr=%d\n", shell_thread_pid);

   	get_power_stat(&watch_state.pwr_stat);

	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = lv_input_cb;
	lv_indev_drv_register(&indev_drv);


	// DEBUG("c=%d\n", sizeof(lv_color_t));
	lv_main_screen = create_main_screen();
	update_main_screen();
	lv_second_screen = NULL;

	lv_disp_load_scr(lv_main_screen);

	watchy_event_queue_add(EV_SEC_TICK);
	thread_wakeup(event_thread_pid);

	watchy_gatt_init();

#if 0
	switch (bmx280_init(&bmx280_dev, &bmx280_params[0])) {
        case BMX280_ERR_BUS:
            DEBUG("[Error] Something went wrong when using the I2C bus");
            return 1;
        case BMX280_ERR_NODEV:
            DEBUG("[Error] Unable to communicate with any BMX280 device");
            return 1;
        default:
            /* all good -> do nothing */
            break;
    }

    DEBUG("Initialization successful\n");
#endif
	lvgl_run();

	return 0;
}
