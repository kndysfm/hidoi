# HID OI
The library to access HID more easily and simply with C++11 grammer.
It is only for Win32API application. 

It can notify connection/disconnection of a device by watching WM_DEVICECHANGE.

It can watch also WM_INPUT, so you can read input report not only from custom HID I/F but also from exclusively OS managed HID devices ; e.g. mouse or keyboard.

