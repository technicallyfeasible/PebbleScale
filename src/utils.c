#include <pebble.h>
#include "utils.h"

char* floatStr(char *out, float num, int decimals) {
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


inline float mySqrt(const float x) {
	const float xhalf = 0.5f*x;
	union { float x; int i; 	} u;
	u.x = x;
	u.i = 0x5f3759df - (u.i >> 1);
	return x*u.x*(1.5f - xhalf*u.x*u.x);
}


inline void center_text(GContext *ctx, const char *text, GFont font, GRect frame) {
	GSize size = graphics_text_layout_get_content_size(text, font, frame, GTextOverflowModeWordWrap, GTextAlignmentCenter);
	graphics_draw_text(ctx, text, font, GRect(frame.origin.x, frame.origin.y + (frame.size.h - size.h) / 2, frame.size.w, size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}
inline void center_text_point(GContext *ctx, const char *text, GFont font, GPoint p) {
	GSize size = graphics_text_layout_get_content_size(text, font, GRect(0, 0, 144, 164), GTextOverflowModeWordWrap, GTextAlignmentCenter);
	graphics_draw_text(ctx, text, font, GRect(p.x - size.w / 2, p.y - size.h / 2, size.w, size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

inline int16_t sgn(int16_t val) {
    return (0 < val) - (val < 0);
}

void dashed_line_h(GContext *ctx, GPoint p, int width, int l1, int l2) {
	if (width < 0) {
		p.x += width;
		width = -width;
	}
	int x = p.x + width;
	while (p.x < x) {
		for (int i = 0; i < l1 && p.x < x; i++, p.x++)
			graphics_draw_pixel(ctx, p);
		p.x += l2;
	}
}

void dashed_line_v(GContext *ctx, GPoint p, int height, int l1, int l2) {
	if (height < 0) {
		p.y += height;
		height = -height;
	}
	int y = p.y + height;
	while (p.y < y) {
		for (int i = 0; i < l1 && p.y < y; i++, p.y++)
			graphics_draw_pixel(ctx, p);
		p.y += l2;
	}
}


void draw_line(GContext *ctx, GPoint start, GPoint end, int l1, int l2) {
	if (start.y == end.y)
		dashed_line_h(ctx, start, end.x - start.x + 1, l1, l2);
	else if (start.x == end.x)
		dashed_line_h(ctx, start, end.y - start.y + 1, l1, l2);
	else {
		// swap coordinates until we are in the correct quadrant
		int16_t t;
		if (end.x < start.x) {
			t = end.x; end.x = start.x; start.x = t;
			t = end.y; end.y = start.y; start.y = t;
		}
		int16_t sy = 1;
		if (end.y < start.y) {
			sy = -1;
		}

		GPoint dp, yAdd, xAdd;
		int16_t dx = end.x - start.x;
		int16_t dy = sy * (end.y - start.y);
		int16_t ex = end.x;
		if (dy > dx) {
			t = dy; dy = dx; dx = t;
			yAdd = GPoint(1, 0);
			xAdd = GPoint(0, sy);
			dp = GPoint(sy * (start.y + 1), start.x);
			ex = sy * end.y;
		} else {
			yAdd = GPoint(0, sy);
			xAdd = GPoint(1, 0);
			dp = GPoint(start.x + 1, start.y);
		}

		int16_t d = (dy << 1) - dx;
		int16_t l = l1;
		if (l > 0)
			graphics_draw_pixel(ctx, start);

		GPoint p = GPoint(start.x + xAdd.x, start.y + xAdd.y);
		while (dp.x <= ex) {
			if (d > 0) {
				p.x += yAdd.x;
				p.y += yAdd.y;
				d += (dy << 1) - (dx << 1);
			} else {
				d += (dy << 1);
			}
			l--;
			if (l <= -l2)
				l = l1;
			if (l > 0)
				graphics_draw_pixel(ctx, p);
			p.x += xAdd.x;
			p.y += xAdd.y;
			dp.x++;
		}
	}
}
