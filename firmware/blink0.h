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

#ifndef BLINK0_H__
#define BLINK0_H__

#include "usb.h"
#include <xc.h>
#include <string.h>
#include "usb_config.h"
#include "usb_ch9.h"
#include "usb_hid.h"

#define LED_COUNT     18

struct ws_led_struct
{
	uint8_t g, r, b; /* the order is critical: the WS281x expects green, red, then blue */
};

struct bookkeep_struct
{
	uint16_t increment;
	uint8_t fraction;
};

struct target_struct
{
	struct ws_led_struct leds;
	uint16_t fade_delay;
	struct bookkeep_struct bookkeep_g, bookkeep_r, bookkeep_b;
};

#endif /* BLINK0_H__ */
