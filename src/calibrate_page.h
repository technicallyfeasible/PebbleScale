#pragma once
#include "main.h"
#include "measure.h"

#define MAX_CALIBRATIONS	30

extern Measurement calibrations[MAX_CALIBRATIONS];

void calibrate_page_open();
void calibrate_page_close();
