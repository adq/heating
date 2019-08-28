heating:
	gcc energenie.c pihw.c radio.c main.c -o heating -lbcm2835 -lpthread


clean:
	rm -f heating *~
