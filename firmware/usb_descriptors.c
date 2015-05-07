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

#include "usb_config.h"
#include "usb.h"
#include "usb_ch9.h"
#include "usb_hid.h"

#ifdef __C18
#define ROMPTR rom
#else
#define ROMPTR
#endif

struct configuration_1_packet
{
	struct configuration_descriptor  config;
	struct interface_descriptor      interface;
	struct hid_descriptor            hid;
	struct endpoint_descriptor       ep1_in;
};


/* Device Descriptor */
const ROMPTR struct device_descriptor this_device_descriptor =
{
	sizeof(struct device_descriptor), // bLength
	DESC_DEVICE, // bDescriptorType
	0x0200, // 0x0200 = USB 2.0, 0x0110 = USB 1.1
	0x00, // Device class
	0x00, // Device Subclass
	0x00, // Protocol.
	EP_0_LEN, // bMaxPacketSize0
	0x27B8, // Vendor
	0x01ED, // Product
	0x0002, // device release
	1, // Manufacturer
	2, // Product
	3, // Serial
	NUMBER_OF_CONFIGURATIONS // NumConfigurations
};

static const ROMPTR uint8_t hid_report_descriptor[] =
{
    /* for better or worse, this the descriptor to look like the Blink(1) */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x85, 0x01,                    //   REPORT_ID (1)
    0x95, 8,                       //   REPORT_COUNT (8)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

/* Configuration 1 Descriptor */
static const ROMPTR struct configuration_1_packet configuration_1 =
{
	{
	// Members from struct configuration_descriptor
	sizeof(struct configuration_descriptor),
	DESC_CONFIGURATION,
	sizeof(configuration_1), // wTotalLength (length of the whole packet)
	1, // bNumInterfaces
	1, // bConfigurationValue
	0, // iConfiguration (index of string descriptor)
	0b10000000,
	120/2,   // 120mA
	},

	{
	// Members from struct interface_descriptor
	sizeof(struct interface_descriptor), // bLength;
	DESC_INTERFACE,
	0x0, // InterfaceNumber
	0x0, // AlternateSetting
	0x1, // bNumEndpoints (num besides endpoint 0)
	HID_INTERFACE_CLASS, // bInterfaceClass 3=HID
	0x00, // bInterfaceSubclass
	0x00, // bInterfaceProtocol
	0x00, // iInterface (index of string describing interface)
	},

	{
	// Members from struct hid_descriptor
	sizeof(struct hid_descriptor),
	DESC_HID,
	0x0101, // bcdHID
	0x0, // bCountryCode
	1,   // bNumDescriptors
	DESC_REPORT, // bDescriptorType2
	sizeof(hid_report_descriptor), // wDescriptorLength
	},

	{
	// Members of the Endpoint Descriptor (EP1 IN)
	sizeof(struct endpoint_descriptor),
	DESC_ENDPOINT,
	0x01 | 0x80, // endpoint #1 0x80=IN
	EP_INTERRUPT, // bmAttributes
	EP_1_IN_LEN, // wMaxPacketSize
	1, // bInterval in ms.
	},
};

/* String Descriptors */

static const ROMPTR struct { uint8_t bLength; uint8_t bDescriptorType; uint16_t lang; } str00 = 
{
	sizeof(str00),
	DESC_STRING,
	0x0409 // US English
};

static const ROMPTR struct { uint8_t bLength; uint8_t bDescriptorType; uint16_t chars[12]; } product_string = 
{
	sizeof(product_string),
	DESC_STRING,
	{'b','l','i','n','k','(','1',')',' ','m','k','2'}
};

static const ROMPTR struct { uint8_t bLength; uint8_t bDescriptorType; uint16_t chars[6]; } manufacturer_string = 
{
	sizeof(manufacturer_string),
	DESC_STRING,
	{'T','h','i','n','g','M'}
};

static const ROMPTR struct { uint8_t bLength; uint8_t bDescriptorType; uint16_t chars[8]; } serial_string = 
{
	sizeof(serial_string),
	DESC_STRING,
	{'9','6','C','5','A','2','3','9'}
};

/* Get String function
 *
 * This function is called by the USB stack to get a pointer to a string
 * descriptor.  If using strings, USB_STRING_DESCRIPTOR_FUNC must be defined
 * to the name of this function in usb_config.h.  See
 * USB_STRING_DESCRIPTOR_FUNC in usb.h for information about this function.
 * This is a function, and not simply a list or map, because it is useful,
 * and advisable, to have a serial number string which may be read from
 * EEPROM or somewhere that's not part of static program memory.
 */
int16_t usb_application_get_string(uint8_t string_number, const void **ptr)
{
	if (0 == string_number)
	{
		*ptr = &str00;
		return sizeof(str00);
	}
	else if (1 == string_number)
	{
		*ptr = &manufacturer_string;
		return sizeof(manufacturer_string);
	}
	else if (2 == string_number)
	{
		*ptr = &product_string;
		return sizeof(product_string);
	}
	else if (3 == string_number)
	{
		*ptr = &serial_string;
		return sizeof(serial_string);
	}

	return -1;
}

/* Configuration Descriptor List */
const struct configuration_descriptor *usb_application_config_descs[] =
{
	(struct configuration_descriptor*) &configuration_1,
};

STATIC_SIZE_CHECK_EQUAL(USB_ARRAYLEN(USB_CONFIG_DESCRIPTOR_MAP), NUMBER_OF_CONFIGURATIONS);
STATIC_SIZE_CHECK_EQUAL(sizeof(USB_DEVICE_DESCRIPTOR), 18);

/* HID Descriptor Function */
int16_t usb_application_get_hid_descriptor(uint8_t interface, const void **ptr)
{
	/* Only one interface in this demo. The two-step assignment avoids an
	 * incorrect error in XC8 on PIC16. */
	const void *p = &configuration_1.hid;
	*ptr = p;
	return sizeof(configuration_1.hid);
}

/** HID Report Descriptor Function */
int16_t usb_application_get_hid_report_descriptor(uint8_t interface, const void **ptr)
{
	*ptr = hid_report_descriptor;
	return sizeof(hid_report_descriptor);
}
