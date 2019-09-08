heating:
	gcc energenie.c hw.c radio.c salus.c main.c -o heating -lbcm2835 -lpthread

salustest:
	gcc energenie.c hw.c radio.c salus.c salustest.c -o salustest -lbcm2835 -lpthread
	gcc energenie.c hw.c radio.c salus.c salusmon.c -o salusmon -lbcm2835 -lpthread

clean:
	rm -f heating salustest salusmon *~
