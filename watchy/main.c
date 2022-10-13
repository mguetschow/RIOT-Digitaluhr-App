/*
 * Copyright (C) 2022 Nicole Faerber, <nicole.faerber@digitaluhr-manufaktur.de>
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
#include <rtc_utils.h>
#include <byteorder.h>

#include <shell.h>
//#include "shell_commands.h"

#include <clk.h>
#include <board.h>
#include <irq.h>
#include <periph_conf.h>
#include <ringbuffer.h>
#include <timex.h>
#include <ztimer.h>
#include <ztimer/periodic.h>
#include <thread.h>
#include <tm.h>
#include <periph/gpio.h>
#include <periph/pm.h>
#include <periph/adc.h>
#include <periph/spi.h>
#include <periph/pwm.h>
#include <periph/i2c.h>
#include <periph/uart.h>
#include <cst816s.h>

#include <bmx280.h>

#include <lvgl/lvgl.h>
#include <lvgl_riot.h>
#include <disp_dev.h>

#include <minmea.h>

#include "watchy.h"
#include "watchy_events.h"
#include "watchy_shell.h"
#include "gnss.h"

#include "nimble_riot.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "net/bluetil/ad.h"

#include "nimble_autoadv.h"

#include "gatt-adv.h"

#include "kx023-1025.h"
#include "magneto.h"
#include "vc31.h"

#include "weatherstation.h"

#include <screens.h>

#define ENABLE_DEBUG 1
#include "debug.h"

// #include "rl2.h"

static uint32_t button_ev = false;
static char shell_thread_stack[THREAD_STACKSIZE_SMALL+THREAD_EXTRA_STACKSIZE_PRINTF];
static char event_thread_stack[THREAD_STACKSIZE_SMALL+THREAD_EXTRA_STACKSIZE_PRINTF];

watchy_state_t watch_state;

static char nmea_line[NMEA_LINE_BUF_LEN];

#define DISPLAY_TIMEOUT 5

bmx280_t bmx280_dev;
static const bmx280_params_t bmx280_params[] =
{
  {
    .i2c_dev  = ATM_PRESSURE_I2C_DEV,
    .i2c_addr = ATM_PRESSURE_I2C_ADDR,
    .t_sb = BMX280_SB_0_5,
    .filter = BMX280_FILTER_OFF,
    .run_mode = BMX280_MODE_FORCED,
    .temp_oversample = BMX280_OSRS_X1,
    .press_oversample = BMX280_OSRS_X1,
    .humid_oversample = BMX280_OSRS_X1,
  }
};
#define BMX280_NUMOF    ARRAY_SIZE(bmx280_params)

static cst816s_t _input_dev;
static cst816s_touch_data_t _tdata;

static const cst816s_params_t _cst816s_input_params = {
	.i2c_dev = I2C_DEV(0),
	.i2c_addr = TOUCH_I2C_ADDR,
	.irq = TOUCH_INT,
	.irq_flank = GPIO_FALLING,
	.reset = TOUCH_RESET,
};


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
	// linear approximation of battery % from full (chg voltage 4.2V) to
	// minimum (MIN_BAT_VOLT)
	// at the low current that the device draws
	// the discharge curve is almost linear
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

static void touch_cb(void *arg)
{
	(void) arg;

	// can not read I2C from IRQ context, just set a flag
	watchy_event_queue_add(EV_TOUCH);
	//thread_wakeup(watch_state.event_thread_pid);
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
        button_ev = 1;
    }
    watchy_event_queue_add(EV_BUTTON);
    //thread_wakeup(watch_state.event_thread_pid);
}

static void _ext_power_cb(void *arg)
{
    (void) arg;
    
    watchy_event_queue_add(EV_POWER_CHANGE);
    // thread_wakeup(event_thread_pid);
}


void uart_rx_cb(void *arg, uint8_t data)
{
  (void) arg;

  if (data == '\n') {
      watchy_event_queue_add(EV_GNSS);
      //thread_wakeup(watch_state.event_thread_pid);
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


void *event_thread(void *arg)
{
    (void) arg;
    watchy_event_t ev;
    bool wake_lvgl=false;
    static uint8_t bl_timeout=DISPLAY_TIMEOUT;
 
    while (true) {
      while (watchy_event_queue_length()) {
          // DEBUG("ev=%d ", watchy_event_queue_length());
          ev=watchy_event_queue_get();
          // DEBUG("%d\n", ev);
          switch (ev) {
              case EV_MSEC_TICK:
                  break;
              case EV_SEC_TICK:
                  if (bl_timeout) {
                    bl_timeout--;
                  } else {
                      //lpm013m126_off();
                      xdisplay_off();
                  }
                  break;
              case EV_MIN_TICK:
                  // wake_lvgl=true;
                  weather_update_readings(&watch_state.clock);
                  get_power_stat(&watch_state.pwr_stat);
                  // update_main_screen();
                  break;
              case EV_HOUR_TICK:
                  break;
              case EV_TOUCH:
                  cst816s_read(&_input_dev, &_tdata);
                  // print_tdata(&_tdata);
                  watch_state.touch_state.x = _tdata.x;
                  watch_state.touch_state.y = _tdata.y;
                  if (_tdata.action == CST816S_TOUCH_UP) {
                    switch (_tdata.gesture) {
                      case CST816S_GESTURE_SLIDE_UP:
                        watch_state.touch_state.gesture = TOUCH_G_SWP_UP;
                        break;
                      case CST816S_GESTURE_SLIDE_DOWN:
                        watch_state.touch_state.gesture = TOUCH_G_SWP_DOWN;
                        break;
                      case CST816S_GESTURE_SLIDE_LEFT:
                        watch_state.touch_state.gesture = TOUCH_G_SWP_LEFT;
                        break;
                      case CST816S_GESTURE_SLIDE_RIGHT:
                        watch_state.touch_state.gesture = TOUCH_G_SWP_RIGHT;
                        break;
                      case CST816S_GESTURE_LONG_PRESS:
                        watch_state.touch_state.gesture = TOUCH_G_LONG_PRESS;
                        break;
                      default:
                        watch_state.touch_state.gesture = TOUCH_G_NONE;
                        break;
                    }
                    wake_lvgl=true;
                    xdisplay_on();
                    bl_timeout=DISPLAY_TIMEOUT;
                  } else if (_tdata.action == CST816S_TOUCH_DOWN && _tdata.gesture==CST816S_GESTURE_SINGLE_CLICK) {
                        watch_state.touch_state.gesture = TOUCH_G_CLICK;
                        watch_state.touch_state.clicked = true;
                        wake_lvgl=true;
                        xdisplay_on();
                        bl_timeout=DISPLAY_TIMEOUT;
                   } else
                    watch_state.touch_state.gesture = TOUCH_G_NONE;
                  break;
              case EV_BUTTON:
                  DEBUG("btn\n");
                  bl_timeout=DISPLAY_TIMEOUT;
                  //lpm013m126_on();
                  xdisplay_on();
#if 0
                  if (lv_main_screen==NULL) {
                      lv_main_screen = create_main_screen();
                      update_main_screen();
                      lv_scr_load_anim(lv_main_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 250, 0, true);
                      lv_second_screen = NULL;
                  }
#endif
                  break;
              case EV_ACCEL:
                  DEBUG("acc\n");
                  break;
              case EV_MAGNETOMETER:
                  DEBUG("mag\n");
                  break;
              case EV_GNSS:
              	  // DEBUG("g: %s\n", nmea_line);
                  handle_gnss_event(nmea_line, &watch_state);
                  break;
              case EV_ATMOSPHERE:
                  DEBUG("atmo\n");
                  break;
              case EV_DISPLAY_TIMEOUT:
                  break;
              case EV_POWER_CHANGE:
                  get_power_stat(&watch_state.pwr_stat);
                  // update_main_screen();
                  bl_timeout=DISPLAY_TIMEOUT;
                  //lpm013m126_on();
                  xdisplay_on();
                  // wake_lvgl = true;
                  break;
              case EV_UPDATE_DISPLAY:
                  wake_lvgl = true;
                  break;
              case EV_INFO_NOTE:
                  break;
              case EV_BT_CONN:
                  break;
              case EV_BT_NUS: {
                  char buf[32];
                  watchy_gatt_nus_get_rx(buf, 30);
                  DEBUG("NUS RX '%s'\n", buf);
                  gatt_svr_nus_tx_buf(buf, strlen(buf));
                  break;
              }
              default:
                  DEBUG("no event?\n");
                  break;
          }; // switch()
          // when system is done let the screen know
          screens_handle_event(ev);
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


static bool rtc_second_cb(void *arg)
{
   (void) arg;

   watch_state.rtc_time++;
   rtc_localtime(watch_state.rtc_time, &watch_state.clock);

   watchy_event_queue_add(EV_SEC_TICK);
   // we just rolled over to a full minute
   if (watch_state.clock.tm_sec == 0)
     watchy_event_queue_add(EV_MIN_TICK);
   // we just rolled over to a full hour
   if (watch_state.clock.tm_sec == 0 && watch_state.clock.tm_min ==0)
     watchy_event_queue_add(EV_HOUR_TICK);

#if 0
   watch_state.clock.tm_sec++;
   if (watch_state.clock.tm_sec > 59) {
     watch_state.clock.tm_sec=0;
     watch_state.clock.tm_min++;
     watchy_event_queue_add(EV_MIN_TICK);
   }
   if (watch_state.clock.tm_min > 59) {
     watch_state.clock.tm_min=0;
     watch_state.clock.tm_hour++;
     watchy_event_queue_add(EV_HOUR_TICK);
   }
   if (watch_state.clock.tm_hour > 23) {
     watch_state.clock.tm_hour=0;
     watch_state.clock.tm_mday++;
     tm_fill_derived_values(&watch_state.clock);
   }
#endif

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


int main(void)
{
	static ztimer_periodic_t timer;
	static lv_indev_drv_t indev_drv;

	memset(&watch_state, 0, sizeof(watchy_state_t));

	watch_state.clock.tm_year = 2022 - TM_YEAR_OFFSET;
	watch_state.clock.tm_mon = 10;
	watch_state.clock.tm_mday = 9;
	watch_state.clock.tm_hour = 8;
	watch_state.clock.tm_min = 0;
	watch_state.clock.tm_sec = 0;
	watch_state.rtc_time = rtc_mktime(&watch_state.clock);

	ztimer_periodic_init(ZTIMER_SEC, &timer, rtc_second_cb, NULL, 1);
	ztimer_periodic_start(&timer);

	watch_state.gnss_pwr = false;
	watch_state.bluetooth_pwr = BT_OFF;

	strncpy(watch_state.info, "This is a way too long text to be displayed in these two lines" , 63);

	// init LCD, display logo and enable backlight
	// lpm013m126_init(&_disp_dev, &lpm013m126_params);
	// display_logo(&_disp_dev);

	// init backlight PWM
	pwm_set(PWM_DEV(0), 0, 20);
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
	} else
		uart_poweroff(UART_DEV(0));


	watch_state.event_thread_pid=thread_create(event_thread_stack, sizeof(event_thread_stack),
		THREAD_PRIORITY_IDLE - 1, THREAD_CREATE_STACKTEST,
		event_thread, NULL, "event_thread");
	//DEBUG("eventthr=%d\n", watch_state.event_thread_pid);

	watch_state.shell_thread_pid=thread_create(shell_thread_stack, sizeof(shell_thread_stack),
		THREAD_PRIORITY_IDLE - 1, THREAD_CREATE_STACKTEST,
		shell_thread, NULL, "shell_thread");
	//DEBUG("shellthr=%d\n", watch_state.shell_thread_pid);

   	get_power_stat(&watch_state.pwr_stat);

	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = lv_input_cb;
	lv_indev_drv_register(&indev_drv);

	screens_init();

	watchy_event_queue_add(EV_SEC_TICK);
	//thread_wakeup(watch_state.event_thread_pid);

	watchy_gatt_init();
	//extern void stdio_ble_init(void);
	//stdio_ble_init();


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
        // DEBUG("BMX280 Initialization successful\n");

        kx023_init();

        magneto_init();

        vc31_init();

        weatherstation_init();
        weather_update_readings(&watch_state.clock);

	lvgl_run();

	return 0;
}
