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
			{"picoseconds",	no_argument, &globalArgs.picoseconds, true},
			{"nanoseconds",	no_argument, &globalArgs.nanoseconds, true},
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

/**
 * Gets the cpu speed from /proc/cpuinfo
 */
unsigned long get_cpu_speed() {
	FILE *pp;
	char buf[30];
	
	if ((pp = popen("grep \"cpu MHz\" /proc/cpuinfo | head -n1 \
			| cut -d':' -f2 | tr -d ' \n\r'", "r")) == NULL) {
		perror("unable to find CPU speed");
		return -1;
	}
	
	if (fgets(buf, sizeof(buf), pp)) {
		float multi;
		if (globalArgs.picoseconds) multi = 1.0;
		else if (globalArgs.nanoseconds) multi = 1000.0;
		else multi = 1000000.0;
		
		return (unsigned long)(atof(buf) * multi);
	}
	
	return 0;
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
		tracepoint(ust_npt, nptloop, counter, duration);
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
			meanDuration = meanDuration + deltaDuration / (counter-NPT_NOCOUNTLOOP);
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
	variance_n = meanSquared / counter;
	stdDeviation = sqrt(variance_n);

	// Print the statistics
	printf("%" PRIu64 " cycles done over %" PRIu64 ".\n", counter, globalArgs.loops);
	printf("Cycles duration:\n");
	printf("	min:		%f %s\n", minDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	max:		%f %s\n", maxDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	mean:		%f %s\n", sumDuration / (double)counter, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	sum:		%f %s\n", sumDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	variance:	%f %s\n", variance_n, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	std dev:	%f %s\n", stdDeviation, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	
	return 0;
}

int print_histogram() {
	int i;
	FILE *hfd = NULL;
	
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

int main (int argc, char **argv) {
	int i;

	initopt();
	if (npt_getopt(argc, argv) != 0) exit(1);
	
	globalArgs.cpuHz = get_cpu_speed();
	if (globalArgs.cpuHz <= 0) exit(1);
	globalArgs.cpuPeriod = 1.0 / (double)globalArgs.cpuHz;
	
	for (i = 0; i < NPT_HISTOGRAM_SIZE; i++) histogram[i] = 0;
	histogramOverruns = 0;
	
	cycle();
	
	print_histogram();
	
	free(globalArgs.output);
		
	return 0;
}
