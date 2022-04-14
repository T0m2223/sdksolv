all: sdksolv.c
	cc -O3 -o sdksolv sdksolv.c -lm -pthread
