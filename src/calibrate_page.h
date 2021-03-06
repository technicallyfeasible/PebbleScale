#pragma once
#include "main.h"
#include "measure.h"

#define MAX_CALIBRATIONS	8

extern int16_t calibrations_count;
extern float beta[3];

void calibrate_page_open();
void calibrate_page_close();

void calibrations_save();
void calibrations_load();
