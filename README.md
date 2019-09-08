This project is designed to run on a raspberry pi with an Energenie 433Mhz ENER314-RT 
<https://energenie4u.co.uk/catalogue/product/ENER314-RT> two way radio controller board attached.

Additionally I modded my ENER314-RT board to attach an additonal 868MHz radio so I can control my Salus boiler controller 
with it as well.

## ENER314-RT 868 Mhz Hardware modification

by attaching an 868Mhz RFM69CW to its SPI lines. 
This is so I can also control 868Mhz devices (such as my Salus RT510TX based boiler) as well.


## Energenie code and protocol
The code for controlling the energenie valves is open source, but I found the various libraries impenetrable. 

Therefore I decided to rewrite it myself in pure C. However, this rewrite only supports the radiator valves, not Energenie's 
entire product suite.

## Salus protocol


## References
* Energenie library <https://github.com/Energenie/pyenergenie>
* Whaleygeek's energenie library <https://github.com/whaleygeek/pyenergenie>
