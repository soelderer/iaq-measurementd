bin_PROGRAMS = iaq-measurementd

iaq_measurementd_SOURCES = iaq-measurementd.h iaq-measurementd.c \
	config-parser.h config-parser.c \
	measurement.h measurement.c \
	output.h output.c

AM_CFLAGS =
AM_CFLAGS += -Wall

iaq_measurementd_LDADD =
iaq_measurementd_LDADD += -lconfig
iaq_measurementd_LDADD += -lwiringPi
iaq_measurementd_LDADD += -lpthread
iaq_measurementd_LDADD += ${libcurl_LIBS}

iaq_measurementd_CFLAGS =
iaq_measurementd_CFLAGS += -DSYSCONFDIR='"${sysconfdir}"'
iaq_measurementd_CFLAGS += -DRUNSTATEDIR='"${runstatedir}"'
iaq_measurementd_CFLAGS += -DPKGSTATEDIR='"${pkgstatedir}"'
iaq_measurementd_CFLAGS += ${libcurl_CFLAGS}
