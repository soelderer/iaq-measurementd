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
 * src/config-parser.c
 *
 * Parses the config file
 */

#include <stdio.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdlib.h>
#include <libconfig.h>
#include <errno.h>
#include <string.h>

#include "config-parser.h"
#include "config.h"
#include "iaq-measurementd.h"

void parse_config() {
	config_t cfg;
	config_setting_t *setting;
	int logging_interval_min;
	const char *i2c_device_local;
	const char *room_local;
	const char *host_local;
	double double_helper; // libconfig uses double instead of float
	struct stat st;

	config_init(&cfg);

	if(config_read_file(&cfg, CONFFILE) == CONFIG_FALSE) {

		if(config_error_type(&cfg) == CONFIG_ERR_FILE_IO) {
			syslog(LOG_INFO, "config-file " CONFFILE " not found."
					" terminating");

			terminate(EXIT_FAILURE);
		}

		else { // parsing error
			syslog(LOG_INFO, "%s:%d - %s. terminating.",
					config_error_file(&cfg), config_error_line(&cfg),
					config_error_text(&cfg));

			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}
	}

	else {
		syslog(LOG_INFO, "reading config-file " CONFFILE);

/* ******************************* ic2_device ******************************* */
		if(config_lookup_string(&cfg, "i2c_device", &i2c_device_local)
				== CONFIG_FALSE)

			syslog(LOG_INFO, "i2c_device: either not set or wrong format. using"
					" default value");

		else
			if((i2c_device = strdup(i2c_device_local)) == NULL) {
				syslog(LOG_ERR, "failed to strdup i2c_device: %m. terminating");
				terminate(EXIT_FAILURE);
			}

/* ************************ logging_interval_min *************************** */
		if(config_lookup_int(&cfg, "logging_interval", &logging_interval_min)
				== CONFIG_FALSE)

			syslog(LOG_INFO, "logging_interval: either not set or wrong "
					"format. using default value");

		else if(logging_interval_min < LOGGING_INTERVAL_MIN) {

			syslog(LOG_INFO, "logging_interval: out of range."
					" using default value");

			logging_interval_sec = DEFAULT_LOGGING_INTERVAL * 60;
		}

		else
			logging_interval_sec = logging_interval_min * 60;

/* ******************************* green_pin ******************************** */
		if(config_lookup_int(&cfg, "green_pin", &green_pin) == CONFIG_FALSE)

			syslog(LOG_INFO, "green_pin: either not set or wrong format. "
					"using default value");

		else if(green_pin < WIRING_PI_MIN || green_pin > WIRING_PI_MAX) {

			syslog(LOG_INFO, "green_pin: out of range. using default value");

			green_pin = DEFAULT_GREEN_PIN;
		}

/* ****************************** yellow_pin ******************************** */
		if(config_lookup_int(&cfg, "yellow_pin", &yellow_pin) == CONFIG_FALSE)

			syslog(LOG_INFO, "yellow_pin: either not set or wrong format. "
					"using default value");

		else if(yellow_pin < WIRING_PI_MIN || yellow_pin > WIRING_PI_MAX) {

			syslog(LOG_INFO, "yellow_pin: out of range (" XSTR(WIRING_PI_MIN)
					".." XSTR(WIRING_PI_MAX) "). using default value");

			yellow_pin = DEFAULT_YELLOW_PIN;
		}

/* ******************************** red_pin ********************************* */
		if(config_lookup_int(&cfg, "red_pin", &red_pin) == CONFIG_FALSE)

			syslog(LOG_INFO, "red_pin: either not set or wrong format. "
					"using default value");

		else if(red_pin < WIRING_PI_MIN || red_pin > WIRING_PI_MAX) {

			syslog(LOG_INFO, "red_pin: out of range (" XSTR(WIRING_PI_MIN) ".."
					XSTR(WIRING_PI_MAX) "). using default value");

			red_pin = DEFAULT_RED_PIN;
		}

/* ************************** co2_threshold_yellow ************************** */
		if(config_lookup_int(&cfg, "co2_threshold_yellow",
				&co2_threshold_yellow) == CONFIG_FALSE)

			syslog(LOG_INFO, "co2_threshold_yellow: either not set or wrong "
					"format. using default value");

		else if(co2_threshold_yellow < 0) {

			syslog(LOG_INFO, "co2_threshold_yellow: cannot be negative. using "
					"default value");

			co2_threshold_yellow = DEFAULT_CO2_THRESHOLD_YELLOW;
		}

		// PKGSTATEDIR exists? if not, try to create it
		if(stat(PKGSTATEDIR, &st) == -1) {
			if(mkdir(PKGSTATEDIR, 0755) == -1) {
				syslog(LOG_ERR, "failed to create directory " PKGSTATEDIR
						". %m. terminating");
				config_destroy(&cfg);
				terminate(EXIT_FAILURE);
			}
		}

		FILE *co2_threshold_file = fopen(PKGSTATEDIR "/co2_threshold_yellow",
				"w");

		if(co2_threshold_file == NULL) {
			syslog(LOG_ERR, "failed to open file " PKGSTATEDIR
					"/co2_threshold_yellow. %m. terminating");
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}

		else {
			if(fprintf(co2_threshold_file, "%d\n", co2_threshold_yellow) < 0) {
				syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR
						"/co2_threshold_yellow. %m. terminating");
				fclose(co2_threshold_file);
				config_destroy(&cfg);
				exit(EXIT_FAILURE);
			}
			fclose(co2_threshold_file);
		}

/* ************************** co2_threshold_red ***************************** */
		if(config_lookup_int(&cfg, "co2_threshold_red", &co2_threshold_red) ==
				CONFIG_FALSE)

			syslog(LOG_INFO, "co2_threshold_red: either not set or wrong "
					"format. using default value");

		else if(co2_threshold_red < 0) {

			syslog(LOG_INFO, "co2_threshold_red: cannot be negative. using "
					"default value");

			co2_threshold_red = DEFAULT_CO2_THRESHOLD_RED;
		}

		co2_threshold_file = fopen(PKGSTATEDIR "/co2_threshold_red",
				"w");

		if(co2_threshold_file == NULL) {
			syslog(LOG_ERR, "failed to open file " PKGSTATEDIR
					"/co2_threshold_red. %m. terminating");
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}

		else {
			if(fprintf(co2_threshold_file, "%d\n", co2_threshold_red) < 0) {
				syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR
						"/co2_threshold_red. %m. terminating");
				fclose(co2_threshold_file);
				config_destroy(&cfg);
				exit(EXIT_FAILURE);
			}
			fclose(co2_threshold_file);
		}

/* ****************************** co2_hysteresis **************************** */
		if(config_lookup_int(&cfg, "co2_hysteresis", &co2_hysteresis) ==
				CONFIG_FALSE)

			syslog(LOG_INFO, "co2_hysteresis: either not set or wrong format. "
					"using default value");

		else if(co2_hysteresis < 0) {

			syslog(LOG_INFO, "co2_hysteresis: cannot be negative. using "
					"default value");

		}

		FILE *co2_hysteresis_file = fopen(PKGSTATEDIR "/co2_hysteresis",
				"w");

		if(co2_hysteresis_file == NULL) {
			syslog(LOG_ERR, "failed to open file " PKGSTATEDIR
					"/co2_hysteresis. %m. terminating");
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}

		else {
			if(fprintf(co2_hysteresis_file, "%d\n", co2_hysteresis) < 0) {
				syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR
						"/co2_hysteresis. %m. terminating");
				fclose(co2_hysteresis_file);
				config_destroy(&cfg);
				exit(EXIT_FAILURE);
			}
			fclose(co2_hysteresis_file);
		}

/* ************************** temp_threshold_yellow ************************* */
		if(config_lookup_float(&cfg, "temp_threshold_yellow",
				&double_helper) == CONFIG_FALSE)

			syslog(LOG_INFO, "temp_threshold_yellow: either not set or wrong "
					"format. using default value");

		else
			temp_threshold_yellow = (float) double_helper;

		FILE *temp_threshold_file = fopen(PKGSTATEDIR "/temp_threshold_yellow",
				"w");

		if(temp_threshold_file == NULL) {
			syslog(LOG_ERR, "failed to open file " PKGSTATEDIR
					"/temp_threshold_yellow. %m. terminating");
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}

		else {
			if(fprintf(temp_threshold_file, "%f\n", temp_threshold_yellow)
					< 0) {
				syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR
						"/temp_threshold_yellow. %m. terminating");
				fclose(temp_threshold_file);
				config_destroy(&cfg);
				exit(EXIT_FAILURE);
			}
			fclose(temp_threshold_file);
		}


/* *************************** temp_threshold_red *************************** */
		if(config_lookup_float(&cfg, "temp_threshold_red",
				&double_helper) == CONFIG_FALSE)

			syslog(LOG_INFO, "temp_threshold_red: either not set or wrong "
					"format. using default value");

		else if(double_helper < 0) {

			syslog(LOG_INFO, "temp_threshold_red: cannot be negative. using "
					"default value");

			temp_threshold_red = DEFAULT_TEMP_THRESHOLD_RED;
		}

		else
			temp_threshold_red = (float) double_helper;

		temp_threshold_file = fopen(PKGSTATEDIR "/temp_threshold_red",
				"w");

		if(temp_threshold_file == NULL) {
			syslog(LOG_ERR, "failed to open file " PKGSTATEDIR
					"/temp_threshold_red. %m. terminating");
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}

		else {
			if(fprintf(temp_threshold_file, "%f\n", temp_threshold_red) < 0) {
				syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR
						"/temp_threshold_red. %m. terminating");
				fclose(temp_threshold_file);
				config_destroy(&cfg);
				exit(EXIT_FAILURE);
			}
			fclose(temp_threshold_file);
		}

/* *************************** rh_threshold_yellow ************************** */
		if(config_lookup_float(&cfg, "rh_threshold_yellow",
				&double_helper) == CONFIG_FALSE)

			syslog(LOG_INFO, "rh_threshold_yellow: either not set or wrong "
					"format. using default value");

		else if(double_helper < 0) {

			syslog(LOG_INFO, "rh_threshold_yellow: cannot be negative. using "
					"default value");

			rh_threshold_yellow = DEFAULT_RH_THRESHOLD_YELLOW;
		}

		else
			rh_threshold_yellow = (float) double_helper;

		FILE *rh_threshold_file = fopen(PKGSTATEDIR "/rh_threshold_yellow",
				"w");

		if(rh_threshold_file == NULL) {
			syslog(LOG_ERR, "failed to open file " PKGSTATEDIR
					"/rh_threshold_yellow. %m. terminating");
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}

		else {
			if(fprintf(rh_threshold_file, "%f\n", rh_threshold_yellow) < 0) {
				syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR
						"/rh_threshold_yellow. %m. terminating");
				fclose(rh_threshold_file);
				config_destroy(&cfg);
				exit(EXIT_FAILURE);
			}
			fclose(rh_threshold_file);
		}

/* **************************** rh_threshold_red **************************** */
		if(config_lookup_float(&cfg, "rh_threshold_red",
				&double_helper) == CONFIG_FALSE)

			syslog(LOG_INFO, "rh_threshold_red: either not set or wrong "
					"format. using default value");

		else if(rh_threshold_red < 0) {

			syslog(LOG_INFO, "rh_threshold_red: cannot be negative. using "
					"default value");

			rh_threshold_red = DEFAULT_RH_THRESHOLD_RED;
		}

		else
			rh_threshold_red = (float) double_helper;

		rh_threshold_file = fopen(PKGSTATEDIR "/rh_threshold_red",
				"w");

		if(rh_threshold_file == NULL) {
			syslog(LOG_ERR, "failed to open file " PKGSTATEDIR
					"/rh_threshold_red. %m. terminating");
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}

		else {
			if(fprintf(rh_threshold_file, "%f\n", rh_threshold_red) < 0) {
				syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR
						"/rh_threshold_red. %m. terminating");
				fclose(rh_threshold_file);
				config_destroy(&cfg);
				exit(EXIT_FAILURE);
			}
			fclose(rh_threshold_file);
		}

/* *********************************** room ********************************* */
		if(config_lookup_string(&cfg, "room", &room_local) == CONFIG_FALSE) {

			syslog(LOG_ERR, "room: either not set or wrong format. terminating"
					);
			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}
		else
			if((room = strdup(room_local)) == NULL) {
				syslog(LOG_ERR, "failed to strdup room: %m. terminating");
				terminate(EXIT_FAILURE);
			}
/* ********************************** host ********************************** */
		if(config_lookup_string(&cfg, "host", &host_local) == CONFIG_FALSE) {

			syslog(LOG_INFO, "host: not set or wrong format. terminating.");

			config_destroy(&cfg);
			terminate(EXIT_FAILURE);
		}
		else
			if((host = strdup(host_local)) == NULL) {
				syslog(LOG_ERR, "failed to strdup host: %m. terminating");
				terminate(EXIT_FAILURE);
			}
	}
	config_destroy(&cfg);
}
