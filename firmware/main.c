/*
    blink0: genuinely open-source firmware that emulates a Blink(1)

    Copyright (C) 2015 Peter Lawrence

    based on top of M-Stack USB driver stack by Alan Ott, Signal 11 Software

    The author's intent in writing this code is to provide more readable 
    firmware source code that can be used in tandem with a bootloader.
    This enables the hobbyist/maker to experiment, innovate, and improve 
    far more readily than may be possible with the Blink(1).

    Permission is hereby granted, free of charge, to any person obtaining a 
    copy of this software and associated documentation files (the "Software"), 
    to deal in the Software without restriction, including without limitation 
    the rights to use, copy, modify, merge, publish, distribute, sublicense, 
    and/or sell copies of the Software, and to permit persons to whom the 
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in 
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
    DEALINGS IN THE SOFTWARE.
*/

#include "blink0.h"

/* 
since this is a downloaded app, configuration words (e.g. __CONFIG or #pragma config) are not relevant
*/

/*
local function prototyping
*/
static void set_target(uint8_t ledn);

static struct ws_led_struct leds[LED_COUNT + 1];

static uint8_t *ptr;

int main(void)
{
	/* SPI (WS281x) init */
	SSP1STAT = 0x40;
	SSP1CON1 = 0x20;
	ANSELCbits.ANSC2 = 0;
	TRISCbits.TRISC2 = 0;

	/* enable everything but global interrupts in preparation for SPI interrupt */
	PIR1bits.SSP1IF = 0;
	PIE1bits.SSP1IE = 1;
	INTCONbits.PEIE = 1;

	/* configure TMR2 for 100Hz (100.16Hz) */
	T2CONbits.T2CKPS = 0b11;    /* Prescaler is 64 */
	T2CONbits.T2OUTPS = 0b0111; /* Postscaler is 8 */
	PR2 = 234;
	T2CONbits.TMR2ON = 1;       /* enable TMR2 */

	usb_init();

	for (;;)
	{
		/* let the USB driver stack handle the USB functionality */
		usb_service();

		/* check if the timer has fired... */
		if (TMR2IF)
		{
			/* ... and if so, acknowledge it */
			TMR2IF = 0;

			/*
			this is the secret sauce to efficiently write to the WS281x
			rather than some pointlessly complicated fine-tuned delay loops, etc.,
			we just fire and forget using the interrupt service routine
			*/
			ptr = (uint8_t *)&leds[1];
			INTCONbits.GIE = 1;
			SSP1BUF = 0x00;
		}
	}
}

/* HID Callbacks for GET_REPORT and SET_REPORT via EP0 */

static uint8_t get_report_buf[EP_0_LEN + 1]; /* data to be sent to the PC in response to a GET_REPORT */
static uint8_t set_report_buf[EP_0_LEN + 1]; /* incoming data from PC sent via a SET_REPORT */

int16_t app_get_report_callback(uint8_t interface, uint8_t report_type,
                                uint8_t report_id, const void **report,
                                usb_ep0_data_stage_callback *callback,
                                void **context)
{
	*report = get_report_buf;
	*callback = NULL; /* indicate no callback needed */
	*context = NULL;
	return sizeof(get_report_buf);
}

static void set_report_callback(bool transfer_ok, void *context)
{
	uint8_t ledn, index;

	/* preemptively echo the contents of the SET_REPORT */
	memcpy(get_report_buf, set_report_buf, EP_0_LEN);

	/* only act upon messages sent with the right report id */
	if (0x01 != set_report_buf[0])
		return;

	ledn = set_report_buf[7];
	if (ledn > LED_COUNT)
		ledn = 0;

	leds[0].r = set_report_buf[2];
	leds[0].g = set_report_buf[3];
	leds[0].b = set_report_buf[4];

	switch (set_report_buf[1])
	{
	case 'c':
	case 'n': // 'n' is nothing but a pointless subset of 'c' and does not deserve its own code
		set_target(ledn);
		break;
	case '!':
		/* enable watchdog; the code doesn't clear the watchdog, so the PIC will eventually reset (into the bootloader) */
		WDTCONbits.SWDTEN = 1;
		break;
	case 'v':
		get_report_buf[3] = '2';
		get_report_buf[4] = '3';
		break;
	case 'r':
		get_report_buf[2] = leds[ledn].r;
		get_report_buf[3] = leds[ledn].g;
		get_report_buf[4] = leds[ledn].b;
		get_report_buf[5] = 0;
		get_report_buf[6] = 0;
		get_report_buf[7] = ledn;
		break;
	}
}

int8_t app_set_report_callback(uint8_t interface, uint8_t report_type, uint8_t report_id)
{
	usb_start_receive_ep0_data_stage(set_report_buf, sizeof(set_report_buf), &set_report_callback, NULL);

	return 0;
}

void interrupt isr()
{
	static uint8_t bit_position;
	static uint8_t byte_count;
	static uint8_t current_byte;

	/* check if SSP1IF interrupt has fired... */
	if (PIR1bits.SSP1IF)
	{
		/* ... and acknowledge it by clearing SSP1IF flag */
		PIR1bits.SSP1IF = 0;

		if (0 == bit_position)
		{
			/* 
			if bit_position is zero, we've exhausted all the bits in the previous byte 
			and need to load current_byte with the next byte
			*/
			if ((LED_COUNT * sizeof(struct ws_led_struct)) == byte_count)
			{
				/*
				we've reached the end of the LED data, and 
				the interrupt routine's work is done;
				so, we disable the interrupt and bail
				*/
				INTCONbits.GIE = 0;
				byte_count = 0;
				return;
			}

			/* load next byte into current_byte */			
			current_byte = *ptr++;
			byte_count++;
		}

		/* WS281X expects long pulse for '1' and short pulse for '0' */
		SSP1BUF = (current_byte & 0x80) ? 0xFF : 0xF0;

		/* preemptively shift next bit into position and update bit_position */
		current_byte <<= 1;
		bit_position = (bit_position + 1) & 0x7;		
	}
}

static void set_target(uint8_t ledn)
{
	uint8_t index;

	for (index = 1; index <= LED_COUNT; index++)
	{
		if (ledn && (ledn != index))
			continue;

		leds[index] = leds[0];
	}
}
