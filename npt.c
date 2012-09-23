/*
 * Non-Preempt Test software
 *
 * (C) 2012 Raphaël Beamonte <raphael.beamonte@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version
 * 2 as published by the Free Software Foundation.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <math.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <sys/io.h>

#include <config.h>

#if NPT_TRACE == 1
#	if NPT_LTTNG_UST == 1
#		define TRACEPOINT_DEFINE
#		include "ust_npt.h"
#	else /* NPT_LTTNG_UST == 1 */
#		undef NPT_LTTNG_UST
#	endif /* NPT_LTTNG_UST == 1 */
#	if NPT_LTTNG_KERNEL != 1
#		undef NPT_LTTNG_KERNEL
#	endif /* NPT_LTTNG_KERNEL != 1 */
#else /* NPT_TRACE == 1 */
#	undef NPT_LTTNG_UST
#	undef NPT_LTTNG_KERNEL
#endif /* NPT_TRACE == 1 */

/**
 * Create a structure to store the variables
 */
struct globalArgs_t {
#	ifdef NPT_ALLOW_VERBOSITY
	int verbosity;		/* -v option */
#	endif /* NPT_ALLOW_VERBOSITY */

	uint64_t loops;		/* -l option */
	char* output;		/* -o option */
	bool trace_ust;		/* -t option */
	bool trace_kernel;	/* -t option */
	int picoseconds;	/* flag */
	int nanoseconds;	/* flag */
	int evaluateSpeed;	/* flag */

	int priority;		/* not an option yet */
	int affinity;		/* not an option yet */

	unsigned long cpuHz;
	double cpuPeriod;
} globalArgs;

/**
 * Initialize options
 */
void initopt() {
#	ifdef NPT_ALLOW_VERBOSITY
	globalArgs.verbosity = 0;
#	endif /* NPT_ALLOW_VERBOSITY */

	globalArgs.loops = NPT_DEFAULT_LOOP_NUMBER;
	globalArgs.output = NULL;
	globalArgs.trace_ust = false;
	globalArgs.trace_kernel = false;
	globalArgs.picoseconds = false;
	globalArgs.nanoseconds = false;
	globalArgs.priority = 99;
	globalArgs.affinity = 1;
	globalArgs.evaluateSpeed = 0;
}

#ifdef NPT_ALLOW_VERBOSITY
/**
 * The function used to show verbose messages
 */
void verbose(int lvl, char* txt) {
	if (lvl <= globalArgs.verbosity)
		printf("DEBUG%d: %s\n", lvl, txt);
}
#endif /* NPT_ALLOW_VERBOSITY */

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

/** The array used to store the histogram */
uint64_t histogram[NPT_HISTOGRAM_SIZE];

/** counter for bigger values than histogram size */
uint64_t histogramOverruns;

/**
 * Treat line command options
 */
int npt_getopt(int argc, char **argv) {

	int c;

	while (1) {
		static struct option long_options[] = {
			// Regular options
			{"loops",		required_argument,	0,	'l'},
			{"output",		required_argument,	0,	'o'},
			{"trace",		required_argument,	0,	't'},
#			ifdef NPT_ALLOW_VERBOSITY
			{"verbose",		no_argument,		0,	'v'},
#			endif /* NPT_ALLOW_VERBOSITY */

			// Flags options
			{"picoseconds",		no_argument, &globalArgs.picoseconds, true},
			{"nanoseconds",		no_argument, &globalArgs.nanoseconds, true},
			{"eval-cpu-speed",	no_argument, &globalArgs.evaluateSpeed, true},
			{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;

#		ifdef NPT_ALLOW_VERBOSITY
		char* shortopt = {"l:o:t:v"};
#		else /* NPT_ALLOW_VERBOSITY */
		char* shortopt = {"l:o:t:"};
#		endif /* NPT_ALLOW_VERBOSITY */

		c = getopt_long(argc, argv, shortopt,
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1) break;

		switch (c) {
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0) break;

				printf ("option %s", long_options[option_index].name);
				if (optarg) printf (" with arg %s", optarg);
				printf ("\n");
				break;

			// Option --loops (-l)
			case 'l':
				if (sscanf(optarg, "%" PRIu64 "", &globalArgs.loops) == 0) {
					fprintf(stderr, "--loops: argument must be a 64bits unsigned int\n");
					return 1;
				}
				break;

			// Option --output (-o)
			case 'o':
				if (asprintf(&globalArgs.output, "%s", optarg) < 0) {
					fprintf(stderr, "--output: argument invalid\n");
					return 1;
				}
				break;

			// Option --trace (-t)
			case 't':
				if (strcmp(optarg, "ust") == 0) globalArgs.trace_ust = true;
				else if (strcmp(optarg, "kernel") == 0) globalArgs.trace_kernel = true;
				else {
					fprintf(stderr, "--trace: argument must be one of 'ust' or 'kernel'\n");
					return 1;
				}
				break;

			// Option --verbose (-v)
			case 'v':
				globalArgs.verbosity++;
				break;

			case '?':
				/* getopt_long already printed an error message. */
				return 1;
				break;

			default:
				abort();
		}
	}

	/* Print any remaining command line arguments (not options). */
	if (optind < argc) {
		printf ("non-option ARGV-elements: ");
		while (optind < argc)
			printf ("%s ", argv[optind++]);
		putchar ('\n');
	}

	return 0;
}

struct timespec *_timespec_diff(struct timespec *ts1, struct timespec *ts2) {
	static struct timespec ts;
	ts.tv_sec = ts1->tv_sec - ts2->tv_sec;
	ts.tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
	if (ts.tv_nsec < 0) {
		ts.tv_sec--;
		ts.tv_nsec += 1e9;
	}
	return &ts;
}

/**
 * Stress the CPU to disallow frequency scaling
 */
void __inline__ _cpu_stress() {
	// We have to stress the CPU to be sure it's not in frequency scaling
	volatile uint64_t i;
	for (i = 0; i < 100000000; i++);
}

/**
 * Gets the cpu speed from /proc/cpuinfo
 */
unsigned long _get_cpu_speed_from_proc_cpuinfo() {
	FILE *pp;
	char buf[30];
	char* cmdLine;

	asprintf(&cmdLine, "grep \"cpu MHz\" /proc/cpuinfo | head -n%d \
			| tail -n1 | cut -d':' -f2 | tr -d ' \n\r'",
			(globalArgs.affinity+1));

	_cpu_stress();

	if ((pp = popen(cmdLine, "r")) == NULL) {
		perror("unable to find CPU speed");
		free(cmdLine);
		return -1;
	}
	free(cmdLine);

	if (fgets(buf, sizeof(buf), pp)) {
		return (unsigned long)(atof(buf) * 1.0e6);
	}

	return 0;
}

/**
 * Gets the cpu speed by testing the system
 */
unsigned long _evaluate_cpu_speed() {
	struct timespec ts0, ts1;
	uint64_t t0 = 0, t1 = 0;

	// Time before CPU stress
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts0);
	t0 = rdtsc();

	_cpu_stress();

	// Time after CPU stress
	t1 = rdtsc();
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts1);

	// We compare clock_gettime and rdtsc values
	struct timespec *ts = _timespec_diff(&ts1, &ts0);
	uint64_t ts_nsec = ts->tv_sec * 1000000000LL + ts->tv_nsec;
	return (unsigned long)((double)(t1 - t0) / (double)ts_nsec * 1.0e9);
}

unsigned long get_cpu_speed() {
	if (globalArgs.evaluateSpeed) return _evaluate_cpu_speed();
	else return _get_cpu_speed_from_proc_cpuinfo();
}

/**
 * Function to calculate the time difference between two rdtsc
 */
static __inline__ double diff(uint64_t start, uint64_t end) {
	if (globalArgs.cpuHz == 0) return 0.0;
	else if (end < start) return (double)(UINT64_MAX-start+end+1) * globalArgs.cpuPeriod;
	else return (double)(end-start) * globalArgs.cpuPeriod;
}

/**
 * The loop
 */
#define UNITE(pico, nano) ((pico)?"ps":((nano)?"ns":"us"))
int cycle() {
	double duration = 0;
	volatile uint64_t counter = 0;
	uint64_t t0, t1;

	// General statistics
	double minDuration = 99999.0;
	double maxDuration = 0.0;
	double sumDuration = 0.0;

	// For variance and standard deviation
	double deltaDuration = 0.0;
	double meanDuration = 0.0;
	double meanSquared = 0.0;
	double variance_n, stdDeviation;

	// Time declaration for the first loop
	t0 = rdtsc();
	t1 = t0;

	// We are cycling NPT_NOCOUNTLOOP more times to let the system
	// enters in the cycle period we want to analyze
	while (counter < globalArgs.loops+NPT_NOCOUNTLOOP) {
		// Calculate diff between t0 and t1
		duration = diff(t1, t0);

#		ifdef NPT_LTTNG_UST
		tracepoint(ust_npt, nptloop, counter, t0-t1, duration);
#		endif /* NPT_LTTNG_UST */


		// Increment counter as we have done one more cycle
		counter++;

		// Update stat variables
		if (counter > NPT_NOCOUNTLOOP) {
			// General statistics
			if (duration < minDuration) minDuration = duration;
			if (duration > maxDuration) maxDuration = duration;
			sumDuration += duration;

			// For variance and standard deviation
			deltaDuration = duration - meanDuration;
			meanDuration = meanDuration + deltaDuration / (double)(counter-NPT_NOCOUNTLOOP);
			meanSquared = meanSquared + deltaDuration * (duration - meanDuration);

			// Store data in the histogram if duration < max size
			if (duration < NPT_HISTOGRAM_SIZE) histogram[(int)duration]++;
			else histogramOverruns++;
		}

		// Get new t0 from rdtsc function
		t1 = t0;
		t0 = rdtsc();
	}
	// Readapt the counter to the number of counted cycles
	counter = counter-NPT_NOCOUNTLOOP;

	// Calcul of variance dans standard deviation
	variance_n = meanSquared / (double)counter;
	stdDeviation = sqrt(variance_n);

	// Print the statistics
	printf("%" PRIu64 " cycles done over %" PRIu64 ".\n", counter, globalArgs.loops);
	printf("Cycles duration:\n");
	printf("	min:		%.6f %s\n", minDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	max:		%.6f %s\n", maxDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	mean:		%.6f %s\n", meanDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	sum:		%.6f %s\n", sumDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	variance:	%g %s\n", variance_n, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	std dev:	%.6f %s\n", stdDeviation, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));

	return 0;
}

int print_histogram() {
	int i;
	FILE *hfd = NULL;

	// If we want to save the histogram in a file
	if (globalArgs.output != NULL) {
		hfd = fopen(globalArgs.output, "w");
		if (hfd == NULL) {
			fprintf(stderr, "Error: unable to open '%s' in write mode.\n", globalArgs.output);
			globalArgs.output = NULL;
		} else {
			fprintf(hfd, "# Data generated by NPT for %" PRIu64 " cycles\n", globalArgs.loops);
			fprintf(hfd, "# The time value is expressed in %s.\n", UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
			fprintf(hfd, "#\n");
			fprintf(hfd, "#	time	nb. cycles\n");
			fprintf(hfd, "#	------------------\n");
		}
	}

	printf("--------------------------\n");
	printf("duration (%s)	nb. cycles\n", UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("--------------------------\n");
	for (i = 0; i < NPT_HISTOGRAM_SIZE; i++) {
		// Just print the lines for which we have data
		if (histogram[i] > 0) {
			printf("%d		%" PRIu64 "\n", i, histogram[i]);
			if (globalArgs.output != NULL)
				fprintf(hfd, "	%d	%" PRIu64 "\n", i, histogram[i]);
		}
	}
	if (globalArgs.output != NULL) fclose(hfd);
	printf("--------------------------\n");
	printf("Overruns (%d+): %" PRIu64 "\n", NPT_HISTOGRAM_SIZE, histogramOverruns);

	return 0;
}

/**
 * Update scheduler to the given priority and policy
 */
int setrtpriority(int priority, int policy) {
	struct sched_param schedp;

	memset(&schedp, 0, sizeof(schedp));
	schedp.sched_priority = priority;
	if (sched_setscheduler(0, policy, &schedp) != EXIT_SUCCESS) {
		fprintf(stderr, "Error: unable to set scheduler, %s (%d)\n", strerror(errno), errno);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/**
 * Enable local IRQs
 */
static __inline__ void sti() {
	iopl(3); // High I/O privileges
	__asm__ __volatile__ ("sti":::"memory");
	iopl(0); // Normal I/O privileges
}

/**
 * Disable local IRQs
 */
static __inline__ void cli() {
	iopl(3); // High I/O privileges
	__asm__ __volatile__ ("cli":::"memory");
	iopl(0); // Normal I/O privileges
}

/**
 * Set the different parameters to favor RT
 */
int setrtmode(bool rt) {
	if (rt) {
		// Set CPU affinity
		cpu_set_t cpuMask;
		CPU_ZERO(&cpuMask);
		CPU_SET(1, &cpuMask);
		if (sched_setaffinity(globalArgs.affinity, sizeof(cpuMask), &cpuMask) == EXIT_SUCCESS)
			printf("# CPU affinity set on CPU %d\n", globalArgs.affinity);
		else {
			fprintf(stderr, "Error: unable to set CPU affinity, %s (%d)\n", strerror(errno), errno);
			return EXIT_FAILURE;
		}

		// Set RT scheduler
		if (setrtpriority(globalArgs.priority, SCHED_FIFO) == EXIT_SUCCESS)
			printf("# Application priority set to %d\n", globalArgs.priority);
		else return EXIT_FAILURE;

		// Disable local IRQs
		cli();
	} else {
		// Reset scheduler to default
		if (setrtpriority(0, SCHED_OTHER) != EXIT_SUCCESS)
			return EXIT_FAILURE;

		// Enable local IRQs
		sti();
	}
	
	return EXIT_SUCCESS;
}

int main (int argc, char **argv) {
	int i, ret = 0;

	// Running as root ?
	if (getuid() != 0) {
			printf("Root access is needed. -- Aborting!\n");
			return EXIT_SUCCESS;
	}
	
	// Init options and load command line arguments
	initopt();
	if (npt_getopt(argc, argv) != 0) exit(1);

	// Prepare histogram
	for (i = 0; i < NPT_HISTOGRAM_SIZE; i++) histogram[i] = 0;
	histogramOverruns = 0;

	// Enter in RT mode
	if (setrtmode(true) != EXIT_SUCCESS)
		goto err;

	// Get CPU frequency and calculate period
	globalArgs.cpuHz = get_cpu_speed();
	if (globalArgs.cpuHz <= 0) exit(1);

	double multi;
	if (globalArgs.picoseconds) multi = 1.0e12;
	else if (globalArgs.nanoseconds) multi = 1.0e9;
	else multi = 1.0e6;
	globalArgs.cpuPeriod = multi / (double)globalArgs.cpuHz;

	printf("# CPU frequency (%s): %.02f MHz\n",
		((globalArgs.evaluateSpeed)?"evaluation":"/proc/cpuinfo"),
		globalArgs.cpuHz / 1e6);

	// Start cycling
	cycle();

	// Exit RT mode
	setrtmode(false);

	// Generate and print the histogram
	print_histogram();

end:
	// Free variables
	free(globalArgs.output);
	return ret;

err:
	ret = 1;
	goto end;

}
