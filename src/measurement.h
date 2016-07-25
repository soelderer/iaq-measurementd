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
 * src/measurement.h
 *
 * Header file for the measurement code
 */

#ifndef _IAQ_MEASUREMENTD_MEASUREMENT_H_
#define _IAQ_MEASUREMENTD_MEASUREMENT_H_

int CO2(int *co2);
int si7021(float *temp, float *rh);

uint8_t crc8(const void *vptr, int len);

extern int i2c_fd;

#endif
