/*
	uDMX.c
	
	Max-Interface to the [ a n y m a | usb-dmx-interface ]
	
	Authors:	Max & Michael Egger
	Copyright:	2006-2008 [ a n y m a ]
	Website:	www.anyma.ch
	
	License:	GNU GPL 2.0 www.gnu.org
	
	Version:	2008-05-31
*/

#ifdef PUREDATA   // define PUREDATA if you want to compile for puredata
	#include "m_pd.h"
	#define SETSYM SETSYMBOL
#else	// compiling for MaxMSP
	#include "ext.h"  		// you must include this - it contains the external object's link to available Max functions
	#include "ext_common.h"
#endif


#ifdef PUREDATA   	// compiling for PUREDATA 
#else				// compiling for MaxMSP
#endif				// Max/PD switch

#include "../common/uDMX_cmds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include </opt/local/include/usb.h>    /* this is libusb, see http://libusb.sourceforge.net/ */

#define USBDEV_SHARED_VENDOR		0x16C0  /* VOTI */
#define USBDEV_SHARED_PRODUCT		0x05DC  /* Obdev's free shared PID */
#define USBDEV_SHARED_PRODUCT_MIDI   0x05E4  /* Obdev's free shared PID for MIDI devices*/
#define SPEED_LIMIT				10 		//  default transmission speed limit in ms

typedef struct _uDMX				// defines our object's internal variables for each instance in a patch
{
	t_object 		p_ob;			// object header - ALL objects MUST begin with this...
	int 			channel;		// int value - received from the right inlet and stored internally for each object instance
	usb_dev_handle	*dev_handle;	// handle to the uDMX converter
	int				debug_flag;
	void			*m_clock;		// handle to our clock
	int				clock_running;	// is our clock running?
	int				speedlim;
	void 			*statusOutlet;		// our status outlet
	void			*msgOutlet;		//
	int				dmx_buffer[512]; 
	int				channel_changed_min,channel_changed_max;
	char			serial_number[32];
	char			bind_to[32];
} t_uDMX;

void *uDMX_class;					// global pointer to the object class - so max can reference the object 

// these are prototypes for the methods that are defined below
void uDMX_int(t_uDMX *x, long n);
void uDMX_speedlim(t_uDMX *x, long n);
void uDMX_in1(t_uDMX *x, long n);
void uDMX_list(t_uDMX *x, t_symbol *s, short ac, t_atom *av);
void uDMX_open(t_uDMX *x);
void uDMX_close(t_uDMX *x);
void uDMX_bind(t_uDMX *x, t_symbol *s);
void uDMX_getSerial(t_uDMX *x);
void uDMX_assist(t_uDMX *x, void *b, long m, long a, char *s);
void *uDMX_new(long n);
static int  usbGetStringAscii(usb_dev_handle *dev, int index, int langid, char *buf, int buflen);
void find_device(t_uDMX *x);
void uDMX_send_range(t_uDMX *x, int from, int to);
void uDMX_send_single(t_uDMX *x, int chann);

void uDMX_message(t_uDMX *x,t_symbol *message) {
	t_atom out[1];
	SETSYM(out,message);	
	outlet_anything(x->msgOutlet,gensym("set"),1,&out);
	
}
//----------------------------------------------------------------------------------------------------------------
// Int in right inlet changes the channel address
//----------------------------------------------------------------------------------------------------------------
void uDMX_in1(t_uDMX *x, long n){
	// x = the instance of the object, n = the int received in the right inlet 	
	if (n > 511) n=511;
	if (n < 0) n=0;
	x->channel = n;					// just store right operand value in instance's data structure and do nothing else
}
//----------------------------------------------------------------------------------------------------------------
// uDMX_int: 	a list received in the main inlet
//
// 				update dmx buffer, send immediately if we can
//----------------------------------------------------------------------------------------------------------------
void uDMX_int(t_uDMX *x, long n){
// x = the instance of the object; n = the int received in the left inlet 

	if (n > 255) n=255;
	if (n < 0) n=0;
	
	
	if (x->dmx_buffer[x->channel] == n) return; // do nothing if value did not change

	x->dmx_buffer[x->channel] = n;
	
	if (x->clock_running) { // we're over the speed limit, track changes for later
		if (x->channel > x->channel_changed_max) x->channel_changed_max = x->channel;
		if (x->channel < x->channel_changed_min)  x->channel_changed_min = x->channel;

	} else {
		
		uDMX_send_single(x,x->channel);
		
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
// uDMX_list: 	a list received in the main inlet
//
// 				update dmx buffer, send immediately if we can
//----------------------------------------------------------------------------------------------------------------
void uDMX_list(t_uDMX *x, t_symbol *s, short ac, t_atom *av) {

	int i,chan,val,change_min,change_max;
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
		if (change_max - change_min) uDMX_send_range(x,change_min,change_max);
		else uDMX_send_single (x,change_min);
	
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
// uDMX_send_single
//
//	-> change only one channel
//----------------------------------------------------------------------------------------------------------------
void uDMX_send_single(t_uDMX *x, int chann) {

/*
		t_atom out[4];
		SETSYM(out,gensym("single"));
		SETLONG(out+1,chann);
		SETSYM(out+2,gensym("val"));
		SETLONG(out+3,x->dmx_buffer[chann]);

		outlet_list(x->outlet, 0L,4,&out);
		
*/			
		int nBytes;

		if (!(x->dev_handle)) find_device(x);
		else {
		nBytes = usb_control_msg(x->dev_handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
									cmd_SetSingleChannel, x->dmx_buffer[chann], chann, NULL, 0, 200);
		if(nBytes < 0)
			 if (x->debug_flag) error("uDMX: USB error: %s", usb_strerror());
		}
}
//----------------------------------------------------------------------------------------------------------------
// uDMX_send_range
//
// 	-> update a range of channels
//----------------------------------------------------------------------------------------------------------------
void uDMX_send_range(t_uDMX *x, int from, int to) {
	
/*			t_atom out[(to-from)+4];

		SETSYM	(out,	gensym("list"));
		SETLONG	(out+1,	from);
		SETSYM	(out+2,	gensym("vals"));

		for (i=0;i<(to-from)+1;i++){
			SETLONG	(out+3+i,	x->dmx_buffer[from+i]);
		}

		outlet_list(x->outlet, 0L,(to-from)+4,&out);		

*/


	if (!(x->dev_handle)) find_device(x);
	else {
		int i,len,nBytes;

		if (to <= from) {to = from+1;}
		len = to - from + 1;

		char* buf = malloc( len );

		for(i=0; i<len; i++) {
				buf[i] = x->dmx_buffer[from+i];
		}
		
		nBytes = usb_control_msg(x->dev_handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
									cmd_SetChannelRange, len, from, buf, len, 1000);
		
		if (x->debug_flag) post( "bytes returned: %i\n", nBytes);
		if(nBytes < 0)
           if (x->debug_flag) error("uDMX: USB error: %s\n", usb_strerror());
		else if(nBytes > 0)  if (x->debug_flag) post("uDMX: returned: %i\n", (int)(buf[0]));

		free(buf);
	}

}
//----------------------------------------------------------------------------------------------------------------
// clock tick
void udmx_tick(t_uDMX *x) { 

	if (x->channel_changed_min < 512) { // there was a change since we last sent something

		if (x->channel_changed_max - x->channel_changed_min) {
			uDMX_send_range(x,x->channel_changed_min,x->channel_changed_max);	
		} else { 
			uDMX_send_single(x,x->channel_changed_min);
		}

	}

	x->channel_changed_min = 512;
	x->channel_changed_max = -1;
	x->clock_running = 0;

} 
//----------------------------------------------------------------------------------------------------------------
// set speed limit in ms
void uDMX_speedlim(t_uDMX *x, long n){
	if (n < 0) n = 0;
	x->speedlim = n;
	x->clock_running = 0;
}
//----------------------------------------------------------------------------------------------------------------
// establish connection with the udmx hardware
void uDMX_open(t_uDMX *x){

	if (x->dev_handle) {
		uDMX_message(x,gensym("There is already a connection to www.anyma.ch/uDMX"));
#ifdef PUREDATA   	// compiling for PUREDATA 
		outlet_float(x->statusOutlet,1);
#else				// compiling for MaxMSP
		outlet_int(x->statusOutlet,1);
#endif				// Max/PD switch
	} else find_device(x);
}
//----------------------------------------------------------------------------------------------------------------
// establish connection with the udmx hardware by serial number
void uDMX_bind(t_uDMX *x, t_symbol *s) {
	if (x->dev_handle) { uDMX_close(x); }
	
	if (s == gensym("")) {
		x->bind_to[0] = 0;
	} else {
		strcpy(x->bind_to,s->s_name);
	}
	
	find_device(x);
}

//----------------------------------------------------------------------------------------------------------------
// post serial number
void uDMX_getSerial(t_uDMX *x){
	if (x->dev_handle) {
		post("uDMX serial number: %s\n", x->serial_number);
	} else {
		post("Not connected to an uDMX\n");
	}

}

//----------------------------------------------------------------------------------------------------------------
// close connection to hardware
void uDMX_close(t_uDMX *x){
#ifdef PUREDATA   	// compiling for PUREDATA 
		outlet_float(x->statusOutlet,0);
#else				// compiling for MaxMSP
		outlet_int(x->statusOutlet,0);
#endif				// Max/PD switch

	if (x->dev_handle) {
		usb_close(x->dev_handle);
		x->dev_handle = NULL;
		uDMX_message(x,gensym("Closed connection to www.anyma.ch/uDMX"));
	} else
		uDMX_message(x,gensym("There was no open connection to www.anyma.ch/uDMX"));
}
//----------------------------------------------------------------------------------------------------------------
// Asssistance when user hovers over inlets and outlets
#ifndef PUREDATA
void uDMX_assist(t_uDMX *x, void *b, long m, long a, char *s) {
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

#ifdef PUREDATA   	// compiling for PUREDATA 
	void udmx_setup(void)
	{
	
		uDMX_class = class_new ( gensym("uDMX"),(t_newmethod)uDMX_new, 0, sizeof(t_uDMX), 	CLASS_DEFAULT,0);
		
		class_addfloat(uDMX_class, (t_method)uDMX_int);			// the method for an int in the left inlet (inlet 0)
		class_addlist(uDMX_class, (t_method)uDMX_list);
		class_addmethod(uDMX_class, (t_method)uDMX_open, gensym("open"), 0);		
		class_addmethod(uDMX_class, (t_method)uDMX_getSerial, gensym("get_serial"), 0);		
		class_addmethod(uDMX_class, (t_method)uDMX_close, gensym("close"), 0);	
		class_addmethod(uDMX_class, (t_method)uDMX_speedlim, gensym("speedlim"), A_FLOAT,0);	
		class_addmethod(uDMX_class, (t_method)uDMX_bind, gensym("bind"), A_DEFSYM,0);	
	
	}
	
	void uDMX_setup(void) {
		udmx_setup();
	}
#else				// compiling for MaxMSP
	int main(void){
		setup((t_messlist **)&uDMX_class, (method)uDMX_new, 0L, (short)sizeof(t_uDMX), 0L, A_DEFLONG, 0); 
		// setup() loads our external into Max's memory so it can be used in a patch
		// uDMX_new = object creation method defined below, A_DEFLONG = its (optional) argument is a long (32-bit) int 
		
		addint((method)uDMX_int);			// the method for an int in the left inlet (inlet 0)
		addinx((method)uDMX_in1, 1);		// the method for an int in the right inlet (inlet 1)
		addmess((method)uDMX_list,"list", A_GIMME, 0);
		addmess((method)uDMX_open, "open", 0);		
		addmess((method)uDMX_getSerial, "get_serial", 0);
		addmess((method)uDMX_bind, "bind", A_DEFSYM,0);	
		addmess((method)uDMX_close, "close", 0);	
		addmess((method)uDMX_speedlim, "speedlim", A_LONG,0);	
		addmess((method)uDMX_assist, "assist", A_CANT, 0); // (optional) assistance method needs to be declared like this
		
		//uDMX_message(x,gensym("uDMX version 2008-06-07 - (c) 2008 [ a n y m a ]"));	// post any important info to the max window when our object is laoded
		return 0;
	}
#endif				// Max/PD switch


//----------------------------------------------------------------------------------------------------------------
// object creation
void *uDMX_new(long n)	{
	// n = int argument typed into object box (A_DEFLONG) -- defaults to 0 if no args are typed

	t_uDMX *x;				// local variable (pointer to a t_uDMX data structure)

	#ifdef PUREDATA   	// compiling for PUREDATA 
		x = (t_uDMX *)pd_new(uDMX_class); // create a new instance of this object
		floatinlet_new(x, x->channel); //assigns float in inlet 2 directly to channel
		x->m_clock = clock_new(x,(t_method)udmx_tick); 	// make new clock for polling and attach tick function to it
		x->msgOutlet = outlet_new(&x->p_ob,0L);	//create right outlet
		x->statusOutlet = outlet_new(&x->p_ob,0L);	//create an outlet for connected flag
	#else				// compiling for MaxMSP
		x = (t_uDMX *)newobject(uDMX_class); // create a new instance of this object
		intin(x,1);					// create a second int inlet (leftmost inlet is automatic - all objects have one inlet by default)
		x->m_clock = clock_new(x,(method)udmx_tick); 	// make new clock for polling and attach tick function to it
		x->msgOutlet = outlet_new(x,0L);	//create right outlet
		x->statusOutlet = outlet_new(x,0L);	//create an outlet for connected flag
	#endif				// Max/PD switch


	x->channel = n;
	x->debug_flag = 0;
	x->channel_changed_min = 512;
	x->channel_changed_max = -1;
	x->clock_running = 0;
	x->dev_handle = NULL;
	x->speedlim = SPEED_LIMIT;
	
	
	return(x);					// return a reference to the object instance 
}
//----------------------------------------------------------------------------------------------------------------
// object destruction
void uDMX_free(t_uDMX *x){
	if (x->dev_handle)
		uDMX_close(x);
}
//----------------------------------------------------------------------------------------------------------------
// USB HELPER FUNCTIONS
static int  usbGetStringAscii(usb_dev_handle *dev, int index, int langid, char *buf, int buflen){

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


void find_device(t_uDMX *x) {

	usb_dev_handle      *handle = NULL;
	struct usb_bus      *bus;
	struct usb_device   *dev;
	
	usb_find_busses();
    usb_find_devices();
	 for(bus=usb_busses; bus; bus=bus->next){
        for(dev=bus->devices; dev; dev=dev->next){
            if(dev->descriptor.idVendor == USBDEV_SHARED_VENDOR
				&& (dev->descriptor.idProduct == USBDEV_SHARED_PRODUCT || dev->descriptor.idProduct == USBDEV_SHARED_PRODUCT_MIDI)){
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
                    error("uDMX: warning: cannot query manufacturer for device: %s", usb_strerror());
                    goto skipDevice;
                }
                /* post("uDMX: seen device from vendor ->%s<-", string); */
                if(strcmp(string, "www.anyma.ch") != 0)
                    goto skipDevice;
                len = usbGetStringAscii(handle, dev->descriptor.iProduct, 0x0409, string, sizeof(string));
                if(len < 0){
                    error("uDMX: warning: cannot query product for device: %s", usb_strerror());
                    goto skipDevice;
                }
                // post("uDMX: seen product ->%s<-", string); 
                if(strcmp(string, "uDMX") == 0 || strcmp(string, "uDMX-midi") == 0) { 
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
        uDMX_message(x,gensym("Could not find USB device www.anyma.ch/uDMX"));
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
		 uDMX_message(x,gensym("Found USB device www.anyma.ch/uDMX"));
	}
}