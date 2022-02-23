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

#include "lpm013m126.h"

#define LCD_COLUMNS 176
#define LCD_LINES 176

#define LCD_COLOR_CMD_UPDATE            (0x90) // Update Mode (4bit Data Mode)
#define LCD_COLOR_CMD_ALL_CLEAR         (0x20) // All Clear Mode
#define LCD_COLOR_CMD_NO_UPDATE         (0x00) // No Update Mode
#define LCD_COLOR_CMD_BLINKING_WHITE    (0x18) // Display Blinking Color Mode (White)
#define LCD_COLOR_CMD_BLINKING_BLACK    (0x10) // Display Blinking Color Mode (Black)
#define LCD_COLOR_CMD_INVERSION         (0x14) // Display Inversion Mode

static bool is_init=false;
// a buffer to prep data for transfer to the LCD
static uint8_t transfer_buffer[(16 * ((LCD_COLUMNS / 2) + 2)) +2];

int lpm013m126_write(uint8_t *buf, int len)
{
    // if not initialized yet, bail out
    if (!is_init)
        return -1;
    // at least two bytes for commands
    if (len<2)
        return -1;
    // all writes must be 16bit aligned
    if (len%2)
        return -1;

//    pwm_poweron(PWM_DEV(1));

    gpio_set(LCD_CS);

    spi_acquire(SPI_DEV(0), SPI_CS_UNDEF, SPI_MODE_0, SPI_CLK_1MHZ);
    spi_transfer_bytes(SPI_DEV(0), SPI_CS_UNDEF, false, buf, NULL, len);
    spi_release(SPI_DEV(0));

    gpio_clear(LCD_CS);

//    pwm_poweroff(PWM_DEV(1));

    return len;
}


//
// write cnt number of lines to the LCD,
// starting from line start
// Note: only full lines can be written
// line buffer oly conntains pixel data, one nibble per pixel RGBx
//
int lpm013m126_write_lines(uint8_t *lines, uint8_t start, uint16_t cnt)
{
    uint8_t *bufp;

    if (cnt > 16)
        cnt=16;

#if 0
    if ((sizeof(lines)/(176/2)) < cnt) {
        puts("-buf too small");
        return -1;
    }
#endif
//    pwm_poweron(PWM_DEV(1));

    gpio_set(LCD_CS);

    spi_acquire(SPI_DEV(0), SPI_CS_UNDEF, SPI_MODE_0, SPI_CLK_1MHZ);

    bufp = transfer_buffer;
    *bufp++ = LCD_COLOR_CMD_UPDATE | (start >> 8);
    *bufp++ = (start & 0xff);

    for (int i=0; i<cnt; i++) {
        memcpy(bufp, &lines[i*(176/2)], (176/2));
        bufp += (176/2);
        start++;
        *bufp++ = (start >> 8);
        *bufp++ = (start & 0xff);
    }
    *bufp++ = 0;
    *bufp++ = 0;

    gpio_set(LCD_CS);
    spi_transfer_bytes(SPI_DEV(0), SPI_CS_UNDEF, false, transfer_buffer, NULL, (cnt*(2+(176/2))+2) );
    gpio_clear(LCD_CS);

    spi_release(SPI_DEV(0));

//    pwm_poweroff(PWM_DEV(1));

    return cnt;
}

void lpm013m126_clear(void)
{
    uint8_t buf[2];

    buf[0]=LCD_COLOR_CMD_ALL_CLEAR;
    buf[1]=0x00;
    lpm013m126_write(buf, 2);
}

void lpm013m126_invert(void)
{
    uint8_t buf[2];

    buf[0]=LCD_COLOR_CMD_INVERSION;
    buf[1]=0x00;
    lpm013m126_write(buf, 2);
}

void lpm013m126_noupdate(void)
{
    uint8_t buf[2];

    buf[0]=LCD_COLOR_CMD_NO_UPDATE;
    buf[1]=0x00;
    lpm013m126_write(buf, 2);
}


//
// init the display and what else is needed,
// returns 0 if OK, !=0 otherwise
//
int lpm013m126_init(void)
{
    uint8_t buf[2];

    pwm_poweron(PWM_DEV(1));

    gpio_clear(LCD_CS);
    gpio_set(LCD_DISP);
    spi_init(SPI_DEV(0));

    buf[0]=LCD_COLOR_CMD_ALL_CLEAR;
    buf[1]=0;
    buf[2]=0;
    buf[3]=0;
    lpm013m126_write(buf, 4);

    is_init=true;
    return 0;
}
