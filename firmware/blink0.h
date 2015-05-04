/*
    blink0: 

    Copyright (C) 2015 Peter Lawrence
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

#endif /* BLINK0_H__ */
