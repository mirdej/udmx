#ifndef __udmx_h_included__
#define __udmx_h_included__

#ifndef __ASSEMBLER__


#define NUM_CHANNELS 512		// number of channels in DMX-512

// values for dmx_state
#define dmx_Off 0
#define dmx_NewPacket 1	
#define dmx_InPacket 2
#define dmx_EndOfPacket 3
#define dmx_InBreak 4
#define dmx_InMAB 5

// values for usb_state
#define usb_NotInitialized 0
#define usb_Idle 1
#define usb_ChannelRange 2


// PORTB States for leds
#define LED_YELLOW 0x10
#define LED_GREEN 0x1
#define LED_BOTH 0x0
#define LED_NONE 0x11
#define LED_KEEP_ALIVE 200;


// function prototypes
void hadAddressAssigned(void);

// convenience macros (from Pascal Stangs avrlib)
#ifndef BV
	#define BV(bit)			(1<<(bit))
#endif
#ifndef cbi
	#define cbi(reg,bit)	reg &= ~(BV(bit))
#endif
#ifndef sbi
	#define sbi(reg,bit)	reg |= (BV(bit))
#endif
#endif
#endif


