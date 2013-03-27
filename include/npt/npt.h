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

/**
 * Create a structure to store the variables
 */
struct globalArgs_t {
#ifdef ENABLE_VERBOSE
	int verbosity;		/* -v option */
#endif /* ENABLE_VERBOSE */

	unsigned int affinity;  /* -a option */
	uint64_t loops;         /* -l option */
	char* output;           /* -o option */
	unsigned int priority;  /* -p option */
	bool trace_ust;         /* -t option */
	bool trace_kernel;      /* -t option */
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
#ifdef ENABLE_UST_TRACE
	#ifdef HAVE_LIBLTTNG_UST
		#define TRACEPOINT_DEFINE
		#include <npt/tracepoints.h>
		#define UST_TRACE_START	tracepoint(npt, start);
		#define UST_TRACE_LOOP	tracepoint(npt, loop, counter, t0-t1, duration);
		#define UST_TRACE_STOP	tracepoint(npt, stop);
	#else
		#undef ENABLE_UST_TRACE
		#define UST_TRACE_START
		#define UST_TRACE_LOOP
		#define UST_TRACE_STOP
	#endif
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
#else /* ENABLE_CLI_STI */
	#define sti()
	#define cli()
#endif /* ENABLE_CLI_STI */
