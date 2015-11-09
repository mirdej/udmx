/*
	udmx.c
	
	Max-Interface to the [ a n y m a | usb-dmx-interface ]
	
	Authors:	Max & Michael Egger
	Copyright:	2006-2015 [ a n y m a ]
	Website:	www.anyma.ch
	
	License:	GNU GPL 2.0 www.gnu.org
	
	Version:	2015-11-09
 */


#include "ext.h"  		// you must include this - it contains the external object's link to available Max functions
#include "ext_common.h"


#include "../common/udmx_cmds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include </usr/local/include/usb.h>    /* this is libusb, see http://libusb.sourceforge.net/ */

#define USBDEV_SHARED_VENDOR    	0x16C0  /* VOTI */
#define USBDEV_SHARED_PRODUCT   	0x05DC  /* Obdev's free shared PID for Vendor-Type devices*/
#define USBDEV_SHARED_PRODUCT_HID   0x05DF  /* Obdev's free shared PID for HID devices*/
#define USBDEV_SHARED_PRODUCT_MIDI   0x05E4  /* Obdev's free shared PID for MIDI devices*/

#define SPEED_LIMIT				10 		//  default transmission speed limit in ms

typedef struct _udmx				// defines our object's internal variables for each instance in a patch
{
    t_object 		p_ob;			// object header - ALL objects MUST begin with this...
    t_uint16 		channel;		// int value - received from the right inlet and stored internally for each object instance
    usb_dev_handle	*dev_handle;	// handle to the udmx converter
    t_uint8			debug_flag;
    void			*m_clock;		// handle to our clock
    void *m_qelem;
    t_uint8			clock_running;	// is our clock running?
    t_uint16		speedlim;
    void 			*statusOutlet;		// our status outlet
    void			*msgOutlet;		//
    t_uint8			dmx_buffer[512];
    t_int8			usb_devices_seen;
    t_uint16		channel_changed_min,channel_changed_max;
    char			serial_number[32];
    char			bind_to[32];
    t_uint8         correct_adressing;
} t_udmx;

static t_class *udmx_class; // global pointer to the object class - so max can reference the object

// these are prototypes for the methods that are defined below
void udmx_int(t_udmx *x, t_int16 n);
void udmx_float(t_udmx *x, double f);
void udmx_speedlim(t_udmx *x, t_uint16 n);
void udmx_in1(t_udmx *x, t_int16 n);
void udmx_list(t_udmx *x, t_symbol *s, short ac, t_atom *av);
void udmx_open(t_udmx *x);
void udmx_close(t_udmx *x);
void udmx_bind(t_udmx *x, t_symbol *s);
void udmx_tick(t_udmx *x);
void udmx_getSerial(t_udmx *x);
void udmx_blackout(t_udmx *x);
void udmx_assist(t_udmx *x, void *b, t_uint16 m, t_uint16 a, char *s);
void *udmx_new(t_symbol *s, long argc, t_atom *argv);
void udmx_free(t_udmx *x);
static int  usbGetStringAscii(usb_dev_handle *dev, t_uint16 index, t_uint16 langid, char *buf, t_uint16 buflen);
void find_device(t_udmx *x);
void udmx_send_range(t_udmx *x, t_uint16 from, t_uint16 to);
void udmx_send_single(t_udmx *x, t_uint16 chann);

void udmx_message(t_udmx *x,t_symbol *message) {
    //outlet_anything(x->msgOutlet,gensym("set"),1,&out);
    outlet_anything(x->msgOutlet, message, 0, NULL);
}
//----------------------------------------------------------------------------------------------------------------
// Int in right inlet changes the channel address
//----------------------------------------------------------------------------------------------------------------
void udmx_in1(t_udmx *x, t_int16 n){

    if(x->correct_adressing) n=n-1;
    if (n > 511) n=511;
    if (n < 0) n=0;
    x->channel = n;					// just store right operand value in instance's data structure and do nothing else
}
//----------------------------------------------------------------------------------------------------------------
// udmx_int: 	a list received in the main inlet
//
// 				update dmx buffer, send immediately if we can
//----------------------------------------------------------------------------------------------------------------
void udmx_int(t_udmx *x, t_int16 n){
    // x = the instance of the object; n = the int received in the left inlet
    
    if (n > 255) n=255;
    if (n < 0) n=0;
    
    
    if (x->dmx_buffer[x->channel] == n) return; // do nothing if value did not change
    
    x->dmx_buffer[x->channel] = n;
    
    if (x->clock_running) { // we're over the speed limit, track changes for later
        if (x->channel > x->channel_changed_max) x->channel_changed_max = x->channel;
        if (x->channel < x->channel_changed_min)  x->channel_changed_min = x->channel;
        
    } else {
        
        udmx_send_single(x,x->channel);
        
        if (x->speedlim) {
            // start clock
#ifdef PUREDATA   	// compiling for PUREDATA
            clock_delay(x->m_clock,x->speedlim);
#else				// compiling for MaxMSP
            clock_fdelay(x->m_clock,x->speedlim);
#endif				// Max/PD switch
            x->clock_running  = 1;
        }
    }
    
}

//----------------------------------------------------------------------------------------------------------------
// udmx_float: 	a float received in the main inlet
//
// 				update dmx buffer, send immediately if we can
//----------------------------------------------------------------------------------------------------------------
void udmx_float(t_udmx *x, double f){
    // x = the instance of the object; n = the int received in the left inlet
    
    if (f > 1.) f = 1.;
    if (f < 0) f=0;

    f *= 255.;
    unsigned char n = f;
    
    
    if (x->dmx_buffer[x->channel] == n) return; // do nothing if value did not change
    
    x->dmx_buffer[x->channel] = n;
    
    if (x->clock_running) { // we're over the speed limit, track changes for later
        if (x->channel > x->channel_changed_max) x->channel_changed_max = x->channel;
        if (x->channel < x->channel_changed_min)  x->channel_changed_min = x->channel;
        
    } else {
        
        udmx_send_single(x,x->channel);
        
        if (x->speedlim) {
            // start clock
#ifdef PUREDATA   	// compiling for PUREDATA
            clock_delay(x->m_clock,x->speedlim);
#else				// compiling for MaxMSP
            clock_fdelay(x->m_clock,x->speedlim);
#endif				// Max/PD switch
            x->clock_running  = 1;
        }
    }
    
}


//----------------------------------------------------------------------------------------------------------------
// udmx_list: 	a list received in the main inlet
//
// 				update dmx buffer, send immediately if we can
//----------------------------------------------------------------------------------------------------------------
void udmx_list(t_udmx *x, t_symbol *s, short ac, t_atom *av) {
    
    t_uint16 i,chan,val,change_min,change_max;
    change_min = 512;
    change_max = 0;
    
    for(i=0; i<ac; ++i,av++) {
        
        
#ifdef PUREDATA   	// compiling for PUREDATA
        if (av->a_type==A_FLOAT)	val = av->a_w.w_float;
        else val = 0;
        if (val > 255) val = 255;
        if (val < 255) val = 0;
        
#else				// compiling for MaxMSP
        if (av->a_type==A_LONG)	val = MIN(MAX(av->a_w.w_long, 0), 255);
        else  if (av->a_type==A_FLOAT) {
         val = MIN(MAX(av->a_w.w_float * 255., 0), 255);
        }
        else val = 0;
#endif				// Max/PD switch
        
        
        chan = x->channel+i;
        
        if (val != x->dmx_buffer[chan]) {
            x->dmx_buffer[chan] = val;
            if (change_min > chan) change_min = chan;
            if (change_max < chan) change_max = chan;
        }
    }
    
    if (change_min > change_max) return;	// if there are no changes do nothing
    
    if (x->clock_running) { 				// we're over the speed limit, track changes for later
        if (change_min < x->channel_changed_min)  	x->channel_changed_min = change_min;
        if (change_max > x->channel_changed_max) 	x->channel_changed_max = change_max;
        
    } else {
        if (change_max - change_min) udmx_send_range(x,change_min,change_max);
        else udmx_send_single (x,change_min);
        
        if (x->speedlim) {
            // start clock -> prevent sending over speed limit
#ifdef PUREDATA   	// compiling for PUREDATA
            clock_delay(x->m_clock,x->speedlim);
#else				// compiling for MaxMSP
            clock_fdelay(x->m_clock,x->speedlim);
#endif				// Max/PD switch			x->clock_running  = 1;
        }
        
    }
}
//----------------------------------------------------------------------------------------------------------------
// udmx_send_single
//
//	-> change only one channel
//----------------------------------------------------------------------------------------------------------------
void udmx_send_single(t_udmx *x, t_uint16 chann) {
    
    
    t_uint16 nBytes;
    
    if (!(x->dev_handle)) {
        if (chann < x->channel_changed_min) x->channel_changed_min = chann;
         if (chann > x->channel_changed_max)x->channel_changed_max = chann;
          udmx_tick(x);
    } else {
        nBytes = usb_control_msg(x->dev_handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
                                 cmd_SetSingleChannel, x->dmx_buffer[chann], chann, NULL, 0, 200);
        if(nBytes < 0)
            if (x->debug_flag) error("udmx: USB error: %s", usb_strerror());
    }
}
//----------------------------------------------------------------------------------------------------------------
// udmx_send_range
//
// 	-> update a range of channels
//----------------------------------------------------------------------------------------------------------------
void udmx_send_range(t_udmx *x, t_uint16 from, t_uint16 to) {
    
    if (!(x->dev_handle)) udmx_tick(x);
    else {
        t_uint16 i,len,nBytes;
        from = MIN(MAX(from,0),510);
        to = MIN(MAX(to,1),511);
        
        if (to <= from) {to = from+1;}
        len = to - from + 1;
        
        char* buf = malloc( len );
        
        for(i=0; i<len; i++) {
            buf[i] = x->dmx_buffer[from+i];
        }
        
        nBytes = usb_control_msg(x->dev_handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
                                 cmd_SetChannelRange, len, from, buf, len, 1000);
        
        if (x->debug_flag) post( "bytes returned: %i\n", nBytes);
        if(nBytes < 0){
            if (x->debug_flag) error("udmx: USB error: %s\n", usb_strerror());
        } else if(nBytes > 0) { if (x->debug_flag) post("udmx: returned: %i\n", (int)(buf[0]));}
        
        free(buf);
    }
    
}
//----------------------------------------------------------------------------------------------------------------
// clock tick
void udmx_tick(t_udmx *x) {

    if (!(x->dev_handle)) {
        qelem_set(x->m_qelem);
      //  clock_fdelay(x->m_clock,4000); //check again periodically
        return;
    }

    if (x->channel_changed_min < 512) { // there was a change since we last sent something
        
        if (x->channel_changed_max - x->channel_changed_min) {
            udmx_send_range(x,x->channel_changed_min,x->channel_changed_max);
        } else {
            udmx_send_single(x,x->channel_changed_min);
        }
        
    }
    
    x->channel_changed_min = 512;
    x->channel_changed_max = -1;
    x->clock_running = 0;
    
}
//----------------------------------------------------------------------------------------------------------------
// set speed limit in ms
void udmx_speedlim(t_udmx *x, t_uint16 n){
    if (n < 0) n = 0;
    x->speedlim = n;
    x->clock_running = 0;
}
//----------------------------------------------------------------------------------------------------------------
// establish connection with the udmx hardware
void udmx_open(t_udmx *x){
    
    if (x->dev_handle) {
        udmx_message(x,gensym("There is already a connection to www.anyma.ch/udmx"));
#ifdef PUREDATA   	// compiling for PUREDATA
        outlet_float(x->statusOutlet,1);
#else				// compiling for MaxMSP
        outlet_int(x->statusOutlet,1);
#endif				// Max/PD switch
    } else         qelem_set(x->m_qelem);

}
//----------------------------------------------------------------------------------------------------------------
// establish connection with the udmx hardware by serial number
void udmx_bind(t_udmx *x, t_symbol *s) {
    if (x->dev_handle) { udmx_close(x); }
    
    if (s == gensym("")) {
        x->bind_to[0] = 0;
    } else {
        strcpy(x->bind_to,s->s_name);
    }
    
    qelem_set(x->m_qelem);
}

//----------------------------------------------------------------------------------------------------------------
// post serial number
void udmx_getSerial(t_udmx *x){

    if (x->dev_handle) {
        t_atom argv[1];
        atom_setsym(argv, gensym(x->serial_number));
        outlet_anything(x->msgOutlet, gensym ("serial"), 1, argv);
    } else {
        outlet_anything(x->msgOutlet, gensym ("Not connected to an udmx"), 0, NULL);
    }
    
}

void udmx_blackout(t_udmx *x){
    t_uint16 i;
    for (i = 0; i < 512; i++){
        x->dmx_buffer[i] = 0;
    }
    udmx_send_range(x, 0, 511);
}

//----------------------------------------------------------------------------------------------------------------
// close connection to hardware
void udmx_close(t_udmx *x){
#ifdef PUREDATA   	// compiling for PUREDATA
    outlet_float(x->statusOutlet,0);
#else				// compiling for MaxMSP
    outlet_int(x->statusOutlet,0);
#endif				// Max/PD switch
    
    if (x->dev_handle) {
        usb_close(x->dev_handle);
        x->dev_handle = NULL;
        udmx_message(x,gensym("Closed connection to www.anyma.ch/udmx"));
    } else
        udmx_message(x,gensym("There was no open connection to www.anyma.ch/udmx"));
}
//----------------------------------------------------------------------------------------------------------------
// Asssistance when user hovers over inlets and outlets
#ifndef PUREDATA
void udmx_assist(t_udmx *x, void *b, t_uint16 m, t_uint16 a, char *s) {
    // 4 final arguments are always the same for the assistance method
    
    if (m == ASSIST_OUTLET) {
        switch(a) {
            case 0:
                sprintf(s,"Connection status (1/0)");
                break;
            case 1:
                sprintf(s,"Various messages");
                break;
        }
    } else {
        switch (a) {
            case 0:
                sprintf(s,"DMX Packet (list)");
                break;
            case 1:
                sprintf(s,"Start address (int)");
                break;
        }
    }
}
#endif
//----------------------------------------------------------------------------------------------------------------
// object creation


int C74_EXPORT main(void){
    
    t_class *c;
    
    c = class_new("udmx", (method)udmx_new, (method)udmx_free, (long)sizeof(t_udmx),
                  0L /* leave NULL!! */, A_GIMME, 0);
    
    class_addmethod(c, (method)udmx_int,			"int",		A_LONG, 0);
    class_addmethod(c, (method)udmx_in1,			"in1",		A_LONG, 0);
    class_addmethod(c, (method)udmx_list,			"list", A_GIMME, 0);

    class_addmethod(c, (method)udmx_float,			"float",	A_FLOAT, 0);
    class_addmethod(c, (method)udmx_open,			"open", 0);
    class_addmethod(c, (method)udmx_blackout,		"blackout", 0);
    
    class_addmethod(c, (method)udmx_getSerial,		"get_serial", 0);
    class_addmethod(c, (method)udmx_close, 			"close", 0);
    class_addmethod(c, (method)udmx_speedlim, 		"speedlim", A_FLOAT,0);
    class_addmethod(c, (method)udmx_bind, 			"bind", A_DEFSYM,0);

    class_register(CLASS_BOX, c);
    
    udmx_class = c;
    
    return 0;
}



//----------------------------------------------------------------------------------------------------------------
// object creation
void *udmx_new(t_symbol *s, long argc, t_atom *argv)	{
    

    t_atom *ap;
    t_int16 n;
    n = 0;
    
 
    // n = int argument typed into object box (A_DEFLONG) -- defaults to 0 if no args are typed
    
    t_udmx *x = (t_udmx *)object_alloc(udmx_class);
    intin(x,1);					// create a second int inlet (leftmost inlet is automatic - all objects have one inlet by default)
    x->m_clock = clock_new(x,(method)udmx_tick); 	// make new clock for polling and attach tick function to it
    x->m_qelem = qelem_new((t_object *)x, (method)find_device);
    
    x->msgOutlet = outlet_new(x,0L);	//create right outlet
    x->statusOutlet = outlet_new(x,0L);	//create an outlet for connected flag
    
    x->correct_adressing = 1;
    
    if (argc) {
        ap = argv;
        switch (atom_gettype(ap)) {
            case A_LONG:
                n = atom_getlong(ap);
                break;
            default:
                break;
        }
        ap = &argv[argc-1];
        
        switch (atom_gettype(ap)) {
            case A_LONG:
                if (atom_getlong(ap) == 0 ) {
                    x->correct_adressing = 0;
                }
                break;
            default:
                break;
        }
     }
    if(x->correct_adressing) {n = n - 1;}
    if (n < 0) n = 0;
    if (n > 511) n = 511;

    x->channel = n;
    x->debug_flag = 0;
    x->channel_changed_min = 512;
    x->channel_changed_max = -1;
    x->clock_running = 0;
    x->dev_handle = NULL;
    x->speedlim = SPEED_LIMIT;
    x->usb_devices_seen = -1;

    
    clock_fdelay(x->m_clock,100);
    usb_init();
    
    return(x);					// return a reference to the object instance
}
//----------------------------------------------------------------------------------------------------------------
// object destruction
void udmx_free(t_udmx *x){
    object_free(x->m_clock);
    qelem_free(x->m_qelem);
    if (x->dev_handle)
        udmx_close(x);
}
//----------------------------------------------------------------------------------------------------------------
// USB HELPER FUNCTIONS
static int  usbGetStringAscii(usb_dev_handle *dev, t_uint16 index, t_uint16 langid, char *buf, t_uint16 buflen){
    
    char    buffer[256];
    t_uint16     rval, i;
    
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

char isOurVIDandPID(struct usb_device const* dev) {
    return dev->descriptor.idVendor == USBDEV_SHARED_VENDOR &&
    (dev->descriptor.idProduct == USBDEV_SHARED_PRODUCT ||
     dev->descriptor.idProduct == USBDEV_SHARED_PRODUCT_HID ||
     dev->descriptor.idProduct == USBDEV_SHARED_PRODUCT_MIDI);
}


void find_device(t_udmx *x) {
    usb_dev_handle      *handle = NULL;
    struct usb_bus      *bus;
    struct usb_device   *dev;
    
    t_int16 device_count;
    
    usb_find_busses();
    device_count = usb_find_devices();
    if (device_count == x->usb_devices_seen) {
        return;
    }
    x->usb_devices_seen = device_count;
    
    for(bus=usb_busses; bus; bus=bus->next){
        for(dev=bus->devices; dev; dev=dev->next){
            if(isOurVIDandPID(dev)){
                char    string[256];
                int     len;
                handle = usb_open(dev); /* we need to open the device in order to query strings */
                if(!handle){
                    error ("Warning: cannot open USB device: %s", usb_strerror());
                    continue;
                }
                /* now find out whether the device actually is obdev's Remote Sensor: */
                len = usbGetStringAscii(handle, dev->descriptor.iManufacturer, 0x0409, string, sizeof(string));
                if(len < 0){
                    error("udmx: warning: cannot query manufacturer for device: %s", usb_strerror());
                    goto skipDevice;
                }
                /* post("udmx: seen device from vendor ->%s<-", string); */
                if(strcmp(string, "www.anyma.ch") != 0)
                    goto skipDevice;
                len = usbGetStringAscii(handle, dev->descriptor.iProduct, 0x0409, string, sizeof(string));
                if(len < 0){
                    error("udmx: warning: cannot query product for device: %s", usb_strerror());
                    goto skipDevice;
                }
                // post("udmx: seen product ->%s<-", string);
                if(strcmp(string, "udmx") == 0 || strcmp(string, "uDMX") == 0) {
                    // we've found a udmx device. get serial number
                    
                    if	 (dev->descriptor.iSerialNumber) {
                        len = usb_get_string_simple(handle, dev->descriptor.iSerialNumber, x->serial_number, sizeof (x->serial_number));
                        if (len == 0) {
                            error("Unable to fetch serial number string");
                        }
                    }
                    
                    //see if we're looking for a specific serial number
                    if (x->bind_to[0]) {
                        if (strcmp(x->bind_to,x->serial_number)) {
                            post("Found device for another serial number: %s\n", x->serial_number);	
                            goto skipDevice;
                        }
                    }
                    
                    break;
                }
            skipDevice:
                usb_close(handle);
                handle = NULL;
            }
        }
        if(handle)
            break;
    }
    
    if(!handle){
        udmx_message(x,gensym("Could not find USB device www.anyma.ch/udmx"));
#ifdef PUREDATA   	// compiling for PUREDATA 
        outlet_float(x->statusOutlet,0);
#else				// compiling for MaxMSP
        outlet_int(x->statusOutlet,0);
#endif				// Max/PD switch
        x->dev_handle = NULL;
    } else {
        x->dev_handle = handle;
#ifdef PUREDATA   	// compiling for PUREDATA 
        outlet_float(x->statusOutlet,1);
#else				// compiling for MaxMSP
        outlet_int(x->statusOutlet,1);
#endif				// Max/PD switch
        udmx_message(x,gensym("Found USB device www.anyma.ch/udmx"));
    }
}