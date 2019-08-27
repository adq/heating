heating:
	gcc energenie.c lowlevel.c radio.c main.c -o heating -lbcm2835 -lpthread


clean:
	rm -f heating *~
