This project is designed to run on a raspberry pi with an Energenie 433Mhz ENER314-RT 
<https://energenie4u.co.uk/catalogue/product/ENER314-RT> two way radio controller board attached.

Additionally I modded my ENER314-RT board to attach an additonal 868MHz radio by attaching an 868Mhz RFM69CW to its SPI lines. 
This is so I can also control 868Mhz devices (such as my Salus RT510TX based boiler) as well.

The intention is to control my Energenie radiator valves and my Salus RT510TX based boiler from the pi.

## 868 Mhz Hardware modification.


## Energenie code 
The code for controlling the energenie valves is open source, but I found the various libraries impenetrable. 
Therefore I decided to rewrite it myself in pure C. However, this rewrite only supports the radiator values, not Energenie's 
entire product suite.

## Salus protocol


## References
* Energenie library <https://github.com/Energenie/pyenergenie>
* Whaleygeek's energenie library <https://github.com/whaleygeek/pyenergenie>
