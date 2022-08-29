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
#include "ztimer.h"
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
#define KX0231025_CNTL1 0X18
#define KX0231025_CNTL2 0X19
#define KX0231025_CNTL3 0X20
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

#define KX0231025_TPE (1U << 0)
#define KX0231025_PC1 (1U << 7)

#define KX0231025_BUF_CNTL2 0x3b
#define KX0231025_BUFE (1U << 7)
#define KX0231025_BRES (1U << 6)
#define KX0231025_BFIE (1U << 5)
#define KX0231025_BUF_M0 (1U << 0)
#define KX0231025_BUF_M1 (1U << 1)

static void _kx023_int(void *arg)
{
    (void) arg;
    watchy_event_queue_add(EV_ACCEL);
}

int kx023_read_accel(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t xyz_val[6];

    i2c_acquire(ACCEL_I2C_DEV);
    if (i2c_read_regs(ACCEL_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_OUTX_L_XL, &xyz_val, 6, 0) < 0) {
        DEBUG("KX023 i2c_read_regs() failed\n");
        goto fail;
    }
    i2c_release(ACCEL_I2C_DEV);

#if 1
    *x = xyz_val[0] | (xyz_val[1] << 8);
    *y = xyz_val[2] | (xyz_val[3] << 8);
    *z = xyz_val[4] | (xyz_val[5] << 8);
#else
    *x = *(uint16_t *)xyz_val;
    *y = *(uint16_t *)xyz_val+2;
    *z = *(uint16_t *)xyz_val+4;
#endif
    return 0;

fail:
    i2c_release(ACCEL_I2C_DEV);

    return -1;
}

// turns off the engine
int kx023_deactivate(void)
{
    uint8_t val=0;

    i2c_acquire(ACCEL_I2C_DEV);

    i2c_write_regs(ACCEL_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_CNTL1, &val, 1, 0);

    i2c_release(ACCEL_I2C_DEV);

    return 0;
}

// turns on the engine
int kx023_activate(void)
{
    uint8_t val=0;

    i2c_acquire(ACCEL_I2C_DEV);

    val = KX0231025_BRES;
    i2c_write_regs(ACCEL_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_BUF_CNTL2, &val, 1, 0);

    val=KX0231025_LOWPOWER_MODE | KX0231025_RANGE_2G | KX0231025_TPE | KX0231025_PC1;
    i2c_write_regs(ACCEL_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_CNTL1, &val, 1, 0);

    i2c_release(ACCEL_I2C_DEV);

    return 0;
}

//
// init will only check for device presence and set some basic
// parameters but keep the device in standby / low power state
//
int kx023_init(void)
{
    //int res;
    uint8_t whoami=0;
    uint8_t cotr=0;
    uint8_t val=0;

    i2c_acquire(ACCEL_I2C_DEV);
    if (i2c_read_regs(ACCEL_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_WHO_AM_I, &whoami, 1, 0) < 0) {
        DEBUG("KX023 i2c_read_regs() failed\n");
        goto fail;
    }

    if (i2c_read_regs(ACCEL_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_COTR, &cotr, 1, 0) < 0) {
        DEBUG("KX023 i2c_read_regs() failed\n");
        goto fail;
    }

    if (whoami == 0x14) { // in the BangleJS2 the ID is 0x14
        DEBUG("KX023 found, COTR=0x%02x\n", cotr);
        gpio_init_int(ACCEL_INT, GPIO_IN, GPIO_RISING, _kx023_int, NULL);
    } else {
        DEBUG("KX023 _not_ found 0x%02x\n", whoami);
        goto fail;
    }

    val=0x00;
    i2c_write_regs(ACCEL_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_CNTL1, &val, 1, 0);

    i2c_release(ACCEL_I2C_DEV);

    return 0;

fail:
    i2c_release(ACCEL_I2C_DEV);

    return -1;
}
