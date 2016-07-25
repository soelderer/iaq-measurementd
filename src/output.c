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
 * src/output.c
 *
 * Output module (GPIO, HTTP, state files)
 */

#include <wiringPi.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <time.h>

#include "iaq-measurementd.h"
#include "output.h"

// LED-pin setup
void inipin() {
	if(wiringPiSetup() == -1) {
		syslog(LOG_ERR, "failed to setup wiringPi: %m. terminating");
		terminate(EXIT_FAILURE);
	}

	pinMode(green_pin, OUTPUT);
	pinMode(yellow_pin, OUTPUT);
	pinMode(red_pin, OUTPUT);

	digitalWrite(green_pin, LOW);
	digitalWrite(yellow_pin, LOW);
	digitalWrite(red_pin, LOW);
}

// controls the LEDs based on co2, temp, rh and old led_state
// returns the new LED state
int LEDsystem(int co2, float temp, float rh, int led_state) {
	if((led_state == RED && co2 > (co2_threshold_red - co2_hysteresis)) ||
			(led_state == YELLOW && co2 > co2_threshold_red) ||
			(led_state == OFF && co2 > co2_threshold_red)) {

		// avoid unnecessary calls
		if(led_state != RED) {
			digitalWrite(green_pin, LOW);
			digitalWrite(yellow_pin, LOW);
			digitalWrite(red_pin, HIGH);
		}

		return RED;

	} else if((led_state == YELLOW && co2 < (co2_threshold_yellow -
			co2_hysteresis)) ||
			(led_state == GREEN && co2 < co2_threshold_yellow) ||
			(led_state == OFF && co2 < co2_threshold_yellow)) {

		if(led_state != GREEN) {
			digitalWrite(green_pin, HIGH);
			digitalWrite(yellow_pin, LOW);
			digitalWrite(red_pin, LOW);
		}

		return GREEN;

	} else {

		if(led_state != YELLOW) {
			digitalWrite(green_pin, LOW);
			digitalWrite(yellow_pin, HIGH);
			digitalWrite(red_pin, LOW);
		}

		return YELLOW;

	}
}

void http_log(int co2, float temp, float rh, int led_state) {
	CURL *curl;
	CURLcode res;
	char *url, *room_escaped;
	size_t url_len;
	int status;

	curl = curl_easy_init();

	if(curl == NULL) {
		syslog(LOG_ERR, "failed to curl_easy_init(). terminating");
		terminate(EXIT_FAILURE);
	}

	room_escaped = curl_easy_escape(curl, room, 0);

	if(room_escaped == NULL) {
		syslog(LOG_ERR, "failed to curl_easy_escape(). terminating");
		terminate(EXIT_FAILURE);
	}

	// length of host + length of room_escaped + 200 extra bytes (data are 80
	// + length of http:// and device_interface.php + buffer)
	url_len = strlen(host) + strlen(room_escaped) + 300;
	url = malloc(url_len);

	status = snprintf(url, url_len, "http://%s/device_interface.php?action=log"
			"&room=%s&co2=%d&temp=%.2f&rh=%.2f&led_state=%d", host,
			room_escaped, co2, temp, rh, led_state);

	if(status < 0) {
		syslog(LOG_ERR, "failed to snprintf the logging-server url. terminating"
			);
		terminate(EXIT_FAILURE);
	}


	else {
		curl_easy_setopt(curl, CURLOPT_URL, url);

		if(res = curl_easy_perform(curl) != CURLE_OK)
			syslog(LOG_WARNING, "could not send measurement data to the logging"
					"-server. %s%.", curl_easy_strerror(res));

		curl_easy_cleanup(curl);

		free(url);
		curl_free(room_escaped);
	}
}

void write_state_files() {
	FILE *co2_file, *temp_file, *rh_file, *led_state_file;
	FILE *co2_threshold_yellow_file, *co2_threshold_red_file;
	FILE *co2_hysteresis_file;

	struct stat st;

	if(stat(PKGSTATEDIR, &st) == -1) {
		if(mkdir(PKGSTATEDIR, 0755) == -1)
			syslog(LOG_ERR, "failed to create directory " PKGSTATEDIR
					". terminating");
			terminate(EXIT_FAILURE);
	}

	co2_file = fopen(PKGSTATEDIR "/co2", "w");

	if(co2_file == NULL) {
		syslog(LOG_ERR, "failed to open file " PKGSTATEDIR "/co2."
				" %m. terminating");
		terminate(EXIT_FAILURE);
	}

	else {
		if(fprintf(co2_file, "%d\n", co2) < 0) {
			syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR "/co2."
					" %m. terminating");
			fclose(co2_file);
			terminate(EXIT_FAILURE);
			}
		fclose(co2_file);
	}

	temp_file = fopen(PKGSTATEDIR "/temp", "w");

	if(temp_file == NULL) {
		syslog(LOG_ERR, "failed to open file " PKGSTATEDIR "/temp."
				" %m. terminating");
		terminate(EXIT_FAILURE);
	}

	else {
		if(fprintf(temp_file, "%.2f\n", temp) < 0) {
			syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR "/temp."
					" %m");
			fclose(temp_file);
			terminate(EXIT_FAILURE);
		}
		fclose(temp_file);
	}

	rh_file = fopen(PKGSTATEDIR "/rh", "w");

	if(rh_file == NULL) {
		syslog(LOG_ERR, "failed to open file " PKGSTATEDIR "/rh."
				" %m. terminating");
		terminate(EXIT_FAILURE);
	}

	else {
		if(fprintf(rh_file, "%.2f\n", rh) < 0) {
			syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR "/rh."
					" %m");
			fclose(rh_file);
			terminate(EXIT_FAILURE);
		}
		fclose(rh_file);
	}

	led_state_file = fopen(PKGSTATEDIR "/led_state", "w");

	if(led_state_file == NULL) {
		syslog(LOG_ERR, "failed to open file " PKGSTATEDIR "/led_state."
				" %m. terminating");
		terminate(EXIT_FAILURE);
	}

	else {
		if(fprintf(led_state_file, "%d\n", led_state) < 0) {
			syslog(LOG_ERR, "failed to write to file " PKGSTATEDIR "/led_state."
					" %m");
			fclose(led_state_file);
			terminate(EXIT_FAILURE);
		}
		fclose(led_state_file);
	}
}
