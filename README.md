# Mitsubishi HI RC-E1 Wifi Controller

A man-in-the-middle program for ESP32 that lets you control Mitsubishi Heavy Industries heat pumps/air conditioning systems that use the wired RC-E1 controller (3 wires).

MHI models from around 2004-2006 (series FDTA, FDUA, FDTCA, FDKNA, FDURA, FDENA) should all be compatible with this controller. I only tested this with an FDURA301R, though.

## How does this work?

The RC-EC1 controller and your I/U (internal AC unit) communicate by sending 16-byte-long UART packets at 1200 bauds with 1 even partiy bit through the data line that connects the two devices, roughly once per second. The flow is: the controller sends the desired state, the I/U responds with some confirmation data that I haven't fully reverse engineered, and the controller responds again with an ACK that also includes most of the same state again.

This program works by reading the packets from one end of the data line and writing them on the other end in real time, while optionally modifying them in transit to change the setpoint temperature, mode, fan speed or on/off status. My unit doesn't have louver nor grill options, so I didn't reverse engineer those. 

There's one caveat: The wired controller won't know about the new state set by this, so it won't reflect it on the display. There is some logic in this code, though, to revert to the state set by the controller if any of it changes (ie: if you change the settings using the wired controller, they take precedence).

You can also easily remove the WiFi parts of this and use it on an Arduino. The first commits of this repo, actually, ran on an Arduino.

## What about the hardware part?

Software is soft, hardware is hard.

There are three wires between your I/U machine and your RC-E1 controller. Two of them are power and one is data (that is used bidirectionally). You will have to identify which one is which. This expects to be connected beetween the two ends of the data wire. HOWEVER, Mitsubishi's data cables use 12V, while the ESP32 uses 3.3V and Arduino uses 5V, so you will need two bidrectional logic level converters like the following schematic between the two pins on your ESP32/Arduino and the data cables that go to your I/U and your RC-E1.

![526842ae757b7f1b128b456f](https://user-images.githubusercontent.com/980842/157506552-de58be20-eea5-4c4d-ab3b-f63c7d425018.png)

I hope you don't break your AC while trying to hook this :D
