This project is designed to control both my Energenie radiator valves and my Salus RT510RF / RT510TX boiler controller.

It runs on a Raspberry Pi with an Energenie 433Mhz ENER314-RT 
<https://energenie4u.co.uk/catalogue/product/ENER314-RT> two way radio controller board attached.

I modded my ENER314-RT board to attach an additional 868MHz radio so I can control the Salus boiler controller as well.


## ENER314-RT 868 Mhz Hardware modification
Looking at the ENER314-RT I realised it was using a HopeRF 433Mhz RFM69CW device.

I bought a similar HopeRF 866Mhz RFM69CW (and 866MHz helical antenna), and attached it to the same board using a different 
SPI Chip Select line.

![pi wiring](https://raw.githubusercontent.com/adq/heating/master/pi-with-866mhz.jpg "Pi 866Mhz Wiring")


## Energenie protocol and code
Energenie devices use the OpenThings protocol.

The code for controlling the energenie valves is open source, but I found the various libraries impenetrable. I decided to
rewrite it in C.

However, this rewrite only supports the radiator valves, not Energenie's entire product suite.


## Salus RT500 / RT510 protocol and code 
I found a few other projects which controlled the RT500, but it appears the protocol used by the RT510 is different.

## References
* Energenie library <https://github.com/Energenie/pyenergenie>
* Whaleygeek's energenie library <https://github.com/whaleygeek/pyenergenie>
* Cyclingengineer RT500RF project (dead) <http://the.cyclingengineer.co.uk/2013/11/23/home-automation-integrating-salus-rt500-rf-in-openhab-using-a-jeelink/>
* Klattimer Arduino RT500RF project <https://github.com/klattimer/salus-rt500rf>
