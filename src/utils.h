#pragma once

char* floatStr(char *out, float num, int decimals);
float mySqrt(const float x);
void center_text(GContext *ctx, const char *text, GFont font, GRect frame);
void center_text_point(GContext *ctx, const char *text, GFont font, GPoint p);

void dashed_line_h(GContext *ctx, GPoint p, int width, int l1, int l2);
void dashed_line_v(GContext *ctx, GPoint p, int height, int l1, int l2);
void draw_line(GContext *ctx, GPoint start, GPoint end, int l1, int l2);
