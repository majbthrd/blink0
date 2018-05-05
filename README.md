## blink0: genuinely open-source firmware that emulates a Blink(1)

Copyright (C) 2015 Peter Lawrence

based on top of M-Stack USB driver stack by Alan Ott, Signal 11 Software

The author's intent in writing this code is to provide more readable 
firmware source code that can be used in tandem with a bootloader.
This enables the hobbyist/maker to experiment, innovate, and improve 
far more readily than may be possible with the Blink(1).

The suggested open-source PIC16F1454 bootloader is:

https://github.com/majbthrd/PIC16F1-USB-DFU-Bootloader/

![blink0](http://www.2x4logic.com/mb0.jpg)

### Usage

blink0 emulates Blink(1), but does so with open-source software.  Software written for Blink(1) should also work with blink0.

### Firmware Updates

Unlike Blink(1), blink0 is designed to allow the hobbyist/maker to modify the firmware.

The following hidden command in ThingM's command line utility is used to activate the bootloader:

```
blink1-tool --testtest
```

This instructs the blink0 to boot into the [PIC16F1-USB-DFU-Bootloader](https://github.com/majbthrd/PIC16F1-USB-DFU-Bootloader/).

After this, the user can use [dfu-util](http://dfu-util.sourceforge.net/) to download new firmware:


```
dfu-util -D blink0.dfu
```
