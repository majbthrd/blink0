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
static void adjust_led(volatile uint8_t *current, struct bookkeep_struct *bookkeep);

/*
local variables
*/

/* array storing the current state of all LEDs */
static struct ws_led_struct leds[LED_COUNT + 1];

/* pointer used by ISR to incrementally read leds[] array */
static uint8_t *ptr;

/* array storing the target LED values (and calculated step values to get there) */
static struct target_struct targets[LED_COUNT + 1];

int main(void)
{
	uint8_t count;
	struct target_struct *tpnt;
	struct ws_led_struct *lpnt;

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

			/*
			whilst the ISR takes care of talking to the WS281x, 
			we can focus on the heavy task of fading the LEDs
			*/
			tpnt = &targets[1]; lpnt = &leds[1];
			for (count = 0; count < LED_COUNT; count++)
			{
				if (0 == tpnt->fade_delay)
				{
					/* the fade has elapsed for this LED; write the final values */
					*lpnt = tpnt->leds;
				}
				else
				{
					/* the fade isn't finished, but we are one step (10ms) closer */
					tpnt->fade_delay--;

					/* do that embedded voodoo that you do to make the fade happen */
					adjust_led(&lpnt->g, &tpnt->bookkeep_g);
					adjust_led(&lpnt->r, &tpnt->bookkeep_r);
					adjust_led(&lpnt->b, &tpnt->bookkeep_b);
				}

				tpnt++; lpnt++;
			}
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
	uint8_t ledn;

	/* preemptively echo the contents of the SET_REPORT */
	memcpy(get_report_buf, set_report_buf, EP_0_LEN);

	/* only act upon messages sent with the right report id */
	if (0x01 != set_report_buf[0])
		return;

	ledn = set_report_buf[7];
	if (ledn > LED_COUNT)
		ledn = 0;

	targets[0].leds.r = set_report_buf[2];
	targets[0].leds.g = set_report_buf[3];
	targets[0].leds.b = set_report_buf[4];

	targets[0].fade_delay = (uint16_t)set_report_buf[5] << 8;
	targets[0].fade_delay += set_report_buf[6];

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

static void calc_increment(volatile uint8_t *current, volatile uint8_t *target, struct bookkeep_struct *bookkeep)
{
	uint8_t updir;
	uint16_t distance;

	/* are we going UP or DOWN? */
	updir = *target > *current;

	/* figure the distance that we have to travel */
	if (updir)
		distance = (uint16_t)*target - (uint16_t)*current;
	else
		distance = (uint16_t)*current - (uint16_t)*target;

	/*
	power of 2 multiple by 256
	the 0 to 255 distance value is now super-sized to 0 to 65280
	*/
	distance <<= 8;

	bookkeep->increment = 0;
	bookkeep->fraction = 0;

	/*
	writing tight embedded code means squeezing extra efficiency whenever possible

	what we want is this:
	bookkeep->increment = distance / targets[0].fade_delay;

	but if we write it like this, we use less flash memory:
	*/
	while (distance > targets[0].fade_delay)
	{
		distance -= targets[0].fade_delay;
		bookkeep->increment++;
	}

	/*
	here is another embedded code trick:
	by mangling the computed result slightly, we can store our computed flag for later use
	blink0 has more than twice the fade calc precision of Blink(1), so we still come out ahead of the competition
	*/
	if (updir)
		bookkeep->increment |= 1;
	else
		bookkeep->increment &= 0xFFFE;
}

static void set_target(uint8_t ledn)
{
	uint8_t index;
	struct target_struct *tpnt = &targets[1];
	struct ws_led_struct *lpnt = &leds[1];

	/* sequence through all the LEDs in turn */
	for (index = 1; index <= LED_COUNT; index++)
	{
		/* check if this LED is being written to */
		if (!ledn || (ledn == index))
		{
			/* write the new target value */
			*tpnt = targets[0];

			/* calculate the fade values to aim for each of the target colors */
			calc_increment(&lpnt->g, &targets[0].leds.g, &tpnt->bookkeep_g);
			calc_increment(&lpnt->r, &targets[0].leds.r, &tpnt->bookkeep_r);
			calc_increment(&lpnt->b, &targets[0].leds.b, &tpnt->bookkeep_b);
		}

		/* advance to the next target and led */
		*tpnt++; *lpnt++;
	}
}

static void adjust_led(volatile uint8_t *current, struct bookkeep_struct *bookkeep)
{
	uint8_t updir, msb;
	uint16_t change;

	/* retrieve the flag we hid earlier */
	updir = bookkeep->increment & 1;

	/*
	showing off with writing tight embedded code again
	Blink(1) wastes 16-bits PLUS 8-bits of RAM to store each current LED; blink0 does better

	"fraction" is an 8-bit expression of the fraction after the decimal point of the current 8-bit LED value
	the calculated "change" value is the step next LED value... expressed as a combination of whole and fraction
	we add the whole to the current LED value and write the new fraction back to "fraction"
	*/

	change = (uint16_t)bookkeep->fraction + bookkeep->increment;

	msb = change >> 8;
	if (updir)
		(*current)+=msb;
	else
		(*current)-=msb;

	bookkeep->fraction = change & 0xFF;
}
