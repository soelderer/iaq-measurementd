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
 * src/measurement.c
 *
 * Measurement module for K-30 and Si7021 sensors
 */

#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <inttypes.h>
#include <syslog.h>

#include "measurement.h"
#include "iaq-measurementd.h"

int i2c_fd;
struct timespec error_delay = {0L, ERROR_DELAY};

// changes the I2C slave address
int openI2c(int address) {
	// tell the i2c driver the slave address
	if(ioctl(i2c_fd, I2C_SLAVE, address) < 0) {
		syslog(LOG_ERR, "failed to ioctl i2c device file %s: %m ", i2c_device);
		if(i2c_fd != -1)
			close(i2c_fd);
		return 1;
	}
	return 0;
}

// k-30 measurement function (co2-sensor)
int CO2(int *co2) {
	uint8_t checksum;
	int status_write, status_read;
	// buffer for request
	uint8_t *buffer_write;
	// buffer for response
	uint8_t *buffer_read;
	uint8_t write_error_cnt = 0;
	uint8_t read_error_cnt = 0;
	uint8_t checksum_error_cnt = 0;
	// 1ms delay between wake-up pulses and actual communication according to
	// datasheet
	struct timespec t_WUD = {0L, 1000000L};
	// 20ms delay between request and response according to datasheet
	struct timespec t_WAIT = {0L, 20000000L};

	struct timespec error_co2_delay = {ERROR_CO2_DELAY, 0L};

	int i;
	uint8_t success = 0;

	buffer_write = malloc(4);

	// command sequence according to datasheet
	// 0x22: read 2 bytes
	*buffer_write = 0x22;
	// 0x0008: RAM-address (co2-value)
	*(buffer_write+1) = 0x00;
	*(buffer_write+2) = 0x08;
	// 0x2A: checksum
	*(buffer_write+3) = 0x2A;

	buffer_read = malloc(4);

	// according to the datasheet (I²C communication guide), the k-30
	// sensor will not acknowledge written bytes if it is performing
	// measurements, which is not an error.
	// so just send (and receive) over and over again, until all bytes
	// are written/read or the MAX_ERROR_CNT is reached.
	// k-30 sensor response frame (<> is one byte):
	// <status> <co2-high-byte> <co2-low-byte> <checksum>
	do {
		// 0x00 wake-up pulse for k-30 followed by 1ms delay (see datasheet)
		// this will probably result in an error because there is no device
		// with address 0x00, but we do not care
		if(openI2c(0x00))
			return 1;

		write(i2c_fd, buffer_write, 1);

		nanosleep(&t_WUD, NULL);

		// address of k-30 is 0x68 according to datasheet
		if(openI2c(0x68))
			return 1;

		do {
			do {
				status_write = write(i2c_fd, buffer_write, 4);

				if(status_write != 4) {
					write_error_cnt++;
					// the k-30 co2 sensor might not respond during its
					// measurements, so retry after one larger delay
					if(checksum_error_cnt++ == MAX_ERROR_CNT/2)
						nanosleep(&error_co2_delay, NULL);
					else
						nanosleep(&error_delay, NULL);
				}

			} while(status_write != 4 && write_error_cnt < MAX_ERROR_CNT);

			if(status_write != 4)
				return 2;

			nanosleep(&t_WAIT, NULL);

			status_read = read(i2c_fd, buffer_read, 4);

			if(status_read != 4) {
				read_error_cnt++;
				nanosleep(&error_delay, NULL);
			}

		} while(status_read != 4 && read_error_cnt < MAX_ERROR_CNT);

		if(status_read != 4)
			return 2;

		// according to datasheet, checksum is the sum of all bytes except
		// i2c-address byte and checksum
		checksum = 0x00;
		for(i = 0; i < 3; i++)
			checksum += *(buffer_read+i);

		// checksum correct and status is 'complete' (0x21)
		if(checksum == *(buffer_read+3) && *buffer_read == 0x21) {
			*co2 = (*(buffer_read+1) << 8) + *(buffer_read+2);
			success = 1;
		}
		else {
			checksum_error_cnt++;
			nanosleep(&error_delay, NULL);
		}

	} while(!success && checksum_error_cnt < MAX_ERROR_CNT);

	if(!success)
		return 2;

	return 0;
}

// si7021 measurement function (temperature and relative humidity sensor)
int si7021(float *temp, float *rh) {
	int status_write, status_read;
	// buffer for request
	uint8_t *buffer_write;
	// buffer for response
	uint8_t *buffer_read;
	uint16_t r[2];
	uint8_t crc;
	uint8_t write_error_cnt = 0;
	uint8_t read_error_cnt = 0;
	uint8_t checksum_error_cnt = 0;
	uint16_t rh_bytes, temp_bytes;
	float rh_local = 0;
	int i;
	uint8_t success = 0;

	// address of si7021 on i2c-bus is 0x20
	if(openI2c(0x40))
		return 1;

	buffer_write = malloc(1);

	// according to datasheet:
	// 0xE5: measure relative humidity with hold master mode (clock stretching
	// is used)
	*buffer_write = 0xE5;

	// response is 3 bytes (<> is one byte):
	// <rh-high-byte> <rh-low-byte> <crc-8>
	buffer_read = malloc(3);

	do {
		do {
			status_write = write(i2c_fd, buffer_write, 1);

			if(status_write != 1) {
				write_error_cnt++;
				nanosleep(&error_delay, NULL);
			}

		} while(status_write != 1 && write_error_cnt < MAX_ERROR_CNT);

		if(status_write != 1)
			return 2;

		// due to clock stretching, clock is low during measurement. read()
		// will return -1, so do it multiple times
		do {
			status_read = read(i2c_fd, buffer_read, 3);

			if(status_read != 3) {
				read_error_cnt++;
				nanosleep(&error_delay, NULL);
			}

		} while(status_read != 3 && read_error_cnt < MAX_ERROR_CNT);

		if(status_read != 3)
			return 2;

		crc = crc8(buffer_read, 2);

		// checksum correct
		if(crc == *(buffer_read+2)) {
			// calculate rh according to datasheet
			rh_bytes = (*(buffer_read) << 8) + *(buffer_read+1);
			rh_local = ((125 * rh_bytes) / 65536) - 6;
			// according to datasheet, rh value may be slightly negative or
			// slightly bigger than 100% which is no error and shall be rounded
			if(rh_local < 0)
				*rh = 0;
			else if(rh_local > 100)
				*rh = 100;
			else
				*rh = rh_local;
			success = 1;
		}

		else {
			checksum_error_cnt++;
			nanosleep(&error_delay, NULL);
		}

	} while(!success && checksum_error_cnt < MAX_ERROR_CNT);

	if(!success)
		return 2;

	// read temperature from previous rh measurement
	else {
		// according to datasheet:
		// 0xE0: return temperature from previous rh measurement
		// response is 2 bytes (<> is one byte):
		// <temp-high-byte> <temp-low-byte>
		*buffer_write = 0xE0;

		for(i = 0; i < 3; i++)
			*(buffer_read+i) = 0;

		write_error_cnt = 0;
		read_error_cnt = 0;
		checksum_error_cnt = 0;
		do {
			status_write = write(i2c_fd, buffer_write, 1);

			if(status_write != 1) {
				write_error_cnt++;
				nanosleep(&error_delay, NULL);
			}

		} while(status_write != 1 && write_error_cnt < MAX_ERROR_CNT);

		if(status_write != 1)
			return 2;

		// due to clock stretching, clock is low during measurement.
		// read() will return -1, so do it multiple times
		// shouldn't occur here as 0xE0 does not imply a measurement
		do {
			status_read = read(i2c_fd, buffer_read, 2);

			if(status_read != 2) {
				read_error_cnt++;
				nanosleep(&error_delay, NULL);
			}

		} while(status_read != 2 && read_error_cnt < MAX_ERROR_CNT);

		if(status_read != 2)
			return 2;

		temp_bytes = (*(buffer_read) << 8) + *(buffer_read+1);
		*temp = ((175.72 * temp_bytes) / 65536) - 46.85;

		return 0;
	}
}

/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the licenses/CHROMIUM_LICENSE file.
 */

/**
* Return CRC-8 of the data, using x^8 + x^5 + x^4 + 1 polynomial.  A table-based
* algorithm would be faster, but for only a few bytes it isn't worth the code
* size. */

// note: MSB first with multibyte-data
uint8_t crc8(const void *vptr, int len) {
	const uint8_t *data = vptr;
	unsigned crc = 0;
	int i, j;
	for (j = len; j; j--, data++) {
		crc ^= (*data << 8);
		for(i = 8; i; i--) {
			if (crc & 0x8000)
				crc ^= (0x1310<< 3);
			crc <<= 1;
		}
	}
	return (uint8_t)(crc >> 8);
}
