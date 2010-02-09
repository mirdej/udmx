# Name: uDMX Makefile
# www.anyma.ch

DEVICE     = atmega8
CLOCK      = 12000000
PROGRAMMER = -c usbasp -P usb
AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)

# Choose your favorite programmer and interface above.

COMPILE = avr-gcc -Wall -Os -Iusbdrv -I. -mmcu=$(DEVICE) #-DDEBUG_LEVEL=2
# NEVER compile the final product with debugging! Any debug output will
# distort timing so that the specs can't be met.

OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o


# symbolic targets:
all:	main.hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	#$(AVRDUDE) -U flash:w:main.hex:i #with erase before flashing
	$(AVRDUDE) -D -U flash:w:main.hex:i #without erase

clean:
	rm -f main.hex main.lst main.obj main.cof main.list main.map main.eep.hex main.bin *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s

# file targets:
main.bin:	$(OBJECTS)
	$(COMPILE) -o main.bin $(OBJECTS)

main.hex:	main.bin
	rm -f main.hex main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.bin main.hex
	avr-size main.bin
disasm:	main.bin
	avr-objdump -d main.bin

cpp:
	$(COMPILE) -E main.c


fuse:
	$(AVRDUDE) -U hfuse:w:0xc8:m -U lfuse:w:0xef:m
