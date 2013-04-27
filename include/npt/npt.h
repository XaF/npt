/**
 * Non-Preempt Test (npt) tool
 * Copyright 2012-2013  Raphaël Beamonte <raphael.beamonte@gmail.com>
 *
 * This file is part of Foobar.
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
 * Statistics variables
 */
volatile uint64_t counter;
double minDuration, maxDuration, sumDuration, meanDuration;
double variance_n, stdDeviation;
#ifdef ENABLE_TRACEPOINT_FREQUENCY
	uint64_t tpnb;
#endif /* ENABLE_TRACEPOINT_FREQUENCY */

/**
 * Create a structure to store the variables
 */
struct globalArgs_t {
#ifdef ENABLE_VERBOSE
	int verbosity;		/* -v option */
#endif /* ENABLE_VERBOSE */

#ifdef ENABLE_CLI_STI
	bool allow_interrupts;        /* -i option */
#endif /* ENABLE_CLI_STI */

	unsigned int affinity;  /* -a option */
	uint64_t loops;         /* -l option */
	char* output;           /* -o option */
	unsigned int priority;  /* -p option */
	bool trace_ust;         /* -t option */
	bool trace_kernel;      /* -t option */
#ifdef ENABLE_TRACEPOINT_FREQUENCY
	uint64_t tpmaxfreq;	/* -f option */
#endif /* ENABLE_TRACEPOINT_FREQUENCY */
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
 * Prepare the defines for tracing if necessary
 */
#if defined(WITH_LTTNG_UST) && defined(HAVE_LIBLTTNG_UST)
	#define TRACEPOINT_DEFINE
	#include <npt/tracepoints.h>
	#define UST_TRACE_START	tracepoint(npt, start);
	#define UST_TRACE_LOOP	tracepoint(npt, loop, counter, t0-t1, duration);
	#define UST_TRACE_STOP	tracepoint(npt, stop);
#else
	#undef WITH_UST_TRACE
	#define UST_TRACE_START
	#define UST_TRACE_LOOP
	#define UST_TRACE_STOP
#endif

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
			tpnb++; \
			UST_TRACE_LOOP \
		}
	#define TPMAXFREQ_STATS_PRINT	printf("Tracepoints generated:	%" PRIu64 "\n", tpnb);
	#define TPMAXFREQ_STATS_FILE	fprintf(hfd, "#Tracepoints generated:	%" PRIu64 "\n", tpnb);
#else
	#define BUILD_OPTIONS_TPMAXFREQ
	#define TPMAXFREQ_OPTION_INIT
	#define TPMAXFREQ_OPTION_LONG
	#define TPMAXFREQ_OPTION_SHORT
	#define TPMAXFREQ_OPTION_CASE
	#define TPMAXFREQ_OPTION_HELP
	#define TPMAXFREQ_WORK_INIT
	#define TPMAXFREQ_STATS_PRINT
	#define TPMAXFREQ_STATS_FILE
#endif

/**
 * Define what to put in the loop for the tracepoint
 */
#ifdef TPMAXFREQ_WORK_LOOP
	#define NPT_TRACE_LOOP	TPMAXFREQ_WORK_LOOP
#else
	#define NPT_TRACE_LOOP	UST_TRACE_LOOP
#endif

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
#else
	#define NPT_CLOCK_MONOTONIC CLOCK_MONOTONIC
	#warning Using CLOCK_MONOTONIC as CLOCK_MONOTONIC_RAW is not available
#endif


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
#else /* ENABLE_CLI_STI */
	#define BUILD_OPTIONS_CLI_STI
	#define sti()
	#define cli()
	#define CLI_STI_OPTION_INIT
	#define CLI_STI_OPTION_LONG
	#define CLI_STI_OPTION_SHORT
	#define CLI_STI_OPTION_CASE
	#define CLI_STI_OPTION_HELP
#endif /* ENABLE_CLI_STI */


/**
 * Define the BUILD_OPTIONS string
 */
#ifdef WITH_LTTNG_UST
	#define BUILD_OPTIONS_LTTNG_UST	" --with-lttng-ust"
#else
	#define BUILD_OPTIONS_LTTNG_UST
#endif /* WITH_LTTNG_UST */
#if defined(ENABLE_CLI_STI) || defined(ENABLE_VERBOSE) || defined(WITH_LTTNG_UST)
	#define BUILD_OPTIONS "With" BUILD_OPTIONS_VERBOSE \
		"" BUILD_OPTIONS_CLI_STI \
		"" BUILD_OPTIONS_LTTNG_UST \
		"" BUILD_OPTIONS_TPMAXFREQ \
		"\n"
#else
	#define BUILD_OPTIONS
#endif
