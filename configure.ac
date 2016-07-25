AC_INIT([iaq-measurementd], [1.0], [paul.soelder@students.htlinn.ac.at])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_PROG_CC
AC_PROG_MKDIR_P

PKG_CHECK_MODULES([libcurl], [libcurl])

AC_CHECK_HEADERS([wiringPi.h], , AC_MSG_ERROR([Required header wiringPi.h not
found]))

AC_SUBST([runstatedir], ['${localstatedir}/run'])
AC_SUBST([pkgstatedir], ['${localstatedir}/lib/${PACKAGE_NAME}'])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
