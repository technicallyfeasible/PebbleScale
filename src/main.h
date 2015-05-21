#pragma once
#include<pebble.h>

extern GFont font_large;
extern GFont font_medium;
extern GFont font_tiny;
extern GFont font_symbols;

extern const char *icon_plus;
extern const char *icon_minus;
extern const char *icon_clock;
extern const char *icon_stop;
extern const char *icon_settings;

static char* floatStr(char *out, float num, int decimals) {
	int offset = 0;
	if (num < 0) {
		out[offset++] = '-';
		num = -num;
	}
	float size = 0;
	while (num >= 10) {
		num /= 10;
		size++;
	};
	for (; size >= 0; size--) {
		int f = (int) num;
		out[offset++] = f + '0';
		num -= f;
		num *= 10;
	}
	if (decimals > 0)
		out[offset++] = '.';
	for (; decimals > 0; decimals--) {
		int f = (int) num;
		out[offset++] = f + '0';
		num -= f;
		num *= 10;
	}
	out[offset] = 0;
	return out;
}

static inline float mySqrt(const float x) {
	const float xhalf = 0.5f*x;
	union { float x; int i; 	} u;
	u.x = x;
	u.i = 0x5f3759df - (u.i >> 1);
	return x*u.x*(1.5f - xhalf*u.x*u.x);
}
