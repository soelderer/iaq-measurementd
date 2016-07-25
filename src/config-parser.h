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
 * src/config-parser.h
 *
 * Header file for the config parser
 */

#ifndef _IAQ_MEASUREMENTD_CONFIG_PARSER_H_
#define _IAQ_MEASUREMENTD_CONFIG_PARSER_H_
#endif

#include <libconfig.h>
#include "config.h"

#define CONFFILE SYSCONFDIR "/" PACKAGE_NAME ".cfg"

void parse_config();
