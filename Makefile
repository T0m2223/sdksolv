all: sdksolv.c
	gcc -O3 -o sdksolv sdksolv.c -lm -pthread
