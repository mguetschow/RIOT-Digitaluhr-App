# name of your application
APPLICATION = watchy

# If no BOARD is found in the environment, use this default:
BOARD ?= banglejs-2

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT-Digitaluhr
#RIOTBASE ?= /home/nica/Code/PineTime-apps/RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
#DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += ztimer_sec
USEMODULE += ztimer_msec
USEMODULE += i2c_scan

USEMODULE += lpm013m126
USEMODULE += disp_dev

USEPKG += lvgl
USEMODULE += lvgl_core
USEMODULE += lvgl_contrib
USEMODULE += lvgl_font
USEMODULE += lvgl_widget_label
USEMODULE += lvgl_widget_btn
USEMODULE += lvgl_widget_btnmatrix
USEMODULE += lvgl_extra_layout_flex
USEMODULE += lvgl_extra_widget_msgbox

USEMODULE += lvgl_misc
USEMODULE += lvgl_draw
USEMODULE += lvgl_hal
USEMODULE += core_thread_flags

#USEMODULE += lvgl_extra_widget_win
#USEMODULE += lvgl_extra_layout_flex
USEMODULE += lvgl_extra_theme_default
USEMODULE += lvgl_extra_theme_default_dark

USEMODULE += printf_float

USEPKG += minmea

USEPKG += nimble
USEMODULE += nimble_svc_gap
USEMODULE += nimble_svc_gatt
USEMODULE += nimble_autoadv
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_DEVICE_NAME='"Watchy on RIOT"'
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_START_MANUALLY=1

#USEMODULE += nimble_host_store_ram
#USEMODULE += nimble_host
#USEMODULE += nimble_controller
#USEMODULE += nimble_svc_gap
#USEMODULE += nimble_svc_gatt
#USEMODULE += bluetil_ad

USEMODULE += core_thread_flags
USEMODULE += luid
USEMODULE += fmt
USEMODULE += event_timeout

USEMODULE += mtd
USEMODULE += mtd_mapper
USEMODULE += vfs
USEPKG += littlefs
USEMODULE += littlefs_fs

USEMODULE += bmx280

# Use a peripheral timer for the delay, if available
FEATURES_REQUIRED += periph_i2c
//FEATURES_REQUIRED += periph_timer
FEATURES_REQUIRED += periph_adc
FEATURES_REQUIRED += periph_pwm
FEATURES_REQUIRED += periph_spi
FEATURES_REQUIRED += periph_gpio_irq
FEATURES_REQUIRED += periph_uart


CFLAGS += -DRTT_CONSOLE
#CFLAGS += -DISR_STACKSIZE=512
CFLAGS += -DTHREAD_STACKSIZE_MAIN=THREAD_STACKSIZE_LARGE+THREAD_EXTRA_STACKSIZE_PRINTF

CFLAGS += -DLV_COLOR_DEPTH=8 -DLV_HOR_RES_MAX=176 -DLV_VER_RES_MAX=176 -DLV_CONF_INCLUDE_SIMPLE -DLV_CONF_PATH="$(CURDIR)/lv_conf_watchy.h"

include $(RIOTBASE)/Makefile.include
