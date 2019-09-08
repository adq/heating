heating:
	gcc energenie.c hw.c radio.c main.c -o heating -lbcm2835 -lpthread

salustest:
	gcc energenie.c hw.c radio.c salustest.c -o salustest -lbcm2835 -lpthread
	gcc energenie.c hw.c radio.c salusmonc -o salusmon -lbcm2835 -lpthread


clean:
	rm -f heating salustest salusmon *~
