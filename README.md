This project is designed to control both my Energenie radiator valves and my Salus RT510TX boiler controller.

It runs on a Raspberry Pi with an Energenie 433Mhz ENER314-RT 
<https://energenie4u.co.uk/catalogue/product/ENER314-RT> two way radio controller board attached.

I modded my ENER314-RT board to attach an additional 868MHz radio so I can control the Salus boiler controller as well.


## ENER314-RT 868 Mhz Hardware modification

by attaching an 868Mhz RFM69CW to its SPI lines. 
This is so I can also control 868Mhz devices (such as my Salus RT510TX based boiler) as well.


## Energenie protocol and code
Energenie devices use the OpenThings protocol.

The code for controlling the energenie valves is open source, but I found the various libraries impenetrable. 

Therefore I decided to rewrite it myself in pure C. However, this rewrite only supports the radiator valves, not Energenie's 
entire product suite.


## Salus RT500 / RT510 protocol and code 


## References
* Energenie library <https://github.com/Energenie/pyenergenie>
* Whaleygeek's energenie library <https://github.com/whaleygeek/pyenergenie>
