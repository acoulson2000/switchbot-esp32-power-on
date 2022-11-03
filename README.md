# Switchbot-esp32-power-on
This sketch automatically activeate a SwitchBot Bot device with a "Press" command on boot-up. I created it to automatically press the power button on a USB RAID drive when power is restored following an outage.

## Background

Being a cheapskate, I built my own Network Attached Storage (NAS) solution using a Raspberry Pi and a 5-bay USB3-attached RAID disk enclosure. This works great, and lets me run other processes on the Pi, including my Emby media server.

The one problem is that, when there is a power outage and power is restored, although the Pi boots up, you have to manually press the power button on the back of the disk enclosure. It's an unfortunate design flaw, but I guess you get what you pay for :-) In my neighborhood, between the old infrastructure, numerous trees getting up into power lines, and Texas heat and wind storms, this happens somewhat frequently.

For a while, I've been thinking it would be nice to have a device that basically does one power-button-push upon power-up. I've googled around and haven't found exactly the right thing, but I discovered something close.

SwitchBot makes the "SwitchBot Bot" (here on Amazon), which is ideally suited for pressing the button, but it falls short in a couple of way:

It's more geared towards home-automation tasks - turning switches on and off on-demand, using a phone app to control it, setting up time-based actions...that kind of thing. There is no "press a button once after power is restored" function.

It doesn't connect to your AC power (which is nice), but to ensure battery life, it used Bluetooth Low Energy (BLE) connectivity. So you kind of have to use your phone, although you can buy a "Switchbot Hub" for another 40 bucks.

So, being a long-time enthusiast of all things ESP (ESP8266, ESP32 microcontroller SOCs), my mind immediately went to the idea of looking for some code out there that might integrate the ESP32 with the Bot. The ESP32 includes on-board Bluetooth in addition to CPU, memory and WiFi.


## The Code

I started with the code here: https://github.com/mmglodekk/switchbot-esp32/blob/main/switchbot-esp32.ino but it did not work - commands sent never effected any action on the Switchbot. I ended up borrowing heavily from https://github.com/devWaves/SwitchBot-MQTT-BLE-ESP32, which has more robust scanning and command code, but since it is intended as an MQTT-to-Switchbot bridge, it had tons of code I didn't need. MQTT support is neat, but overkill for my requirement of "just press a button on boot-up", so i borrowed only the bits involved in BLE commmunication.

It actually took a while to get even that to work - my code was behaving like the switch [switchbot-esp32](https://github.com/mmglodekk/switchbot-esp32/blob/main/switchbot-esp32.ino) - seeming to send the command, but the device ignoring it. It turns out my problem, and probably why the switchbot-esp32 code didn't work, was that i *needed* to register a notification callback (see subscribeToNotify function) for it to work. 

Currently, you have to manually edit and set the botAddr variable to you Bot's MAC address, then flash your ESP32. In the future, I may add the [WiFiManager](https://github.com/tzapu/WiFiManager) library to provide a web-based configuration screen.