# name of your application
APPLICATION = watchy

# If no BOARD is found in the environment, use this default:
BOARD ?= banglejs-2

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT-Digitaluhr

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += periph_gpio_irq
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += xtimer

# Use a peripheral timer for the delay, if available
FEATURES_OPTIONAL += periph_timer
FEATURES_OPTIONAL += periph_adc
FEATURES_OPTIONAL += periph_pwm
FEATURES_OPTIONAL += periph_spi

CFLAGS += -DRTT_CONSOLE

include $(RIOTBASE)/Makefile.include
