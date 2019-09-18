heating:
	gcc energenie.c hw.c radio.c salus.c main.c -o heating -lbcm2835 -lpthread -lmosquitto

salusmon:
	gcc energenie.c hw.c radio.c salus.c salusmon.c -o salusmon -lbcm2835 -lpthread

etest:
	gcc energenie.c hw.c radio.c salus.c etest.c -o etest -lbcm2835 -lpthread

clean:
	rm -f heating salusmon etest *~
