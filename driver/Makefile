
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD        := $(shell pwd)
obj-m      := pulse_counter_driver.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	@echo "[DRIVER] Built: pulse_counter_driver.ko"

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

load:
	sudo insmod pulse_counter_driver.ko
	ls -l /dev/pulse_counter
	sudo dmesg | tail -4

unload:
	sudo rmmod pulse_counter_driver
	sudo dmesg | tail -2

test: load
	@echo "--- Injecting 1000 pulses ---"
	echo "1000" | sudo tee /dev/pulse_counter
	@echo "--- Reading back ---"
	sudo cat /dev/pulse_counter
	@echo "--- Resetting ---"
	echo "0" | sudo tee /dev/pulse_counter

status:
	sudo dmesg | grep pulse_counter | tail -10

.PHONY: all clean load unload test status
