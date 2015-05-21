#pragma once
#include "main.h"
#include "measure.h"

#define MAX_CALIBRATIONS	16

extern Measurement calibrations[MAX_CALIBRATIONS];
extern int16_t calibrations_count;

void calibrate_page_open();
void calibrate_page_close();

void calibrations_save();
void calibrations_load();
