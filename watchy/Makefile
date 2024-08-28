# name of your application
APPLICATION = watchy

# If no BOARD is found in the environment, use this default:
BOARD ?= banglejs-2

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT-Digitaluhr
#RIOTBASE ?= /home/nica/Code/PineTime-apps/RIOT

EXTERNAL_MODULE_DIRS += watchy_modules

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
#DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# to enable SEGGER RTT console on SWD
#CFLAGS += -DRTT_CONSOLE
#USEMODULE += stdio_rtt

# to enable console via BLE serial
#USEMODULE += stdio_nimble

USEMODULE += stdio_nus

CFLAGS += -I$(CURDIR)/include/

USEMODULE += tsrb
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps
USEMODULE += schedstatistics
USEMODULE += ztimer_sec
USEMODULE += ztimer_msec
USEMODULE += timex
USEMODULE += rtc_utils
FEATURES_REQUIRED += periph_rtt
#FEATURES_REQUIRED += periph_rtt_set_counter
USEMODULE += ztimer_no_periph_rtt
#USEMODULE += rtt_rtc
CFLAGS += -DRTC_NORMALIZE_COMPAT=1
USEMODULE += lpm013m126
USEMODULE += disp_dev

USEMODULE += shell_lock
USEMODULE += shell_lock_auto_locking
CFLAGS += -DCONFIG_SHELL_LOCK_PASSWORD=\"123456\"

USEPKG += lvgl
USEMODULE += lvgl_core
USEMODULE += lvgl_contrib
USEMODULE += lvgl_font
USEMODULE += lvgl_widget_label
USEMODULE += lvgl_widget_btn
USEMODULE += lvgl_widget_btnmatrix
USEMODULE += lvgl_widget_bar
USEMODULE += lvgl_widget_line
USEMODULE += lvgl_widget_textarea
USEMODULE += lvgl_extra_layout_flex
USEMODULE += lvgl_extra_widget_msgbox
USEMODULE += lvgl_extra_widget_meter
#USEMODULE += lvgl_extra_widget_spinbox
USEMODULE += lvgl_widget_checkbox

USEMODULE += lvgl_misc
USEMODULE += lvgl_draw
USEMODULE += lvgl_hal
USEMODULE += core_thread_flags

#USEMODULE += lvgl_extra_widget_win
#USEMODULE += lvgl_extra_layout_flex
USEMODULE += lvgl_extra_theme_default
USEMODULE += lvgl_extra_theme_default_dark

CFLAGS += -DCONFIG_LVGL_ACTIVITY_PERIOD=1000

USEMODULE += printf_float

USEPKG += minmea
ifneq ($(BOARD),native)
USEPKG += nimble
USEMODULE += nimble_svc_gap
USEMODULE += nimble_svc_gatt
USEMODULE += nimble_autoadv
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_DEVICE_NAME='"Watchy on RIOT"'
CFLAGS += -DCONFIG_NIMBLE_AUTOADV_START_MANUALLY=1

#USEMODULE += nimble_host_store_ram
#USEMODULE += nimble_host
#USEMODULE += nimble_controller
#USEMODULE += bluetil_ad
endif

USEMODULE += core_thread_flags
USEMODULE += luid
USEMODULE += fmt
USEMODULE += event_timeout

#USEMODULE += mtd
#USEMODULE += mtd_mapper
#USEMODULE += vfs
USEMODULE += vfs_default
#USEMODULE += shell_cmd_vfs
#USEPKG += littlefs2
#USEMODULE += constfs
#USEMODULE += devfs

# Enable to automatically format if mount fails
USEMODULE += vfs_auto_format

USEMODULE += bmx280

# Use a peripheral timer for the delay, if available
ifneq ($(BOARD),native)
FEATURES_REQUIRED += periph_i2c
//FEATURES_REQUIRED += periph_timer
FEATURES_REQUIRED += periph_adc
endif
FEATURES_REQUIRED += periph_pwm
FEATURES_REQUIRED += periph_spi
FEATURES_REQUIRED += periph_gpio_irq
FEATURES_REQUIRED += periph_uart

#CFLAGS += -DISR_STACKSIZE=512
CFLAGS += -DTHREAD_STACKSIZE_MAIN=THREAD_STACKSIZE_LARGE+THREAD_EXTRA_STACKSIZE_PRINTF

CFLAGS += -DLV_HOR_RES_MAX=176 -DLV_VER_RES_MAX=176 -DLV_CONF_INCLUDE_SIMPLE -DLV_CONF_PATH="$(CURDIR)/lv_conf_watchy.h"

include $(RIOTBASE)/Makefile.include
