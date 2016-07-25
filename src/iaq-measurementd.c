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
 * src/iaq-measurementd.c
 *
 * Main file of iaq-measurementd
 */

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <wiringPi.h>
#include <curl/curl.h>

#include "config.h"
#include "config-parser.h"
#include "measurement.h"
#include "output.h"

#include "iaq-measurementd.h"

pid_t pid, sid;
struct sigaction sa;

const char *i2c_device = DEFAULT_I2C_DEVICE;

// in seconds for use with struct timespec
time_t logging_interval_sec = DEFAULT_LOGGING_INTERVAL * 60;

int green_pin = DEFAULT_GREEN_PIN;
int yellow_pin = DEFAULT_YELLOW_PIN;
int red_pin = DEFAULT_RED_PIN;

int co2_threshold_yellow = DEFAULT_CO2_THRESHOLD_YELLOW;
int co2_threshold_red = DEFAULT_CO2_THRESHOLD_RED;

int co2_hysteresis = DEFAULT_CO2_HYSTERESIS;

float temp_threshold_yellow = DEFAULT_TEMP_THRESHOLD_YELLOW;
float temp_threshold_red = DEFAULT_TEMP_THRESHOLD_RED;

float rh_threshold_yellow = DEFAULT_RH_THRESHOLD_YELLOW;
float rh_threshold_red = DEFAULT_RH_THRESHOLD_RED;

const char *room;

const char *host;

int co2 = 0;
float temp = 0;
float rh = 0;

int led_state = 0;

pthread_mutex_t measurement_mutex;
pthread_cond_t measurement_cond;
uint8_t measurement_lock = 1;

int main() {
	uid_t uid;
	pthread_t logging_thread;
	struct timespec measurement_interval = {MEASUREMENT_INTERVAL, 0L};
	int measurement_status;

	// set up logging: include pid, write to system console if log opening fails
	openlog(PACKAGE, LOG_PID|LOG_CONS, LOG_USER);

	// instance already running?
	if(access(PIDFILE, F_OK) == 0) {
		printf("iaq-measurementd already running\n");
		// don't remove files on termination here
		exit(EXIT_SUCCESS);
	}

	uid = getuid();

	if(uid != 0) {
		syslog(LOG_ERR, "please run iaq-measurementd as root and use the "
				"initscript. terminating");
		terminate(EXIT_FAILURE);
	}

	daemonize();

	load_kernel_modules();

	parse_config();

	if(pthread_mutex_init(&measurement_mutex, NULL) != 0) {
		syslog(LOG_ERR, "failed to init pthread_mutex_t: %m. terminating");
		terminate(EXIT_FAILURE);
	}

	if(pthread_cond_init(&measurement_cond, NULL) != 0) {
		syslog(LOG_ERR, "failed to init pthread_cond_t: %m. terminating");
		terminate(EXIT_FAILURE);
	}

	if(pthread_create(&logging_thread, NULL, http_logger, NULL) != 0) {
		syslog(LOG_ERR, "failed to create logging thread: %m. terminating");
		terminate(EXIT_FAILURE);
	}

	// setup GPIO-pins
	inipin();

	// open i2c device file
	if((i2c_fd = open((char *)i2c_device, O_RDWR)) < 0) {
		syslog(LOG_ERR, "failed to open i2c device file %s: %m. terminating",
				i2c_device);
		terminate(EXIT_FAILURE);
	}

	// this function is not thread safe so call it here
	if(curl_global_init(CURL_GLOBAL_ALL)) {
		syslog(LOG_ERR, "failed to initialize libcurl. terminating");
		terminate(EXIT_FAILURE);
	}

	while(1) {
		pthread_mutex_lock(&measurement_mutex);
		measurement_lock = 1;
		pthread_mutex_unlock(&measurement_mutex);

		measurement_status = CO2((int *)&co2);
		if(measurement_status != 0) {
			syslog(LOG_WARNING, "error during co2 measurement");
			if(measurement_status == 1) {
				syslog(LOG_ERR, "terminating");
				terminate(EXIT_FAILURE);
			}
		}

		measurement_status = si7021((float *)&temp, (float *)&rh);
		if(measurement_status != 0) {
			syslog(LOG_WARNING, "error during temp/rh measurement");
			if(measurement_status == 1) {
				syslog(LOG_ERR, "terminating");
				terminate(EXIT_FAILURE);
			}
		}

		led_state = LEDsystem(co2, temp, rh, led_state);

		pthread_mutex_lock(&measurement_mutex);
		measurement_lock = 0;
		pthread_cond_signal(&measurement_cond);
		pthread_mutex_unlock(&measurement_mutex);

		write_state_files();

		nanosleep(&measurement_interval, NULL);
	}
}

void signal_handler(int sig) {
	switch(sig) {
		case SIGHUP:
			parse_config();
			break;
		case SIGTERM:
			syslog(LOG_INFO, "caught SIGTERM. terminating");
			terminate(EXIT_SUCCESS);
			break;
	}
}

void daemonize() {
	FILE *pidfile;

	pid = fork();

	if(pid < 0) {
		syslog(LOG_ERR, "failed to fork process: %m");
		terminate(EXIT_FAILURE);
	}

	if(pid > 0) {
		// dont remove pidfile on parent exit
		exit(EXIT_SUCCESS);
	}

	umask(022);

	sid = setsid();

	if(sid < 0) {
		syslog(LOG_ERR, "failed to get sid: %m");
		terminate(EXIT_FAILURE);
	}

	if(chdir("/") < 0) {
		syslog(LOG_ERR, "failed to chdir to /: %m");
		terminate(EXIT_FAILURE);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	pid = getpid();
	pidfile = fopen(PIDFILE, "w");

	if(pidfile == NULL) {
		syslog(LOG_ERR, "failed to open pidfile " PIDFILE ": %m");
		terminate(EXIT_FAILURE);
	}

	if(fprintf(pidfile, "%d\n", pid) < 0) {
		syslog(LOG_ERR, "failed to write to pidfile " PIDFILE ": %m");
		terminate(EXIT_FAILURE);
	}

	if(pidfile != NULL)
		fclose(pidfile);

	sa.sa_handler = &signal_handler;
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);

	if(sigaction(SIGHUP, &sa, NULL) == -1) {
		syslog(LOG_ERR, "failed to register SIGHUP handler: %m");
		terminate(EXIT_FAILURE);
	}
	if(sigaction(SIGTERM, &sa, NULL) == -1) {
		syslog(LOG_ERR, "failed to register SIGTERM handler: %m");
		terminate(EXIT_FAILURE);
	}
}

// glibc does not provide definitions for init_module() and finit_module()
static inline int finit_module(int fd, const char *uargs, int flags) {
	return syscall(__NR_finit_module, fd, uargs, flags);
}

// load i2c kernel modules
void load_kernel_modules() {
	const char *mod_path, *i2c_bcm2708_relative, *i2c_dev_relative;
	char *mod_path_i2c_bcm2708, *mod_path_i2c_dev;
	struct utsname system_name;
	int mod_fd;
	int ret_syscall;

	mod_path = "/lib/modules/";
	i2c_bcm2708_relative = "/kernel/drivers/i2c/busses/i2c-bcm2708.ko";
	i2c_dev_relative = "/kernel/drivers/i2c/i2c-dev.ko";

	// kernel-modules path contains kernel-release
	// e.g. /lib/modules/4.1.15+/kernel/drivers/i2c/i2c-dev.ko
	if(uname(&system_name) < 0) {
		syslog(LOG_ERR, "failed to get kernel release (uname -r), needed to "
			"load kernel modules: %m");
		terminate(EXIT_FAILURE);
	}

	mod_path_i2c_bcm2708 = malloc(strlen(mod_path) + strlen(system_name.release)
		+ strlen(i2c_bcm2708_relative));

	strcpy(mod_path_i2c_bcm2708, mod_path);
	strcat(mod_path_i2c_bcm2708, system_name.release);
	strcat(mod_path_i2c_bcm2708, i2c_bcm2708_relative);

	mod_fd = open(mod_path_i2c_bcm2708, O_RDONLY | O_CLOEXEC);
	if(mod_fd < 0) {
		syslog(LOG_ERR, "failed to open kernel-module file %s: %m. terminating",
			mod_path_i2c_bcm2708);
		terminate(EXIT_FAILURE);
	}

	ret_syscall = finit_module(mod_fd, "", 0);

	if(ret_syscall != 0) {
		close(mod_fd);
		if(errno != EEXIST) {
			syslog(LOG_ERR, "failed to load kernel-module %s: %m",
				mod_path_i2c_bcm2708);
			terminate(EXIT_FAILURE);
		}
	}

	close(mod_fd);

	mod_path_i2c_dev = malloc(strlen(mod_path) + strlen(system_name.release)
		+ strlen(i2c_dev_relative));

	strcpy(mod_path_i2c_dev, mod_path);
	strcat(mod_path_i2c_dev, system_name.release);
	strcat(mod_path_i2c_dev, i2c_dev_relative);

	mod_fd = open(mod_path_i2c_dev, O_RDONLY | O_CLOEXEC);
	if(mod_fd < 0) {
		syslog(LOG_ERR, "failed to open kernel-module file %s: %m",
				mod_path_i2c_dev);
		terminate(EXIT_FAILURE);
	}

	ret_syscall = finit_module(mod_fd, "", 0);

	if(ret_syscall != 0) {
		close(mod_fd);
		if(errno != EEXIST) {
			syslog(LOG_ERR, "failed to load kernel-module %s: %m",
					mod_path_i2c_dev);
			terminate(EXIT_FAILURE);
		}
	}

	close(mod_fd);
}

void *http_logger() {
	struct timespec logging_interval;
	int co2_local;
	float temp_local, rh_local;
	int led_state_local;

	while(1) {
		logging_interval.tv_sec = logging_interval_sec;
		logging_interval.tv_nsec = 0;

		// don't copy measurement values while measurements are ongoing, to keep
		// measurement values consistent
		pthread_mutex_lock(&measurement_mutex);
		while(measurement_lock)
			pthread_cond_wait(&measurement_cond, &measurement_mutex);

		co2_local = co2;
		temp_local = temp;
		rh_local = rh;
		led_state_local = led_state;

		pthread_mutex_unlock(&measurement_mutex);

		http_log(co2_local, temp_local, rh_local, led_state_local);
		nanosleep(&logging_interval, NULL);
	}
}

// clean up and terminate
void terminate(int status) {
	if(remove(PIDFILE) == -1)
		if(errno != ENOENT)
			syslog(LOG_ERR, "failed to remove pidfile " PIDFILE ": %m."
					" please remove it manually");

	// don't care about errors
	remove(PKGSTATEDIR "/co2");
	remove(PKGSTATEDIR "/temp");
	remove(PKGSTATEDIR "/rh");
	remove(PKGSTATEDIR "/led_state");
	remove(PKGSTATEDIR "/co2_threshold_yellow");
	remove(PKGSTATEDIR "/co2_threshold_red");
	remove(PKGSTATEDIR "/co2_hysteresis");
	remove(PKGSTATEDIR "/temp_threshold_yellow");
	remove(PKGSTATEDIR "/temp_threshold_red");
	remove(PKGSTATEDIR "/rh_threshold_yellow");
	remove(PKGSTATEDIR "/rh_threshold_red");

	digitalWrite(green_pin, OFF);
	digitalWrite(yellow_pin, OFF);
	digitalWrite(red_pin, OFF);

	exit(status);
}
