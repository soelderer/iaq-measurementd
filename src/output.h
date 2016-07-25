/* ----------------------------------------------------------------------- *
 *
 *   Copyright (C) 2016, Simon Adam, Markus Dullnig, Paul Soelder
 *   All rights reserved.
 *
 *   This file is part of the indoor air quality measurement daemon,
 *   and is made available under the terms of the BSD 3-Clause Licence.
 *   A full copy of the licence can be found in the COPYING file.
 *
 * ----------------------------------------------------------------------- */

/*
 * src/output.h
 *
 * Header file for the output code
 */

#ifndef _IAQ_MEASUREMENTD_OUTPUT_H_
#define _IAQ_MEASUREMENTD_OUTPUT_H_

void inipin();
int LEDsystem(int co2, float temp, float rh, int led_state);
void http_log(int co2, float temp, float rh, int led_state);
void write_state_files();

#endif
