# Indoor Air Quality Measurement Daemon

This software is part of a diploma thesis about indoor air quality.

The indoor air quality measurement daemon (iaq-measurementd) is for use with the
Raspberry Pi and the SenseAir K-30 CO2 and Si7021 temperature/relative humidity
sensors.

It frequently measures carbon dioxide, temperature and relative humidity.
According to the concentration of carbon dioxide, air quality is rated and then
displayed via three GPIO pins to drive a green, yellow and red LED.

The measured values are sent to a logging-server via HTTP.
See https://github.com/soelderer/iaq-server for the server software.

# Installation

You may want to use the Debian binary package found in the [releases](https://github.com/soelderer/iaq-measurementd/releases/latest).

Alternatively the compilation process is described below.

## Dependencies

 - wiringpi
 - libconfig
 - libcurl

## Download

Get the latest source from [releases](https://github.com/soelderer/iaq-measurementd/releases/latest).

## Compile

1) Unpack the archive

$ tar xf iaq-measurementd-x.x.tar.gz

2) Change to directory created

$ cd iaq-measurementd-x.x

3) Run configure

$ ./configure

4) Compile

$ make

## Install (Optional)

As root:
$ make install

# Legal

iaq-measurementd is released under the terms of the BSD 3-Clause License. A
full copy of the license is included in the COPYING file.
