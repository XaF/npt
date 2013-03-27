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

# Get configure options
AC_ARG_ENABLE([cli-sti],
	AS_HELP_STRING([--enable-cli-sti], [enable or disable the ability of npt to disable interrupts]),
	[case "${enableval}" in
		yes) enableclisti=true ;;
		no)  enableclisti=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-cli-sti]) ;;
	esac],[
		enableclisti=false
])
if test x$enableclisti = xtrue ; then
	AC_DEFINE([ENABLE_CLI_STI], [1], [Define whether or not we can disable the interrupts])]
fi

AC_ARG_ENABLE([ust-trace],
	AS_HELP_STRING([--disable-ust-trace], [disable the UST tracepoints in NPT]),
	[case "${enableval}" in
		yes) enableusttrace=true ;;
		no)  enableusttrace=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-ust-trace]) ;;
	esac],[
		enableusttrace=true
])
if test x$enableusttrace = xtrue ; then
	AC_DEFINE([ENABLE_UST_TRACE], [1], [Define whether or not we want the UST tracepoints enabled if LTTng-UST is installed])]
fi

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
