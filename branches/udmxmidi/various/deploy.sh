cd ../bootloader
read -p "FUSE: make sure gnusbAsp is in low speed mode"
make fuse
read -p "FLASHING BOOTLOADER: put gnusbAsp in high speed mode"
# change to hi speed
make flash
cd ../firmware
#change serial number
bbedit main.c
read -p "FLASHING FIRMWARE: please update serial number first"
make flash