#ifdef RTT_CONSOLE
#include <stdio.h>
#include <stdlib.h>
#include <stdio_rtt.h>
#endif
#include <string.h>
#include <math.h>

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

#define ENABLE_DEBUG 0
#include "debug.h"

#include "watchy.h"
#include "watchy_events.h"

#include "magneto.h"

// reading from 0x3e starts one measuring cycle
#define MAGNETO_SINGLE 0x3e
#define MAGNETO_STATUS 0x4e
#define MAGNETO_STATUS_OK (1U << 1)
#define MAGNETO_STATUS_UN1 (1U << 2)
#define MAGNETO_STATUS_UN2 (1U << 4)
#define MAGNETO_STATUS_BUSY (1U << 5)
#define MAGNETO_Y_L 0x4f
#define MAGNETO_Y_H 0x50
#define MAGNETO_X_L 0x51
#define MAGNETO_X_H 0x52
#define MAGNETO_Z_L 0x53
#define MAGNETO_Z_H 0x54

// Y = hochachse, -90 deg

#ifndef M_PI
#define M_PI    (3.14159265358979323846)
#endif

static uint16_t xmin=0xffff, xmax=0, ymin=0xffff, ymax=0, zmin=0xffff, zmax=0;

int magneto_course(uint16_t x, uint16_t y, uint16_t z)
{
    int course=-1;
    (void) z;

    course = atan2(x, y) * 180 / M_PI;

    return course;
}

int magneto_read(uint16_t *x, uint16_t *y, uint16_t *z)
{
    uint8_t xyz_val[7];
    uint8_t val;

    i2c_acquire(MAGN_I2C_DEV);

    val = 0;
    do {
        if (i2c_read_regs(MAGN_I2C_DEV, MAGN_I2C_ADDR, MAGNETO_STATUS, &xyz_val, 7, 0) < 0) {
            DEBUG("magneto i2c_read_regs() failed\n");
            goto fail;
        }
        if (xyz_val[0] == MAGNETO_STATUS_OK)
            break;
        if (++val > 200)
            goto fail;
    } while (xyz_val[0] & (MAGNETO_STATUS_UN2 | MAGNETO_STATUS_BUSY));


#if 0
    *y = xyz_val[1] | (xyz_val[2] << 8);
    *x = xyz_val[3] | (xyz_val[4] << 8);
    *z = xyz_val[5] | (xyz_val[6] << 8);
#else
    *y = xyz_val[2] | (xyz_val[1] << 8);
    if (*y > ymax) ymax = *y;
    if (*y < ymin) ymin = *y;
    *x = xyz_val[4] | (xyz_val[3] << 8);
    if (*x > xmax) xmax = *x;
    if (*x < xmin) xmin = *x;
    *z = xyz_val[6] | (xyz_val[5] << 8);
    if (*z > zmax) zmax = *z;
    if (*z < zmin) zmin = *z;
#endif
    i2c_read_regs(MAGN_I2C_DEV, MAGN_I2C_ADDR, MAGNETO_SINGLE, &val, 1, 0);

    i2c_release(MAGN_I2C_DEV);
    return xyz_val[0];

fail:
    i2c_release(MAGN_I2C_DEV);

    return -1;
}

int magneto_init(void)
{
    uint8_t val=0;

    i2c_acquire(MAGN_I2C_DEV);

    if (i2c_read_regs(MAGN_I2C_DEV, MAGN_I2C_ADDR, MAGNETO_SINGLE, &val, 1, 0) < 0) {
        goto fail;
    }
    if (val == 0x22) {
        // DEBUG("magneto found\n");
    } else
        DEBUG("magneto 0x3e=0x%02x\n", val);

    i2c_release(MAGN_I2C_DEV);

    return 0;

fail:
    i2c_release(MAGN_I2C_DEV);

    return -1;
}
