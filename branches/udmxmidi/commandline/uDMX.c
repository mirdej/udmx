/* Name: powerSwitch.c
 * Project: PowerSwitch based on AVR USB driver
 * Author: Christian Starkjohann
 * Creation Date: 2005-01-16
 * Tabsize: 4
 * Copyright: (c) 2005 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: Proprietary, free under certain conditions. See Documentation.
 * This Revision: $Id: uDMX.c,v 1.1.1.1 2006/02/15 17:55:06 cvs Exp $
 */

/*
General Description:
This program controls the PowerSwitch USB device from the command line.
It must be linked with libusb, a library for accessing the USB bus from
Linux, FreeBSD, Mac OS X and other Unix operating systems. Libusb can be
obtained from http://libusb.sourceforge.net/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>    /* this is libusb, see http://libusb.sourceforge.net/ */

#define USBDEV_SHARED_VENDOR    0x16C0  /* VOTI */
#define USBDEV_SHARED_PRODUCT   0x05DC  /* Obdev's free shared PID */
/* Use obdev's generic shared VID/PID pair and follow the rules outlined
 * in firmware/usbdrv/USBID-License.txt.
 */

#include "../uDMX_cmds.h"

static void usage(char *name)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s <channel> <value> [<value> ...]\n", name);
}


static int  usbGetStringAscii(usb_dev_handle *dev, int index, int langid, char *buf, int buflen)
{
char    buffer[256];
int     rval, i;

    if((rval = usb_control_msg(dev, USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR, (USB_DT_STRING << 8) + index, langid, buffer, sizeof(buffer), 1000)) < 0)
        return rval;
    if(buffer[1] != USB_DT_STRING)
        return 0;
    if((unsigned char)buffer[0] < rval)
        rval = (unsigned char)buffer[0];
    rval /= 2;
    /* lossy conversion to ISO Latin1 */
    for(i=1;i<rval;i++){
        if(i > buflen)  /* destination buffer overflow */
            break;
        buf[i-1] = buffer[2 * i];
        if(buffer[2 * i + 1] != 0)  /* outside of ISO Latin1 range */
            buf[i-1] = '?';
    }
    buf[i-1] = 0;
    return i-1;
}


/* PowerSwitch uses the free shared default VID/PID. If you want to see an
 * example device lookup where an individually reserved PID is used, see our
 * RemoteSensor reference implementation.
 */
static usb_dev_handle   *findDevice(void)
{
struct usb_bus      *bus;
struct usb_device   *dev;
usb_dev_handle      *handle = 0;

    usb_find_busses();
    usb_find_devices();
    for(bus=usb_busses; bus; bus=bus->next){
        for(dev=bus->devices; dev; dev=dev->next){
            if(dev->descriptor.idVendor == USBDEV_SHARED_VENDOR && dev->descriptor.idProduct == USBDEV_SHARED_PRODUCT){
                char    string[256];
                int     len;
                handle = usb_open(dev); /* we need to open the device in order to query strings */
                if(!handle){
                    fprintf(stderr, "Warning: cannot open USB device: %s\n", usb_strerror());
                    continue;
                }
                /* now find out whether the device actually is obdev's Remote Sensor: */
                len = usbGetStringAscii(handle, dev->descriptor.iManufacturer, 0x0409, string, sizeof(string));
                if(len < 0){
                    fprintf(stderr, "warning: cannot query manufacturer for device: %s\n", usb_strerror());
                    goto skipDevice;
                }
                /* fprintf(stderr, "seen device from vendor ->%s<-\n", string); */
                if(strcmp(string, "www.anyma.ch") != 0)
                    goto skipDevice;
                len = usbGetStringAscii(handle, dev->descriptor.iProduct, 0x0409, string, sizeof(string));
                if(len < 0){
                    fprintf(stderr, "warning: cannot query product for device: %s\n", usb_strerror());
                    goto skipDevice;
                }
                /* fprintf(stderr, "seen product ->%s<-\n", string); */
                if(strcmp(string, "uDMX") == 0)
                    break;
skipDevice:
                usb_close(handle);
                handle = NULL;
            }
        }
        if(handle)
            break;
    }
    if(!handle)
        fprintf(stderr, "Could not find USB device www.anyma.ch/uDMX\n");
    return handle;
}

int main(int argc, char **argv)
{
usb_dev_handle      *handle = NULL;
unsigned char       buffer[8];
int                 nBytes;
	usb_set_debug(1);
   
    usb_init();
    if((handle = findDevice()) == NULL){
        fprintf(stderr, "Could not find USB device \"uDMX\" with vid=0x%x pid=0x%x\n", USBDEV_SHARED_VENDOR, USBDEV_SHARED_PRODUCT);
        exit(1);
    }
	if(argc < 3){
		if (strcmp(argv[1], "-bootloader") == 0) {
			nBytes = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
									cmd_StartBootloader, 0, 0, buffer, sizeof(buffer), 5000);
			printf("Starting bootloader...\nPlease use the ./uboot utility to update firmware.");						

		} else {
			usage(argv[0]);
	        exit(1);
	    }
    }
	if(argc == 3) {
        int channel = 0, value = 0;
        channel = atoi(argv[1]);
		value = atoi(argv[2]);
		nBytes = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
									cmd_SetSingleChannel, value, channel, buffer, sizeof(buffer), 5000);
        fprintf(stderr, "bytes returned: %i\n", nBytes);
		if(nBytes < 0)
            fprintf(stderr, "USB error: %s\n", usb_strerror());
		else if(nBytes > 0) printf("returned: %i\n", (int)(buffer[0]));
    }
	else {
		int channel = atoi(argv[1]), i;
		unsigned char* buf = malloc(argc - 2);
		printf("argc: %i\n", argc);
		for(i=2; i<argc; ++i) buf[i-2] = atoi(argv[i]);
		nBytes = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
									cmd_SetChannelRange, argc-2, channel, buf, argc-2, 5000);
        fprintf(stderr, "bytes returned: %i\n", nBytes);
		if(nBytes < 0)
            fprintf(stderr, "USB error: %s\n", usb_strerror());
		else if(nBytes > 0) printf("returned: %i\n", (int)(buf[0]));
		free(buf);
	}
    usb_close(handle);
    return 0;
}

