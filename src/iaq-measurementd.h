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
 * src/iaq-measurementd.h
 *
 * Header file for iaq-measurementd
 */

#ifndef _IAQ_MEASUREMENTD_H_
#define _IAQ_MEASUREMENTD_H_

#include "config.h"
#include <libconfig.h>

#define PIDFILE RUNSTATEDIR "/" PACKAGE_NAME ".pid"

#define DEFAULT_I2C_DEVICE "/dev/i2c-1"

#define DEFAULT_HOST "localhost"

// time between measurements in seconds
#define MEASUREMENT_INTERVAL 10L
// time between log entries in minutes
#define DEFAULT_LOGGING_INTERVAL 5L

#define DEFAULT_GREEN_PIN 0
#define DEFAULT_YELLOW_PIN 1
#define DEFAULT_RED_PIN 2

// in ppm (parts per million)
#define DEFAULT_CO2_THRESHOLD_YELLOW 1000
#define DEFAULT_CO2_THRESHOLD_RED 1900
#define DEFAULT_CO2_HYSTERESIS 200

// in degree celcius (°C)
#define DEFAULT_TEMP_THRESHOLD_YELLOW 28
#define DEFAULT_TEMP_THRESHOLD_RED 32

// in percent (%)
#define DEFAULT_RH_THRESHOLD_YELLOW 80
#define DEFAULT_RH_THRESHOLD_RED 100

// wiringPi pin numbering goes from 0 to 20
#define WIRING_PI_MIN 0
#define WIRING_PI_MAX 20

// minimum logging_interval in minutes
#define LOGGING_INTERVAL_MIN 1

#define XSTR(S) STR(S)
#define STR(S) #S

// delay between measurements in case of measurement error)
#define ERROR_DELAY 10000000L // in ns
// the k-30 co2 sensor might not respond during measurement, so retry after a
// bigger delay
#define ERROR_CO2_DELAY 4L // in s
// maximum number of tries for measurements
#define MAX_ERROR_CNT 50

// intern constants. do not change
#define OFF 0
#define GREEN 1
#define YELLOW 2
#define RED 3

// read from CONFFILE
extern const char *i2c_device;
// time between log entries in seconds
extern time_t logging_interval_sec;
// wiringPi pins for LEDs
extern int green_pin, yellow_pin, red_pin;
// threshold values for yellow and red LED
extern int co2_threshold_yellow, co2_threshold_red; // in ppm
extern int co2_hysteresis; // in ppm
// in degree celcius
extern float temp_threshold_yellow, temp_threshold_red;
extern float rh_threshold_yellow, rh_threshold_red; // in percent

// (optional) room number
extern const char *room;

// hostname or IP-address of the iaq-server
extern const char *host;

// measurement results
extern int co2;
extern float temp;
extern float rh;

// state of the LEDs
extern int led_state;

void daemonize();
static inline int finit_module(int fd, const char *uargs, int flags);
void load_kernel_modules();
void parse_config();
void terminate();
void *http_logger();

#endif
