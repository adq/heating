heating:
	gcc energenie.c hw.c radio.c main.c -o heating -lbcm2835 -lpthread


clean:
	rm -f heating *~
