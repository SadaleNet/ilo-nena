all : flash

TARGET:=ilonena
CH32FUN:=$(CH32FUN_PATH)/ch32fun
TARGET_MCU:=CH32V003

ADDITIONAL_C_FILES+=$(RV003USB_PATH)/rv003usb/rv003usb.S $(RV003USB_PATH)/rv003usb/rv003usb.c button.c display.c lookup.c keyboard.c optionbytes.c tim2_task.c watchdog.c
ifdef ILONENA_LEGACY_UCSUR
	ADDITIONAL_C_FILES+=generated_legacy.c
else
	ADDITIONAL_C_FILES+=generated_2026.c
endif

EXTRA_CFLAGS:=-I$(RV003USB_PATH)/lib -I$(RV003USB_PATH)/rv003usb


include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
