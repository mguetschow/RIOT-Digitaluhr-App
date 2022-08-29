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

#include "vc31.h"

// VC31
#define VC31A_DEV_ID        0x00 // read only
#define VC31A_STATUS        0x01 // read only
#define VC31A_PPG           0x02 // read only
#define VC31A_CURRENT       0x04 // read only
#define VC31A_PRE           0x06 // read only
#define VC31A_PS            0x08 // read only
#define VC31A_ENV           0x0A // read only
#define VC31A_CTRL          0x20
#define VC31A_PPG_DIV       0x21
#define VC31A_PS_DIV        0x23
#define VC31A_TIA_WAIT      0x24
#define VC31A_AMP_WAIT      0x25
#define VC31A_GREEN_WAIT    0x26
#define VC31A_GREEN_IR_GAP  0x27
#define VC31A_IR_WAIT       0x28
#define VC31A_GREEN_ADJ     0x29
// VC31B
#define VC31B_REG1         0x01 // read only Sensor Status
// see VC31B_STATUS_*
#define VC31B_REG2         0x02 // read only IRQ status
// see VC31B_INT_*
#define VC31B_REG3         0x03 // FIFO write pos (where it'll write NEXT)
#define VC31B_REG7         0x07 // 2x 16 bit counter from internal oscillator (if 2nd is less than 1st there's some kind of rollover)
#define VC31B_REG10        0x3B // write 0x5A for a soft reset
#define VC31B_REG11        0x10 // CTRL
 // 0x80 = enable
 // 0x40 = ??? set for HRM measurement
 // 0x10 = if SPO2 w edisable this when not worn
 // 0x08 = LED control? disable when not worn
 // 0x04 = SLOT2 enable for env/wear detect sensor
 // 0x02 = SLOT1 enable for SPO2
 // 0x01 = SLOT0 enable for HRM/PPG
 // totalSlots = amount of bits set in 0x02/0x01
#define VC31B_REG12        0x11 // INT?
// 0x80 appears to enable interrupts?
// 0x10 = WearStatusDetection (ENV sensor IRQ?)
#define VC31B_REG13        0x12 // ???
#define VC31B_REG14        0x13 // FIFO 0x40=some flag, + assert FIFO IRQ every X samples
#define VC31B_REG15        0x14 // 16 bit time calibration (x>>8,x) (0x31F default)
#define VC31B_REG16        0x16 // ENV samplerate. samplesPerSec-6. After how many samples is the 0x10 IRQ asserted for ENV data
#define VC31B_REG17        0x17 // SLOT0 LED Current - 0xEF = maxLedCur
#define VC31B_REG18        0x18 // SLOT1 LED Current - 0xEF = maxLedCur
#define VC31B_REG19        0x19 // LED current, 0x30=10mA,0x50=40mA,0x5A=60mA,0xE0=80mA
#define VC31B_REG20        0x1A // SLOT0 ENV sensitivity - 0x77 = PDResMax
#define VC31B_REG21        0x1B // SLOT1 ENV sensitivity - 0x77 = PDResMax
#define VC31B_REG22        0x1C // ? set to 0x67 for HRM mode

// Interrupts
// 0x10 = WearStatusDetection (ENV sensor IRQ?)
/* Bit fields for VC31B_REG1 */
#define VC31B_STATUS_CONFLICT                       0x10
#define VC31B_STATUS_INSAMPLE                       0x08
#define VC31B_STATUS_OVERLOAD_MASK                  0x07 // 3x bits for each of the 3 channels
/* Bit fields for VC31B_REG2 */
#define VC31B_INT_PS                          0x10 // used for wear detection
#define VC31B_INT_OV                          0x08 // OvloadAdjust
#define VC31B_INT_FIFO                        0x04
#define VC31B_INT_ENV                         0x02 // EnvAdjust
#define VC31B_INT_PPG                         0x01 // PpgAdjust

#define VC31B_PS_TH                     6 // threshold for wearing/not
#define VC31B_PPG_TH                    10 // Causes of PPG interruption PPG_TH = 300
#define VC31B_ADJUST_INCREASE                   22 // 1.4 << 4 = 22.4//1.4f
#define VC31B_ADJUST_DECREASE                     11 // 0.7 << 4 = 11.2//0.7f
#define VC31B_ADJUST_STEP_MAX                   32
#define VC31B_ADJUST_STEP_MIN                   1


#define VC31A_CTRL_RESET_VALUE       0x03 // Default value for VC31A_CTRL
#define VC31A_CTRL_MASK              0xF7 // Mask for VC31A_CTRL
#define VC31A_CTRL_WORK_MODE         0x80 // Enter work mode. Start sample
#define VC31A_CTRL_ENABLE_PPG        0x40 // Enable green led sample
#define VC31A_CTRL_ENABLE_PRE        0x20 // Enable pre sample
#define VC31A_CTRL_LDO_EXTERN        0x10 // External LDO
#define VC31A_CTRL_INT_DIR_RISING    0x03 // IRQ trigger on raising edge
#define VC31A_CTRL_OPA_GAIN_12_5     0x00 // OPA3 Gain 12.5.
#define VC31A_CTRL_OPA_GAIN_25       0x01 // OPA3 Gain 25.
#define VC31A_CTRL_OPA_GAIN_50       0x02 // OPA3 Gain 50.
#define VC31A_CTRL_OPA_GAIN_100      0x03 // OPA3 Gain 100.

#define VC31A_PPG_DIV_10_HZ          0x0CC5
#define VC31A_PPG_DIV_12_5_HZ        0x0A35
#define VC31A_PPG_DIV_25_HZ          0x0516
#define VC31A_PPG_DIV_50_HZ          0x0287
#define VC31A_PPG_DIV_100_HZ         0x013F
#define VC31A_PPG_DIV_1000_HZ        0x0018

#define VC31A_STATUS_D_ENV_OK        0x10
#define VC31A_STATUS_D_PS_OK         0x08
#define VC31A_STATUS_D_PRE_OK        0x04
#define VC31A_STATUS_D_CUR_OK        0x02
#define VC31A_STATUS_D_PPG_OK        0x01

#define VC31A_GREEN_ADJ_RESET_VALUE  0x0000          // Default value for VC31A_GREEN_ADJ.
#define VC31A_GREEN_ADJ_MASK         0xFFFF          // Mask for VC31A_GREEN_ADJ.
#define VC31A_GREEN_ADJ_ENABLE       0x8000          // Enable current adjust.
#define VC31A_GREEN_ADJ_DISABLE      0               // Disable current adjust.
#define VC31A_GREEN_ADJ_UP           0x4000          // Turn up the current.
#define VC31A_GREEN_ADJ_DOWN         0               // Turn down the current.
#define VC31A_GREEN_ADJ_VALUE_MASK   0x3FFF          // Mask for VC31A_ADJ_CUR value.


#define VC31A_ADJUST_FACTOR_INCREASE   22       // 1.4 << 4 = 22.4
#define VC31A_ADJUST_FACTOR_DECREASE   11       // 0.7 << 4 = 11.2
#define VC31A_ADJUST_FACTOR_MAX        1536000
#define VC31A_ADJUST_FACTOR_MIN        15360
#define VC31A_ADJUST_STEP_MAX          1000
#define VC31A_ADJUST_STEP_MIN          2

#define VC31A_ENV_LIMIT                2500
#define VC31A_PS_LIMIT                 350//150

#define VC31A_PPG_LIMIT_L              200
#define VC31A_PPG_LIMIT_H              3900
#define VC31A_CURRENT_LIMIT_L          12
#define VC31A_CURRENT_LIMIT_H          1000
#define VC31A_UNWEAR_CNT               3
#define VC31A_ISWEAR_CNT               1

typedef enum
{
    AdjustDirection_Null    = 0,
    AdjustDirection_Up      = 1,
    AdjustDirection_Down    = 2,
} VC31AdjustDirection;

typedef struct
{
    VC31AdjustDirection directionLast;
    VC31AdjustDirection direction;// was directionLastBefore for VC31A
    int32_t step;
} VC31AdjustInfo_t;

typedef enum {
  VC31_DEVICE,
  VC31B_DEVICE
} VC31Type;

// Hack to fix Eclipse syntax lint
#ifndef PACKED_FLAGS
#define PACKED_FLAGS
#endif

// VC31 info shared between VC31A/B
typedef struct {
  bool isWearing;
  int8_t isWearCnt, unWearCnt; // counters for switching worn/unworn state
  uint16_t ppgValue; // current PPG value
  uint16_t ppgLastValue; // last PPG value
  int16_t ppgOffset; // PPG 'offset' value. When PPG adjusts we change the offset so it matches the last value, then slowly adjust 'ppgOffset' back down to 0
  uint8_t wasAdjusted; // true if LED/etc adjusted since the last reading
  // the meaning of these is device-dependent but it's nice to have them in one place
  uint8_t irqStatus;
  uint8_t raw[12];
} PACKED_FLAGS VC31Info;

// VC31A-specific info
typedef struct {
  uint8_t ctrl; // current VC31A_CTRL reg value
  uint16_t currentValue;
  uint16_t preValue;
  uint16_t envValue;
  uint16_t psValue;
  VC31AdjustInfo_t adjustInfo;

} PACKED_FLAGS VC31AInfo;
// VC31B-specific info
typedef struct {
  uint8_t maxLedCur;
  uint8_t pdResValue[3];
  uint8_t currentValue[3];
  uint8_t psValue;      //PS Sample value.
  uint8_t preValue[2];  //Environment Sample value.
  uint8_t envValue[3];  //Environment Sample value.
} PACKED_FLAGS VC31BSample;
typedef struct {
  uint8_t vcHr02SampleRate; // Heart rate sample frequency
  uint8_t status; // REG2
  uint8_t fifoReadIndex; // last index we read from
  VC31BSample sampleData;
  VC31AdjustInfo_t adjustInfo[2];

  uint8_t ledCurrent[3];
  uint8_t ledMaxCurrent[3];
  uint8_t pdRes[3],pdResMax[3];
  uint8_t pdResSet[3],ppgGain[3];
  bool slot0EnvIsExceedFlag,slot1EnvIsExceedFlag;

  uint8_t regConfig[17]; // all config registers (written to VC31B_REG11)
  bool psBiasReadInPdFlag;

  // SETUP (computed from regConfig)
  uint8_t totalSlots;  // 2 is SPO2 enabled, 1 otherwise or 0 if all disabled
  uint8_t fifoIntDiv;  // when should IRQ enable (i guess?)
  bool enFifo; // FIFO enabled (otherwise data is at 0x80)

} PACKED_FLAGS VC31BInfo;

// Actual vars
VC31Type vcType;
VC31Info vcInfo;
VC31AInfo vcaInfo;
VC31BInfo vcbInfo;


static void _vc31_int(void *arg)
{
    (void) arg;
    watchy_event_queue_add(EV_HRM);
}

// turns off the engine
int vc31_deactivate(void)
{
    //uint8_t val=0;

    i2c_acquire(HRM_I2C_DEV);

    // i2c_write_regs(HRM_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_CNTL1, &val, 1, 0);

    i2c_release(HRM_I2C_DEV);

    gpio_clear(HRM_PWR);

    return 0;
}

// turns on the engine
int vc31_activate(void)
{
    // uint8_t val=0;

    gpio_set(HRM_PWR);
    ztimer_sleep(ZTIMER_USEC, 1000);

    i2c_acquire(HRM_I2C_DEV);

    //val=KX0231025_LOWPOWER_MODE | KX0231025_RANGE_2G | KX0231025_TPE | KX0231025_PC1;
    //i2c_write_regs(HRM_I2C_DEV, ACCEL_I2C_ADDR, KX0231025_CNTL1, &val, 1, 0);

    i2c_release(HRM_I2C_DEV);

    return 0;
}

//
// init will only check for device presence and set some basic
// parameters but keep the device in standby / low power state
//
int vc31_init(void)
{
    //int res;
    uint8_t val=0;

    gpio_set(HRM_PWR);
    ztimer_sleep(ZTIMER_USEC, 1000);

    i2c_acquire(HRM_I2C_DEV);
    if (i2c_read_regs(HRM_I2C_DEV, HRM_I2C_ADDR, 0x00, &val, 1, 0) < 0) {
        DEBUG("VC31 i2c_read_regs() failed\n");
        goto fail;
    } else {
        // expect 17 / 0x11 (VC31) or 33 / 0x21 (VC31B)
        DEBUG("VC31 found 0x%02x\n", val);
    }
    i2c_release(HRM_I2C_DEV);
    gpio_init_int(HRM_INT, GPIO_IN, GPIO_RISING, _vc31_int, NULL);

    gpio_clear(HRM_PWR);

    return 0;

fail:
    i2c_release(HRM_I2C_DEV);
    gpio_clear(HRM_PWR);

    return -1;
}
