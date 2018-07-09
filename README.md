# Temperature sensor

Uses:

* Feather M0 + Lora
* DS18B20 temperature sensor
* 4.7kOhm resistor
* LiPo battery

Follow instructions here: https://www.loratas.io/blog/2017/11/30/workshop

# Pump remote receiver

Uses:

* Arduino
* Arduino LoRa Shield
* Relay shield
* Loud buzzer

Connect all the shields together, and plug positive of buzzer into pin 12. Plug negative into GND. Connect relay number 2 to the pump on switch and relay number 3 to the pump off switch. Power via 5V DC adaptor plug.

The receiver listens for commands (unencrypted, but hey - don't think anyone's going to be spoofing them) from the farm server over the LoRa radio.

* On receipt of `WVFPONXXX`, it sends a signal back to the server saying `Turning on`, and then schedules the turn-on job. This consists of turning on the siren for `SIREN_DURATION` (=60000 milliseconds); after that, it will send a pulse of `RELAY_PULSE_DURATION` (=1000 milliseconds) to relay number 2.
* On receipt of `WVFPOFFXX`, it sends back `Turning off` and then immediately sends a pulse of `RELAY_PULSE_DURATION` to relay number 3.
* On receipt of `WVFPQSTAT`, it sends back one byte of information: 0 for "pump has recently been turned off"; 1 for "pump has recently been turned on"; 255 for "nothing has recently happened". This allows for the farm server to verify receipt of the signal it sent.

If you need to adjust the relay signal duration, then:

* Grab the [Arduino IDE](https://www.arduino.cc/en/Main/Software).
* Grab a USB B - USB A lead and plug the Arduino into your PC.
* Set Tools > Board to "Arduino/Genuino Uno"
* Set Tools > Port to the Arduino USB port
* You need to download and install the [Radiohead library](http://www.airspayce.com/mikem/arduino/RadioHead/).
* Open `Pump_Receiver.ino` in Arduino and edit the `RELAY_PULSE_DURATION`.
* Hit Upload

You can debug the system by plugging the lead in and using the serial monitor in the Arduino IDE (under the tools menu).