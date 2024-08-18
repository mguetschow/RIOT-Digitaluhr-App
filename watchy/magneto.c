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

static int16_t xmin=32767, xmax=-32767, ymin=32767, ymax=-32767, zmin=32767, zmax=-32767;

static int16_t magneto_course(int16_t x, int16_t y, int16_t z)
{
    int course=-1;
    (void) z;
    int dx = x - ((xmin + xmax)/2);
    int dy = y - ((ymin + ymax)/2);
    //int dz = z - ((magmin.z+magmax.z)/2);

    course = atan2(dx, dy) * 180 / M_PI;
    if (course < 0)
        course += 360;
    course = 360 - course;

    return course;
}

// gets triggered externally
bool magneto_trigger(void)
{
    uint8_t xyz_val[7];
    uint8_t val;

    i2c_acquire(MAGN_I2C_DEV);

    if (i2c_read_regs(MAGN_I2C_DEV, MAGN_I2C_ADDR, MAGNETO_STATUS, &xyz_val, 7, 0) < 0) {
        DEBUG("magneto i2c_read_regs() failed\n");
        goto fail;
    }
    if (xyz_val[0] & MAGNETO_STATUS_BUSY) {
        i2c_release(MAGN_I2C_DEV);
        return false;
    }
    i2c_read_regs(MAGN_I2C_DEV, MAGN_I2C_ADDR, MAGNETO_SINGLE, &val, 1, 0);
    i2c_release(MAGN_I2C_DEV);

#if 0
    *y = xyz_val[1] | (xyz_val[2] << 8);
    *x = xyz_val[3] | (xyz_val[4] << 8);
    *z = xyz_val[5] | (xyz_val[6] << 8);
#endif
#if 1
    watch_state.magnetometer_state.y = xyz_val[2] | (xyz_val[1] << 8);
    watch_state.magnetometer_state.x = xyz_val[4] | (xyz_val[3] << 8);
    watch_state.magnetometer_state.z = xyz_val[6] | (xyz_val[5] << 8);
    if (watch_state.magnetometer_state.calibrate) {
        if (watch_state.magnetometer_state.x > xmax) xmax = watch_state.magnetometer_state.x;
        if (watch_state.magnetometer_state.x < xmin) xmin = watch_state.magnetometer_state.x;
        if (watch_state.magnetometer_state.y > ymax) ymax = watch_state.magnetometer_state.y;
        if (watch_state.magnetometer_state.y < ymin) ymin = watch_state.magnetometer_state.y;
        if (watch_state.magnetometer_state.z > zmax) zmax = watch_state.magnetometer_state.z;
        if (watch_state.magnetometer_state.z < zmin) zmin = watch_state.magnetometer_state.z;
    }
    watch_state.magnetometer_state.course = magneto_course(watch_state.magnetometer_state.x, watch_state.magnetometer_state.y, watch_state.magnetometer_state.z);
#endif
#if 0
    *y = *(int16_t *)xyz_val+1;
    if (*y > ymax) ymax = *y;
    else if (*y < ymin) ymin = *y;
    *x = *(int16_t *)xyz_val+3;
    if (*x > xmax) xmax = *x;
    else if (*x < xmin) xmin = *x;
    *z = *(int16_t *)xyz_val+5;
    if (*z > zmax) zmax = *z;
    else if (*z < zmin) zmin = *z;
#endif
    return true;

fail:
    i2c_release(MAGN_I2C_DEV);

    return false;
}

bool magneto_set_state(bool state)
{
    // not much to do here, we can not en- or disable it,
    // nor power it on or off,
    // just mark as active to enable polling by main loop
    watch_state.magnetometer_state.active = state;

    return watch_state.magnetometer_state.active;
}

bool magneto_calibrate(bool state)
{
    if (state) {
        xmin=32767; xmax=-32767; ymin=32767; ymax=-32767; zmin=32767; zmax=-32767;
    }
    watch_state.magnetometer_state.calibrate = state;

    return state;
}

int magneto_init(void)
{
    uint8_t val=0;

    watch_state.magnetometer_state.active = false;
    watch_state.magnetometer_state.calibrate = false;

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
