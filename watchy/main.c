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
#include "shell_commands.h"

#include "clk.h"
#include "board.h"
#include "irq.h"
#include "periph_conf.h"
#include "periph/gpio.h"
#include "ringbuffer.h"
#include "timex.h"
#include "xtimer.h"
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
#include "lpm013m126.h"

#include "lvgl/lvgl.h"
#include "lvgl_riot.h"
#include "disp_dev.h"

#include "minmea.h"

#include "watchy.h"
#include "watchy_events.h"

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

#define NMEA_LINE_BUF_LEN 80
static char nmea_line[NMEA_LINE_BUF_LEN];

#define TM_YEAR_OFFSET      (1900)


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

    xtimer_usleep(US_PER_SEC * 5);
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

static int _cmd_gnss(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    if (gpio_read(GPS_PWR)) {
      puts("turning GNSS off");
      gpio_clear(GPS_PWR);
    } else {
      puts("turning GNSS on");
      gpio_set(GPS_PWR);
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
        button_ev = xtimer_now_usec() - last_time_pressed;
    } else {
        last_time_pressed = xtimer_now_usec();
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
      if (data>0x1f)
        nmea_line[strlen(nmea_line)] = data;
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

static lv_obj_t *create_second_screen(void)
{
    lv_obj_t *second_screen;
    lv_obj_t *label;

    second_screen = lv_obj_create(NULL);

    label=lv_label_create(second_screen);
    lv_label_set_recolor(label, true);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &SourceSansProSemiBold18_4bpp, LV_STATE_DEFAULT);
    lv_label_set_text(label, "Second screen!");
    lv_obj_set_pos(label, 10, 50);

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
    lv_obj_set_pos(date_label, 40, 120);

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
       DEBUG("%s\n", lstr);
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
       strncat(lstr,"#ffffff " LV_SYMBOL_BLUETOOTH "# ", 31);
       if (gpio_read(GPS_PWR))
           strncat(lstr,"#ffffff " LV_SYMBOL_GPS "# ", 31);
       lv_label_set_text(icon_label, lstr);
   }
}

void *event_thread(void *arg)
{
    (void) arg;
    watchy_event_t ev;
    bool wake_lvgl=false;
    static uint8_t bl_timeout=5;
 
    while (true) {
      while (watchy_event_queue_length()) {
          // DEBUG("ev=%d\n", watchy_event_queue_length());
          ev=watchy_event_queue_get();
          switch (ev) {
              case EV_SEC_TICK:
                  if (watch_state.clock.tm_sec==0) {
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
                  bl_timeout=5;
                  //lpm013m126_on();
                  xdisplay_on();
                  // print_tdata(&_tdata);
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
                    wake_lvgl=true;
                  }
                  break;
              case EV_BUTTON:
                  DEBUG("btn\n");
                  bl_timeout=5;
                  //lpm013m126_on();
                  xdisplay_on();
                  break;
              case EV_ACCEL:
                  break;
              case EV_MAGNETOMETER:
                  break;
              case EV_GNSS:
                  {
                    // DEBUG("\n%d '%s'\n", minmea_sentence_id(nmea_line, false), nmea_line);
                    if (minmea_sentence_id(nmea_line, false) == MINMEA_SENTENCE_ZDA) {
                      struct minmea_sentence_zda frame;
                      int res = minmea_parse_zda(&frame, nmea_line);
                      if (!res) {
                        puts("FAILURE: error parsing GPS sentence");
                      } else {
                        // DEBUG("ZDA %d.%d.%d %d:%d.%d\n", frame.date.day, frame.date.month, frame.date.year, frame.time.hours, frame.time.minutes, frame.time.seconds);
                        if (frame.time.hours != -1) {
                          // time is valid now
                          watch_state.gnss_state.time_valid = true;
                          watch_state.clock.tm_hour = frame.time.hours + 2;
                          watch_state.clock.tm_hour %= 24;
                          watch_state.clock.tm_min = frame.time.minutes;
                          watch_state.clock.tm_sec = frame.time.seconds;
                        } else
                          watch_state.gnss_state.time_valid = false;
                        if (frame.date.day > 0) {
                          watch_state.gnss_state.date_valid = true;
                          watch_state.clock.tm_mday = frame.date.day;
                          watch_state.clock.tm_mon = frame.date.month - 1;
                          watch_state.clock.tm_year = frame.date.year - TM_YEAR_OFFSET;
                        } else
                          watch_state.gnss_state.date_valid = false;
                      }
                    }
                    // clear line for new msgs
                    memset(nmea_line, 0, NMEA_LINE_BUF_LEN);
                  };
                  break;
              case EV_ATMOSPHERE:
                  break;
              case EV_DISPLAY_TIMEOUT:
                  break;
              case EV_POWER_CHANGE:
                  update_main_screen();
                  bl_timeout=5;
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


int main(void)
{
    static ztimer_periodic_t timer;

    memset(&watch_state, 0, sizeof(watchy_state_t));

    watch_state.clock.tm_year = 2022 - TM_YEAR_OFFSET;
    watch_state.clock.tm_mon = 2;
    watch_state.clock.tm_mday = 13;
    watch_state.clock.tm_hour = 1;
    watch_state.clock.tm_min = 0;
    watch_state.clock.tm_sec = 0;

    ztimer_periodic_init(ZTIMER_SEC, &timer, rtc_second_cb, NULL, 1);
    ztimer_periodic_start(&timer);

    // init LCD, display logo and enable backlight
//    lpm013m126_init(&_disp_dev, &lpm013m126_params);
//    display_logo(&_disp_dev);
    pwm_set(PWM_DEV(0), 0, 80);
    pwm_poweron(PWM_DEV(0));

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

    // DEBUG("c=%d\n", sizeof(lv_color_t));
    lv_main_screen = create_main_screen();
    update_main_screen();
    lv_second_screen = NULL;

    lv_disp_load_scr(lv_main_screen);

    watchy_event_queue_add(EV_SEC_TICK);
    thread_wakeup(event_thread_pid);

    xdisplay_on();

    lvgl_run();

    // shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
