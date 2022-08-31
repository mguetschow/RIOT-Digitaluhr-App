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


int magneto_read(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t xyz_val[7];
    uint8_t val;

    i2c_acquire(MAGN_I2C_DEV);
    do {
        if (i2c_read_regs(MAGN_I2C_DEV, MAGN_I2C_ADDR, MAGNETO_STATUS, &xyz_val, 7, 0) < 0) {
            DEBUG("magneto i2c_read_regs() failed\n");
            goto fail;
        }
        if (xyz_val[0] == 0x02)
            break;
    } while ((xyz_val[0] == 0x16) || (xyz_val[0] == 0x32));

    i2c_read_regs(MAGN_I2C_DEV, MAGN_I2C_ADDR, MAGNETO_SINGLE, &val, 1, 0);

    i2c_release(MAGN_I2C_DEV);

    *y = xyz_val[1] | (xyz_val[2] << 8);
    *x = xyz_val[3] | (xyz_val[4] << 8);
    *z = xyz_val[5] | (xyz_val[6] << 8);

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
