#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <string.h>

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
#include "periph/i2c.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#include "watchy.h"
#include "watchy_events.h"

#include "kx023-1025.h"

#define KX0231025_COTR 0X0C
#define KX0231025_WHO_AM_I 0X0F
#define KX0231025_CTRL1 0X18
#define KX0231025_CTRL2 0X19
#define KX0231025_CTRL3 0X20
#define KX0231025_ODCNTL 0X1B

#define KX0231025_STATUS_REG 0X15

#define KX0231025_OUTX_L_XL 0X06
#define KX0231025_OUTX_H_XL 0X07
#define KX0231025_OUTY_L_XL 0X08
#define KX0231025_OUTY_H_XL 0X09
#define KX0231025_OUTZ_L_XL 0X10
#define KX0231025_OUTZ_H_XL 0X11

#define KX0231025_RANGE_2G 0
#define KX0231025_RANGE_4G 1
#define KX0231025_RANGE_8G 2

#define KX0231025_DATARATE_12_5HZ 0
#define KX0231025_DATARATE_25HZ 1
#define KX0231025_DATARATE_50HZ 2
#define KX0231025_DATARATE_100HZ 3
#define KX0231025_DATARATE_200HZ 4
#define KX0231025_DATARATE_400HZ 5
#define KX0231025_DATARATE_800HZ 6
#define KX0231025_DATARATE_1600HZ 7
#define KX0231025_DATARATE_0_781HZ 8
#define KX0231025_DATARATE_1_563HZ 9
#define KX0231025_DATARATE_3_125HZ 10
#define KX0231025_DATARATE_6_25HZ 11

#define KX0231025_LOWPOWER_MODE 0X00
#define KX0231025_HIGHPOWER_MODE 0X40


static void _kx023_int(void *arg)
{
    (void) arg;
    watchy_event_queue_add(EV_ACCEL);
}

int kx023_init(void)
{
    //int res;
    uint8_t whoami=0;
    uint8_t cotr=0;

    i2c_acquire(I2C_DEV(1));
    if (i2c_read_regs(I2C_DEV(1), ACCEL_I2C_ADDR, KX0231025_WHO_AM_I, &whoami, 1, 0) < 0) {
        DEBUG("KX023 i2c_read_regs() failed\n");
    }

    if (i2c_read_regs(I2C_DEV(1), ACCEL_I2C_ADDR, KX0231025_COTR, &cotr, 1, 0) < 0) {
        DEBUG("KX023 i2c_read_regs() failed\n");
    }

    if (whoami == 0x14) { // in the BangleJS2 the ID is 0x14
        DEBUG("KX023 found, COTR=0x%02x\n", cotr);
        gpio_init_int(ACCEL_INT, GPIO_IN, GPIO_RISING, _kx023_int, NULL);
    } else {
        DEBUG("KX023 _not_ found 0x%02x\n", whoami);
    }

    i2c_release(I2C_DEV(1));

    return 0;
}
