USB CDC example
===============

The image consists of three applications:

- USB server in the folder task0_usb_cdc
- Main application in task1.c
- GPIO server which controls the LED in task2.c

The kernel part redirects USB interrupts as messages into the input channel 
of the USB server. USB server sends all incoming data from the CDC interface 
to the application. If the packet starts from '0' or '1' then the app sends
message to the LED server to switch the led into the corresponding state.

After device is connected to the host, locate the corresponding interface 
(like /dev/ttyACM0) and open it with terminal program like Putty or 
Teraterm. Then press '0' or '1', LED should switch according to the key.

