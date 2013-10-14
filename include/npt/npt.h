/**
 * Non-Preempt Test (npt) tool
 * Copyright 2012-2013  Raphaël Beamonte <raphael.beamonte@gmail.com>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version
 * 2 as published by the Free Software Foundation.
 *
 * npt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 * or see <http://www.gnu.org/licenses/>.
 */

/**
 * Enable DEBUG messages
 */
//#define DEBUG

/**
 * Define the maximum duration of a cycle that will be stored in the
 * histogram (microseconds)
 */
#define NPT_HISTOGRAM_SIZE 1000000

/**
 * Define the default number of loops
 */
#define NPT_DEFAULT_LOOP_NUMBER 10000000ULL

/**
 * Define the number of loops not to care at the start of the execution
 */
#define NPT_NOCOUNTLOOP 5


/**
 * Prepare the defines for tracing if necessary
 */
#if defined(WITH_LTTNG_UST) && defined(HAVE_LIBLTTNG_UST)
	#define TRACEPOINT_DEFINE
	#include <npt/tracepoints.h>
	#define UST_TRACE_START	tracepoint(npt, start);
	#define UST_TRACE_LOOP	tracepoint(npt, loop, counter, t0-t1, duration);
	#define UST_TRACE_STOP	tracepoint(npt, stop);
#else /* WITH_LTTNG_UST && HAVE_LIBLTTNG_UST */
	#undef WITH_UST_TRACE
	#define UST_TRACE_START
	#define UST_TRACE_LOOP
	#define UST_TRACE_STOP
#endif /* WITH_LTTNG_UST && HAVE_LIBLTTNG_UST */


/**
 * Statistics variables
 */
uint64_t counter;
double multi;
double minDuration, maxDuration, sumDuration, meanDuration;
double variance_n, stdDeviation;
#if defined(WITH_LTTNG_UST) && defined(ENABLE_TRACEPOINT_FREQUENCY)
	uint64_t tpnb;
#endif /* WITH_LTTNG_UST && ENABLE_TRACEPOINT_FREQUENCY */

/**
 * Create a structure to store the variables
 */
struct globalArgs_t {
	unsigned int affinity;  /* -a option */
	uint64_t duration;	/* -d option */

#if defined(WITH_LTTNG_UST) && defined(ENABLE_TRACEPOINT_FREQUENCY)
	uint64_t tpmaxfreq;	/* -f option */
#endif /* WITH_LTTNG_UST && ENABLE_TRACEPOINT_FREQUENCY */

#ifdef ENABLE_CLI_STI
	bool allow_interrupts;	/* -i option */
#endif /* ENABLE_CLI_STI */

	uint64_t loops;         /* -l option */
	int nocountloop;	/* -n option */
	char* output;           /* -o option */
	unsigned int priority;  /* -p option */

#ifdef ENABLE_VERBOSE
	int verbosity;		/* -v option */
#endif /* ENABLE_VERBOSE */

#if defined(WITH_LTTNG_UST) && defined(ENABLE_WINDOWS_MODE)
	uint64_t window_trace;	/* long option */
	uint64_t window_wait;	/* long option */
#endif /* WITH_LTTNG_UST && ENABLE_WINDOWS_MODE */

	int picoseconds;        /* flag */
	int nanoseconds;        /* flag */
	int evaluateSpeed;      /* flag */

	unsigned long cpuHz;
	double cpuPeriod;
} globalArgs;

/**
 * The function used to show verbose messages
 */
#ifdef ENABLE_VERBOSE
	#define BUILD_OPTIONS_VERBOSE	" --enable-verbose"
	static __inline__ void verbose(int lvl, char* txt) {
		if (lvl <= globalArgs.verbosity)
			printf("DEBUG%d: %s\n", lvl, txt);
	}
	#define VERBOSE(lvl, txt) (verbose(lvl, txt))
	#define VERBOSE_OPTION_INIT	globalArgs.verbosity = 0;
	#define VERBOSE_OPTION_LONG	{"verbose",             no_argument,            0,      'v'},
	#define VERBOSE_OPTION_SHORT	"v"
	#define VERBOSE_OPTION_CASE	\
		case 'v': \
			if (globalArgs.verbosity < INT_MAX) { \
				globalArgs.verbosity++; \
			} \
			break;
	#define VERBOSE_OPTION_HELP	"        -v              --verbose               increment verbosity level\n"
#else   /* ENABLE_VERBOSE */
	#define BUILD_OPTIONS_VERBOSE
	#define VERBOSE(lvl, txt) ((void)0)
	#define VERBOSE_OPTION_INIT
	#define VERBOSE_OPTION_LONG
	#define VERBOSE_OPTION_SHORT
	#define VERBOSE_OPTION_CASE
	#define VERBOSE_OPTION_HELP
#endif  /* ENABLE_VERBOSE */


/**
 * Prepare the defines for the trace and wait windows if necessary
 */
#if defined(WITH_LTTNG_UST) && defined(ENABLE_WINDOWS_MODE)
	#define BUILD_OPTIONS_WINDOWSMODE	" --enable-windows-mode"
	#define WINDOWTRACE_OPTION_INIT	globalArgs.window_trace = 0;
	#define WINDOWTRACE_OPTION_LONG	{"trace-window",	required_argument,	0,	2},
	#define WINDOWTRACE_OPTION_CASE	\
		case 2: \
			if (_human_readable_microsecond(optarg, &globalArgs.window_trace, "--trace-window") != 0) { \
				return 1; \
			} else if (!globalArgs.window_wait) { \
				globalArgs.window_wait = globalArgs.window_trace; \
			} \
			break;
	#define WINDOWTRACE_OPTION_HELP	"	" \
					"		" \
					"--trace-window=TIME	" \
					"duration of the trace window when using windows mode\n"

	#define WINDOWWAIT_OPTION_INIT	globalArgs.window_wait = 0;
	#define WINDOWWAIT_OPTION_LONG	{"wait-window",		required_argument,	0,	3},
	#define WINDOWWAIT_OPTION_CASE	\
		case 3: \
			if (_human_readable_microsecond(optarg, &globalArgs.window_wait, "--wait-window") != 0) { \
				return 1; \
			} else if (!globalArgs.window_trace) { \
				globalArgs.window_trace = globalArgs.window_wait; \
			} \
			break;
	#define WINDOWWAIT_OPTION_HELP	"	" \
					"		" \
					"--wait-window=TIME	" \
					"duration of the wait window when using windows mode\n"

	#define WINDOW_OPTION_SCALE	\
		globalArgs.window_trace *= multi * 1.0e-6; \
		globalArgs.window_wait *= multi * 1.0e-6;
	#define WINDOW_WORK_INIT	\
		bool use_windows = (globalArgs.window_trace > 0); \
		int window = 0; \
		double windows_duration[2]; \
		windows_duration[0] = (double)globalArgs.window_wait; \
		windows_duration[1] = (double)globalArgs.window_trace; \
		double window_duration = 0;
	#define WINDOW_WORK_COND	if (!use_windows || window)
	#define WINDOW_WORK_LOOP	\
		if (use_windows) { \
				window_duration += duration; \
				if (window_duration > windows_duration[window]) { \
					window = (window+1)%2; \
					window_duration = 0; \
				} \
			}
#else /* WITH_LTTNG_UST && ENABLE_WINDOwS_MODE */
	#define BUILD_OPTIONS_WINDOWSMODE
	#define WINDOWTRACE_OPTION_INIT
	#define WINDOWTRACE_OPTION_LONG
	#define WINDOWTRACE_OPTION_CASE
	#define WINDOWTRACE_OPTION_HELP

	#define WINDOWWAIT_OPTION_INIT
	#define WINDOWWAIT_OPTION_LONG
	#define WINDOWWAIT_OPTION_CASE
	#define WINDOWWAIT_OPTION_HELP

	#define WINDOW_OPTION_SCALE
	#define WINDOW_WORK_INIT
	#define WINDOW_WORK_COND
	#define WINDOW_WORK_LOOP
#endif /* WITH_LTTNG_UST && ENABLE_WINDOWS_MODE */


/**
 * Prepare the defines for the tracepoint maximum frequency
 * parameter
 */
#if defined(WITH_LTTNG_UST) && defined(ENABLE_TRACEPOINT_FREQUENCY)
	#define BUILD_OPTIONS_TPMAXFREQ	" --enable-tracepoint-frequency"
	#define TPMAXFREQ_OPTION_INIT	globalArgs.tpmaxfreq = 0;
	#define TPMAXFREQ_OPTION_LONG	{"tp-max-freq",		required_argument,	0,	'f'},
	#define TPMAXFREQ_OPTION_SHORT	"f:"
	#define TPMAXFREQ_OPTION_CASE	\
		case 'f': \
			if (sscanf(optarg, "%" PRIu64 "", &globalArgs.tpmaxfreq) == 0) { \
				fprintf(stderr, "--tp-freq: argument must be a 64bits unsigned int\n"); \
				return 1; \
			} \
			break;

	#define TPMAXFREQ_OPTION_HELP	"	" \
					"-f FREQ		" \
					"--tp-max-freq=FREQ	" \
					"define the maximum number of tracepoints to spawn \n" \
					"						" \
					"per second\n"
	#define TPMAXFREQ_WORK_INIT	\
		tpnb = 0; \
		double timebetweentp; \
		if (globalArgs.tpmaxfreq < globalArgs.loops && globalArgs.tpmaxfreq > 0) { \
			double onesecond; \
			if (globalArgs.picoseconds) onesecond = 1.0e12; \
			else if (globalArgs.nanoseconds) onesecond = 1.0e9; \
			else onesecond = 1.0e6; \
			timebetweentp = (double)onesecond/(double)globalArgs.tpmaxfreq; \
		} else { \
			timebetweentp = 0; \
		} \
		double timespent = timebetweentp+1;
	#define TPMAXFREQ_WORK_LOOP	\
		timespent += duration; \
		if (timespent > timebetweentp) { \
			timespent = 0; \
			WINDOW_WORK_COND { \
				tpnb++; \
				UST_TRACE_LOOP \
			} \
		}
	#define TPMAXFREQ_STATS_PRINT	printf("Tracepoints generated:	%" PRIu64 "\n", tpnb);
	#define TPMAXFREQ_STATS_FILE	fprintf(hfd, "#Tracepoints generated:	%" PRIu64 "\n", tpnb);
#else /* WITH_LTTNG_UST && ENABLE_TRACEPOINT_FREQUENCY */
	#define BUILD_OPTIONS_TPMAXFREQ
	#define TPMAXFREQ_OPTION_INIT
	#define TPMAXFREQ_OPTION_LONG
	#define TPMAXFREQ_OPTION_SHORT
	#define TPMAXFREQ_OPTION_CASE
	#define TPMAXFREQ_OPTION_HELP
	#define TPMAXFREQ_WORK_INIT
	#define TPMAXFREQ_STATS_PRINT
	#define TPMAXFREQ_STATS_FILE
#endif /* WITH_LTTNG_UST && ENABLE_TRACEPOINT_FREQUENCY */

/**
 * Define what to put in the loop for the tracepoint
 */
#ifdef TPMAXFREQ_WORK_LOOP
	#define NPT_TRACE_LOOP	TPMAXFREQ_WORK_LOOP
#else /* TPMAXFREQ_WORK_LOOP */
	#define NPT_TRACE_LOOP	WINDOW_WORK_COND { UST_TRACE_LOOP }
#endif /* TPMAXFREQ_WORK_LOOP */

/**
 * Function rdtsc (ReaD Time Stamp Counter) used to calculate the
 * duration of a cycle
 */
#ifdef __i386
	static __inline__ uint64_t rdtsc() {
		uint64_t x;
		__asm__ volatile ("rdtsc" : "=A" (x));
		return x;
	}
#elif defined __amd64
	static __inline__ uint64_t rdtsc() {
		uint64_t a, d;
		__asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
		return (d<<32) | a;
	}
#endif


/**
 * Use CLOCK_MONOTONIC_RAW if available to avoid NTP adjustment
 * requires linux >= 2.6.28
 */
#ifdef CLOCK_MONOTONIC_RAW
	#define NPT_CLOCK_MONOTONIC CLOCK_MONOTONIC_RAW
#else /* CLOCK_MONOTONIC_RAW */
	#define NPT_CLOCK_MONOTONIC CLOCK_MONOTONIC
	#warning Using CLOCK_MONOTONIC as CLOCK_MONOTONIC_RAW is not available
#endif /* CLOCK_MONOTONIC_RAW */


/**
 * Macro to show the right unit using two booleans
 */
#define UNITE(pico, nano) ((pico)?"ps":((nano)?"ns":"us"))


/**
 * Declare or not the sti() and cli() inline functions
 * depending on the configure option
 */
#ifdef ENABLE_CLI_STI
	#define BUILD_OPTIONS_CLI_STI	" --enable-cli-sti"
	/**
	 * Enable local IRQs
	 */
	static __inline__ void sti() {
		iopl(3); // High I/O privileges
		__asm__ volatile ("sti":::"memory");
		iopl(0); // Normal I/O privileges
	}

	/**
	 * Disable local IRQs
	 */
	static __inline__ void cli() {
		iopl(3); // High I/O privileges
		__asm__ volatile ("cli":::"memory");
		iopl(0); // Normal I/O privileges
	}
	#define CLI_STI_OPTION_INIT	globalArgs.allow_interrupts = false;
	#define CLI_STI_OPTION_LONG	{"allow-interrupts",  no_argument,            0,      'i'},
	#define CLI_STI_OPTION_SHORT	"i"
	#define CLI_STI_OPTION_CASE	\
		case 'i': \
			globalArgs.allow_interrupts = true; \
			break;
	#define CLI_STI_OPTION_HELP	"        -i              --allow-interrupts      do not disable interrupts\n"
	#define CLI_STI_OPTION_COND	if (!globalArgs.allow_interrupts)
#else /* ENABLE_CLI_STI */
	#define BUILD_OPTIONS_CLI_STI
	#define sti()
	#define cli()
	#define CLI_STI_OPTION_INIT
	#define CLI_STI_OPTION_LONG
	#define CLI_STI_OPTION_SHORT
	#define CLI_STI_OPTION_CASE
	#define CLI_STI_OPTION_HELP
	#define CLI_STI_OPTION_COND
#endif /* ENABLE_CLI_STI */


/**
 * Define the BUILD_OPTIONS string
 */
#ifdef WITH_LTTNG_UST
	#define BUILD_OPTIONS_LTTNG_UST	" --with-lttng-ust"
#else /* WITH_LTTNG_UST */
	#define BUILD_OPTIONS_LTTNG_UST
#endif /* WITH_LTTNG_UST */
#if defined(ENABLE_CLI_STI) || defined(ENABLE_VERBOSE) || defined(WITH_LTTNG_UST)
	#define BUILD_OPTIONS "With" BUILD_OPTIONS_VERBOSE \
		"" BUILD_OPTIONS_CLI_STI \
		"" BUILD_OPTIONS_LTTNG_UST \
		"" BUILD_OPTIONS_TPMAXFREQ \
		"" BUILD_OPTIONS_WINDOWSMODE \
		"\n"
#else /* ENABLE_CLI_STI || ENABLE_VERBOSE || WITH_LTTNG_UST */
	#define BUILD_OPTIONS
#endif /* ENABLE_CLI_STI || ENABLE_VERBOSE || WITH_LTTNG_UST */
