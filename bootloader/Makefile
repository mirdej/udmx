# Name: Makefile
# Project: USBaspLoader
# Author: Christian Starkjohann
# Creation Date: 2007-12-10
# Tabsize: 4
# Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
# License: GNU GPL v2 (see License.txt)
# This Revision: $Id: Makefile 487 2008-02-05 18:23:57Z cs $

###############################################################################
# Configure the following variables according to your AVR.
# Program the device with
#     make fuse    # to set the clock generator, boot section size etc.
#     make flash   # to load the boot loader into flash
#     make lock    # to protect the boot loader from overwriting

F_CPU = 12000000
DEVICE = atmega88
BOOTLOADER_ADDRESS = 1800

PROGRAMMER = -c usbasp -P usb
#PROGRAMMER = -c stk500 -P /dev/cu.U*
#PROGRAMMER = -c stk500v2 -P avrdoper
# PROGRAMMER contains AVRDUDE options to address your programmer

#---------------------------------------------------------------------
# ATMega8
#---------------------------------------------------------------------
# Fuse high byte:
# 0xc0 = 1 1 0 0   0 0 0 0 <-- BOOTRST (boot reset vector at 0x1800)
#        ^ ^ ^ ^   ^ ^ ^------ BOOTSZ0
#        | | | |   | +-------- BOOTSZ1
#        | | | |   + --------- EESAVE (preserve EEPROM over chip erase)
#        | | | +-------------- CKOPT (full output swing)
#        | | +---------------- SPIEN (allow serial programming)
#        | +------------------ WDTON (WDT not always on)
#        +-------------------- RSTDISBL (reset pin is enabled)
# Fuse low byte:
# 0x9f = 1 0 0 1   1 1 1 1
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (external >8M crystal)
#        | |  +--------------- SUT 1..0 (crystal osc, BOD enabled)
#        | +------------------ BODEN (BrownOut Detector enabled)
#        +-------------------- BODLEVEL (2.7V)
#---------------------------------------------------------------------
# ATMega88, ATMega168
#---------------------------------------------------------------------
# Fuse extended byte:
# 0x00 = 0 0 0 0   0 0 0 0 <-- BOOTRST (boot reset vector at 0x1800)
#                    \+/
#                     +------- BOOTSZ (00 = 2k bytes)
# Fuse high byte:
# 0xd6 = 1 1 0 1   0 1 1 0
#        ^ ^ ^ ^   ^ \-+-/
#        | | | |   |   +------ BODLEVEL 0..2 (110 = 1.8 V)
#        | | | |   + --------- EESAVE (preserve EEPROM over chip erase)
#        | | | +-------------- WDTON (if 0: watchdog always on)
#        | | +---------------- SPIEN (allow serial programming)
#        | +------------------ DWEN (debug wire enable)
#        +-------------------- RSTDISBL (reset pin is enabled)
# Fuse low byte:
# 0xdf = 1 1 0 1   1 1 1 1
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (external >8M crystal)
#        | |  +--------------- SUT 1..0 (crystal osc, BOD enabled)
#        | +------------------ CKOUT (if 0: Clock output enabled)
#        +-------------------- CKDIV8 (if 0: divide by 8)


###############################################################################

# Tools:
AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)
CC = avr-gcc

# Options:
DEFINES = #-DDEBUG_LEVEL=2
#CFLAGS = -Wall -Os -I. -mmcu=$(DEVICE) -DF_CPU=$(F_CPU) $(DEFINES)
#LDFLAGS = -Wl,--section-start=.text=$(BOOTLOADER_ADDRESS)
CFLAGS = -Wall -Os -fno-move-loop-invariants -fno-tree-scev-cprop -fno-inline-small-functions -I. -mmcu=$(DEVICE) -DF_CPU=$(F_CPU) $(DEFINES)
LDFLAGS = -Wl,--relax,--gc-sections -Wl,--section-start=.text=$(BOOTLOADER_ADDRESS)

OBJECTS =  usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o

# symbolic targets:
all: main.hex

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.S.o:
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(CC) $(CFLAGS) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

readflash:
	$(AVRDUDE) -U flash:r:read.hex:i

fuse:
fuse:
	$(AVRDUDE) -U hfuse:w:0xc8:m -U lfuse:w:0xef:m

lock:
	$(AVRDUDE) $(LOCKOPT)

read_fuses:
	$(UISP) --rd_fuses

clean:
	rm -f main.hex main.bin *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s

# file targets:
main.bin:	$(OBJECTS)
	$(CC) $(CFLAGS) -o main.bin $(OBJECTS) $(LDFLAGS)

main.hex:	main.bin
	rm -f main.hex main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.bin main.hex
	avr-size main.hex

disasm:	main.bin
	avr-objdump -d main.bin

cpp:
	$(CC) $(CFLAGS) -E main.c

# Special rules for generating hex files for various devices and clock speeds
ALLHEXFILES = hexfiles/mega8_12mhz.hex hexfiles/mega8_15mhz.hex hexfiles/mega8_16mhz.hex \
	hexfiles/mega88_12mhz.hex hexfiles/mega88_15mhz.hex hexfiles/mega88_16mhz.hex \
	hexfiles/mega168_12mhz.hex hexfiles/mega168_15mhz.hex hexfiles/mega168_16mhz.hex

allhexfiles: $(ALLHEXFILES)
	$(MAKE) clean
	avr-size hexfiles/*.hex

$(ALLHEXFILES):
	@[ -d hexfiles ] || mkdir hexfiles
	@device=`echo $@ | sed -e 's|.*/mega||g' -e 's|_.*||g'`; \
	clock=`echo $@ | sed -e 's|.*_||g' -e 's|mhz.*||g'`; \
	addr=`echo $$device | sed -e 's/\([0-9]\)8/\1/g' | awk '{printf("%x", ($$1 - 2) * 1024)}'`; \
	echo "### Make with F_CPU=$${clock}000000 DEVICE=atmega$$device BOOTLOADER_ADDRESS=$$addr"; \
	$(MAKE) clean; \
	$(MAKE) main.hex F_CPU=$${clock}000000 DEVICE=atmega$$device BOOTLOADER_ADDRESS=$$addr DEFINES=-DUSE_AUTOCONFIG=1
	mv main.hex $@
