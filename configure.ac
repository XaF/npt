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

####
####
# Get configure options

###
# CLI/STI calls
#
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
	AC_DEFINE([ENABLE_CLI_STI], [1], [Define whether or not we can disable the interrupts])
fi

###
# Verbose
#
AC_ARG_ENABLE([verbose],
	AS_HELP_STRING([--disable-verbose], [disable the ability to enable verbose mode]),
	[case "${enableval}" in
		yes) enableverbose=true ;;
		no)  enableverbose=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --disable-verbose]) ;;
	esac],[
		enableverbose=true
])
if test x$enableverbose = xtrue ; then
	AC_DEFINE([ENABLE_VERBOSE], [1], [Define whether or not we want to allow verbose mode])
fi

###
# Tracepoint Frequency
#
AC_ARG_ENABLE([tracepoint-frequency],
	AS_HELP_STRING([--disable-tracepoint-frequency], [disable the ability to set a maximum tracepoint frequency]),
	[case "${enableval}" in
		yes) enabletpmaxfreq=true ;;
		no)  enabletpmaxfreq=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --disable-tracepoint-frequency]) ;;
	esac],[
		enabletpmaxfreq=true
])
if test x$enabletpmaxfreq = xtrue ; then
	AC_DEFINE([ENABLE_TRACEPOINT_FREQUENCY], [1], [Define whether or not we want to allow to set a maximum tracepoint frequency])
fi

####
####
# Checks for libraries.

###
# LTTng-UST
#
AC_ARG_WITH([lttng-ust],
	AS_HELP_STRING(
		[--without-lttng-ust],
		[Define whether or not we want the UST tracepoints enabled if LTTng-UST is installed]
	),
	[],
	[with_lttng_ust=auto]
)
PKG_CHECK_MODULES([LTTNG_UST], [lttng-ust], [
		have_lttng_ust=yes
		AC_CHECK_LIB([lttng-ust], [lttng_probe_register])
	],
	[have_lttng_ust=no]
)
AS_IF([test \( "x${have_lttng_ust}" = "xyes" \) -a \( "x${with_lttng_ust}" = "xyes" -o "x${with_lttng_ust}" = "xauto" \)], [
		AM_CONDITIONAL([USE_LTTNG_UST], true)
		AC_CHECK_LIB([dl], [dlopen])
		AC_DEFINE([WITH_LTTNG_UST], [1], [Define whether or not we want the UST tracepoints enabled if LTTng-UST is installed])
	], [
		AM_CONDITIONAL([USE_LTTNG_UST], false)
		AS_IF([test "x$with_lttng_ust" = "xyes"], [
			AC_MSG_ERROR([--with-lttng-ust was given but the LTTng-UST library was not found])
		], [
			AS_IF([test "x$with_lttng_ust" = "xauto"], [
				AC_MSG_NOTICE([The LTTng-UST library was not found, npt tracepoints will not be available])
			])
		])
])

###
# Maths
#
AC_CHECK_LIB([m], [sqrt], [], AC_MSG_ERROR([Maths library (-lm) is missing but is needed]))

###
# Realtime
#
AC_CHECK_LIB([rt], [clock_gettime], [], AC_MSG_ERROR([Real-time library (-lrt) is missing but is needed]))

####
####
# Checks for header files.
AC_CHECK_HEADERS([inttypes.h limits.h stdint.h stdlib.h string.h unistd.h])

####
####
# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT64_T

####
####
# Checks for library functions.
AC_CHECK_FUNCS([clock_gettime memset sqrt strerror])

####
####

nptincludedir="${includedir}/npt"
AC_SUBST(nptincludedir)

AC_CONFIG_FILES([
        Makefile
	include/Makefile
	src/Makefile
])

AC_OUTPUT
