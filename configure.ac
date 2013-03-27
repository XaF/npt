AC_INIT([npt], [0.02], [raphael.beamonte@gmail.com])
AC_CONFIG_AUX_DIR([config])
AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([config])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([include/config.h])

# For _GNU_SOURCES
AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([LTTNG_UST], [lttng-ust], [
		AC_CHECK_LIB([dl], [dlopen])
		AC_CHECK_LIB([lttng-ust], [main])
		lttngust=true;
	], [
		AC_MSG_NOTICE([The LTTng-UST library was not found, npt tracepoints will not be available])
		lttngust=false;
])
AM_CONDITIONAL([HAVE_LIBLTTNG_UST], [test x$lttngust = xtrue])
AC_CHECK_LIB([m], [sqrt])
AC_CHECK_LIB([rt], [clock_gettime])

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h limits.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([clock_gettime memset sqrt strerror])

nptincludedir="${includedir}/npt"
AC_SUBST(nptincludedir)

AC_CONFIG_FILES([
        Makefile
	include/Makefile
	src/Makefile
])

AC_OUTPUT
