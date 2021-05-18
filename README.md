# UART2
- Simple slave character device on uart2 for raspberry pi 4B
- The device is defined in the device tree. For some odd reason it doesn't work
  when defined in device tree overlay
- The device driver is built upon serdev bus and currently supports just writing to
  the device. 

- UART2 pins (raspberry pi 4b):
* tx  - GPIO 0
* rx  - GPIO 1
* cts - GPIO 2
* rts - GPIO 3
* gnd - pin 6, 9, 14, 20, 25, 30, 34, 39

To test this device and module, simply hook up logic analyzer (or arduino) to GPIO
pin 0 and GND and run:
```bash
$ sudo sh -c 'echo test > /dev/serdev'
```
Recommended readings:
* <linux_source_root>/Documentation/devicetree/usage-model.rst
* <linux_source_root>/arch/arm/boot/dts/bcm2711.dtsi
* <linux_source_root>/arch/arm/boot/dts/bcm2711-rpi-4-b.dts
* https://elinux.org/Device_Tree_Reference
* https://www.raspberrypi.org/documentation/configuration/device-tree.md

For more on serdev bus, check out these files:
* <linux_source_root>/drivers/tty/serdev/core.c
* <linux_source_root>/include/linux/serdev.h
* <linux_source_root>/drivers/nfc/s3fwrn5/uart.c
* <linux_source_root>/drivers/bluetooth/btmtkuart.c
* <linux_source_root>/drivers/bluetooth/hci_nokia.c
* <linux_source_root>/drivers/nfc/pn533/uart.c

# Building on raspberri pi (raspbian buster) 

Install dependencies:
```bash
$ sudo apt-get install raspberrypi-kernel-headers build-essential bc git wget bison flex libssl-dev make libncurses-dev
$ sudo apt install device-tree-compiler
```
Obtain raspberri pi kernel:
```bash
$ git clone https://github.com/raspberrypi/linux.git
```

Patch and build device tree
```bash
$ cd linux			# enter raspberry pi linux kernel source tree
$ git branch uart
$ git checkout uart
$ git am uart2_device.diff	# apply patch ./uart2_device.diff
$ make bcm2711_defconfig
$ make dtbs			# build device tree
```
Install newly built device tree:
```bash
$ sudo cp arch/arm/boot/dts/bcm2711-rpi-4-b.dtb /boot/bcm2711-uart2-slave-rpi-4-b.dtb
```

Add this line to /boot/config.txt 
```
device_tree = bcm2711-uart2-slave-rpi-4-b.dtb
```
# TODO
* reading from file
* set baud at runtime 
* hook it up to some real device 
