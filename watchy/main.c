/*
 * Copyright (C) 2022 Nicole Faerber, Digitaluhr Manufaktur
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
#include "shell.h"
#include "shell_commands.h"


#include "clk.h"
#include "board.h"
#include "irq.h"
#include "periph_conf.h"
#include "periph/gpio.h"
#include "timex.h"
#include "xtimer.h"
#include "thread.h"
#include "periph/pm.h"
#include "periph/adc.h"
#include "periph/pwm.h"
#include "periph/spi.h"
#include "cst816s.h"

#include "lpm013m126.h"

static char shell_thread_stack[THREAD_STACKSIZE_SMALL+THREAD_EXTRA_STACKSIZE_PRINTF];
static char line_buf[SHELL_DEFAULT_BUFSIZE];
//static uint32_t dttick=0;
static xtimer_ticks32_t last_wakeup;

extern cst816s_t _input_dev;
extern char touch_ev;
extern cst816s_touch_data_t tdata;
static uint8_t buf[5+((176/2)*16)];

int _cmd_time(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("t: %ld\n", (uint32_t)last_wakeup.ticks32);

return 0;
}

int _cmd_off(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    board_power_off();

    return 0;
}

int _cmd_bat(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    int32_t bvolt;

    printf("ext power %spresent\n", gpio_read(EXTPOWER_PRESENT) ? "not " : "");
    printf("chg complete %s\n", gpio_read(CHARGE_COMPLETE) ? "yes" : "no");
    bvolt=(int32_t)adc_sample(1, ADC_RES_12BIT);
    //printf("%d.%dV\n", (bvolt/1000), (bvolt%1000));
    printf("%ldmV\n", 3300*(uint32_t)bvolt/(4095/4));

    return 0;
}

int _cmd_bl(int argc, char **argv)
{
#if 0
unsigned char onoff=0;

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: %s [1/0]\n", argv[0]);

        return 0;
    }
    if (argc >= 2) {
        onoff = (unsigned char)atoi(argv[1]);
        if (onoff)
            gpio_set(LCD_BACKLIGHT);
        else
            gpio_clear(LCD_BACKLIGHT);
    } else {
        gpio_toggle(LCD_BACKLIGHT);
    }
#else
    uint16_t bright;

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: %s brightness\n", argv[0]);

        return 0;
    }
    if (argc >= 2) {
        bright = (uint16_t)atoi(argv[1]);
        if (bright)
            pwm_set(PWM_DEV(0), 0, bright);
        else
            pwm_set(PWM_DEV(0), 0, 0);
    }
#endif

return 0;
}

int _cmd_disp(int argc, char **argv)
{
unsigned char onoff=0;

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: %s [1/0]\n", argv[0]);

        return 0;
    }
    if (argc >= 2) {
        onoff = (unsigned char)atoi(argv[1]);
        if (onoff)
            gpio_set(LCD_DISP);
        else
            gpio_clear(LCD_DISP);
    } else {
        gpio_toggle(LCD_DISP);
    }

return 0;
}


#define LCD_COLOR_CMD_UPDATE            (0x90) // Update Mode (4bit Data Mode)
#define LCD_COLOR_CMD_ALL_CLEAR         (0x20) // All Clear Mode
#define LCD_COLOR_CMD_NO_UPDATE         (0x00) // No Update Mode
#define LCD_COLOR_CMD_BLINKING_WHITE    (0x18) // Display Blinking Color Mode (White)
#define LCD_COLOR_CMD_BLINKING_BLACK    (0x10) // Display Blinking Color Mode (Black)
#define LCD_COLOR_CMD_INVERSION         (0x14) // Display Inversion Mode

int _cmd_lcd(int argc, char **argv)
{
    if ((argc<2) || ((argc == 2) && (memcmp(argv[1], "help", 4) == 0))) {
        printf("usage: %s [clear,blink_white,blink_black,blink_stop,invert,red,green,blue,white]\n", argv[0]);

        return 0;
    }
    if (memcmp(argv[1], "clear", 5) == 0) {
        lpm013m126_clear();
    }
    if (memcmp(argv[1], "blink_white", 11) == 0) {
        buf[0]=LCD_COLOR_CMD_BLINKING_WHITE;
        buf[1]=0x00;
        lpm013m126_write(buf, 2);
    }
    if (memcmp(argv[1], "blink_black", 11) == 0) {
        buf[0]=LCD_COLOR_CMD_BLINKING_BLACK;
        buf[1]=0x00;
        lpm013m126_write(buf, 2);
    }
    if (memcmp(argv[1], "blink_stop", 10) == 0) {
        lpm013m126_noupdate();
    }
    if (memcmp(argv[1], "invert", 6) == 0) {
        lpm013m126_invert();
    }
    if (memcmp(argv[1], "red", 3) == 0) {
        for (int l=0; l<16; l++) {
          if (l==0)
              buf[l*90]=LCD_COLOR_CMD_UPDATE;
          else
              buf[l*90]=0x00; 
          buf[l*90+1]=l;
          for (int p=0; p<(176/2); p++)
            buf[l*90+p+2]=0x88;
        }
        lpm013m126_write(buf, 16*90+2+2);
    }
    if (memcmp(argv[1], "green", 5) == 0) {
        for (int l=0; l<10; l++) {
          if (l==0)
              buf[l*90]=LCD_COLOR_CMD_UPDATE;
          else
              buf[l*90]=0x00; 
          buf[l*90+1]=l;
          for (int p=0; p<(176/2); p++)
            buf[l*90+p+2]=0x44;
        }
        lpm013m126_write(buf, 10*90+2+2);
    }
    if (memcmp(argv[1], "blue", 4) == 0) {
        for (int l=0; l<10; l++) {
          if (l==0)
              buf[l*90]=LCD_COLOR_CMD_UPDATE;
          else
              buf[l*90]=0x00; 
          buf[l*90+1]=l;
          for (int p=0; p<(176/2); p++)
            buf[l*90+p+2]=0x22;
        }
        lpm013m126_write(buf, 10*90+2+2);
    }
    if (memcmp(argv[1], "white", 5) == 0) {
        for (int l=0; l<10; l++) {
          if (l==0)
              buf[l*90]=LCD_COLOR_CMD_UPDATE;
          else
              buf[l*90]=0x00; 
          buf[l*90+1]=l;
          for (int p=0; p<(176/2); p++)
            buf[l*90+p+2]=0xee;
        }
        lpm013m126_write(buf, 10*90+2+2);
    }
    if (memcmp(argv[1], "lines", 5) == 0) {
        for (int p=0; p<(10*(176/2)); p++)
            buf[p]=0x44;
        memset(buf, 0x44, sizeof(buf));
        lpm013m126_write_lines(buf, 80, 10);
    }

    return 0;
}

static const shell_command_t _commands[] = {
    { "bat", "get battery state", _cmd_bat },
    { "bl", "toggle or on/off LCD backlight", _cmd_bl },
    { "disp", "toggle or on/off LCD", _cmd_disp },
    { "lcd", "LCD commands", _cmd_lcd },
    { "off", "power off device", _cmd_off },
    { "time", "print dttick", _cmd_time },
    { NULL, NULL, NULL }
};

#if 1
static void button_cb(void *arg)
{
    (void) arg;
    printf("Button %s!\n", gpio_read(BUTTON0) ? "released" : "pressed");
}
#endif
void *shell_thread(void *arg)
{
    (void) arg;
 
    shell_run(_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return NULL;
}

#if 0
static const cst816s_params_t _cst816s_input_params = {
    .i2c_dev = I2C_DEV(0),
    .i2c_addr = TOUCH_I2C_ADDR,
    .irq = TOUCH_INT,
    .irq_flank = GPIO_FALLING,
    .reset = TOUCH_RESET,
};
#endif

#if 0
void touch_cb(void *arg)
{
    (void) arg;

    // can not read I2C from IRQ context, just set a flag
    touch_ev=1;
}
#endif

int main(void)
{
//    uint8_t start=0, color=0x00;

//    printf("PWM: %ld\n", pwm_init(PWM_DEV(1), PWM_RIGHT, 1250, 100));
//    pwm_set(PWM_DEV(1), 0, 50); // 50% duty cylce @ 125kHz for EXTCOM ?
//    pwm_poweron(PWM_DEV(1));

    // we want display on when we power up
    lpm013m126_init();

//    i2c_init(I2C_DEV(0));

//    if (cst816s_init(&_input_dev, &_cst816s_input_params, touch_cb, NULL) != CST816S_OK) {
//        printf("cst init fail\n");
//    };

//    printf("PWM: %ld\n", pwm_init(PWM_DEV(0), PWM_RIGHT, 1250, 100));
//    pwm_set(PWM_DEV(0), 0, 80); // 80% brightness
//    pwm_poweron(PWM_DEV(0));

//    adc_init(1);
    gpio_init_int(BUTTON0, GPIO_IN_PU, GPIO_BOTH, button_cb, NULL);

#if 0
    {
        uint32_t *reg;
        reg=(uint32_t *)0x40003554;
        printf("SPI0 config=0x%04lx\n", *reg);
        reg=(uint32_t *)0x40003524;
        printf("SPI0 freq=0x%04lx\n", *reg);
    }
#endif
#if 1
    thread_create(shell_thread_stack, sizeof(shell_thread_stack),
                  THREAD_PRIORITY_IDLE - 1, THREAD_CREATE_STACKTEST,
                  shell_thread, NULL, "shell_thread");
#endif
    last_wakeup = xtimer_now();

    // for some reason only the second one will really do it!?
    lpm013m126_clear();
//    lpm013m126_clear();

    memset(buf, 0, sizeof(buf));
    while (1) {
        xtimer_periodic_wakeup(&last_wakeup, US_PER_SEC / 100);
#if 0
#if 0
        {
            //lpm013m126_clear();
            memset(buf, color++, sizeof(buf));
            lpm013m126_write_lines(buf, start, 16);
            start++;
            start = start % 176;
        }
#endif
//        xtimer_usleep(US_PER_SEC / 10);
//        gpio_set(LCD_EXTCOMIN);
//        dttick++;
//        gpio_clear(LCD_EXTCOMIN);
        if (touch_ev) {
            cst816s_read(&_input_dev, &tdata);
            touch_ev=0;
            //printf("touch ");
#if 0
            switch (tdata.action) {
                case CST816S_TOUCH_DOWN:
                    printf("down ");
                    break;
                case CST816S_TOUCH_UP:
                    printf("up ");
                    break;
                case CST816S_TOUCH_CONTACT:
                    printf("contact ");
                    break;
                default:
                    break;
            };
            switch (tdata.gesture) {
                case CST816S_GESTURE_NONE:
                    printf("gnone ");
                    break;
                case CST816S_GESTURE_SLIDE_DOWN:
                    printf("gslide_dwn ");
                    break;
                case CST816S_GESTURE_SLIDE_UP:
                    printf("gslide_up ");
                    break;
                case CST816S_GESTURE_SLIDE_LEFT:
                    printf("gslide_left ");
                    break;
                case CST816S_GESTURE_SLIDE_RIGHT:
                    printf("gslide_right ");
                    break;
                case CST816S_GESTURE_SINGLE_CLICK:
                    printf("gsclick ");
                    break;
                case CST816S_GESTURE_DOUBLE_CLICK:
                    printf("gdclick ");
                    break;
                case CST816S_GESTURE_LONG_PRESS:
                    printf("glpress ");
                    break;
                default:
                    break;
            }
#endif
//            printf("%d,%d\n", tdata.x, tdata.y);
            memset(buf, 0, sizeof(buf));
            buf[(tdata.x/2)-1]=0xff;
            buf[(tdata.x/2)-0]=0xff;
            buf[(tdata.x/2)+1]=0xff;
            buf[(tdata.x/2)-1+88]=0xff;
            buf[(tdata.x/2)-0+88]=0xff;
            buf[(tdata.x/2)+1+88]=0xff;
            buf[(tdata.x/2)-1+176]=0xff;
            buf[(tdata.x/2)-0+176]=0xff;
            buf[(tdata.x/2)+1+176]=0xff;
            lpm013m126_write_lines(buf, tdata.y, 3);
        }
#endif
    };
    return 0;
}
