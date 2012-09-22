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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <config.h>

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef bool
#define bool int
#endif

/**
 * Create a structure to store the variables
 */
struct globalArgs_t {
#	ifdef NPT_ALLOW_VERBOSITY
    int verbosity;		/* -v option */
#	endif /* NPT_ALLOW_VERBOSITY */

	unsigned long long loops;	/* -l option */
	bool trace_ust;
	bool trace_kernel;
	bool picoseconds;
	bool nanoseconds;
	unsigned long cpuHz;
} globalArgs;

/**
 * Initialize options
 */
void initopt() {
#	ifdef NPT_ALLOW_VERBOSITY
	globalArgs.verbosity = 0;
#	endif /* NPT_ALLOW_VERBOSITY */

	globalArgs.loops = NPT_DEFAULT_LOOP_NUMBER;
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
static __inline__ unsigned long long rdtsc() {
	unsigned long long x;
	__asm__ volatile ("rdtsc" : "=A" (x));
	return x;
}
#elif defined __amd64
static __inline__ unsigned long long rdtsc() {
	unsigned long long a, d;
	__asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
	return (d<<32) | a;
}
#endif

/** The array used to store the histogram */
unsigned long long histogram[NPT_HISTOGRAM_SIZE];

/** counter for bigger values than histogram size */
unsigned long long histogramOverruns;

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
		char* shortopt = {"l:t:v"};
#		else /* NPT_ALLOW_VERBOSITY */
		char* shortopt = {"l:t:"};
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
				if (sscanf(optarg, "%llu", &globalArgs.loops) == 0) {
					fprintf(stderr, "--loops: argument must be an unsigned long long\n");
					return 1;
				}
				break;
			
			// Option --output (-o)
			case 'o':
				
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
#define MAXULL ((unsigned long long)(~0ULL))
static __inline__ double diff(unsigned long long start, unsigned long long end) {
	if (globalArgs.cpuHz == 0) return 0.0;
	else if (end < start) return (double)(MAXULL-start+end+1ULL) / (double)globalArgs.cpuHz;
	else return (double)(end-start) / (double)globalArgs.cpuHz;
}

/**
 * The loop
 */
#define UNITE(pico, nano) ((pico)?"ps":((nano)?"ns":"us"))
int cycle() {
	double duration = 0;
	volatile unsigned long long counter = 0;
	unsigned long long t0, t1;
	double minDuration = 99999.0;
	double maxDuration = 0.0;
	double sumDuration = 0.0;
	
	// Time declaration for the first loop
	t0 = rdtsc();
	t1 = t0;

	// We are cycling NPT_NOCOUNTLOOP more times to let the system
	// enters in the cycle period we want to analyze
	while (counter < globalArgs.loops+NPT_NOCOUNTLOOP) {
		// Calculate diff between t0 and t1
		duration = diff(t1, t0);
		
		// Update stat variables
		if (counter > NPT_NOCOUNTLOOP-1) {
			if (duration < minDuration) minDuration = duration;
			if (duration > maxDuration) maxDuration = duration;
			sumDuration += duration;
			
			// Store data in the histogram if duration < max size
			if (duration < NPT_HISTOGRAM_SIZE) histogram[(int)duration]++;
			else histogramOverruns++;
		}
		
		// Increment counter as we have done one more cycle
		counter++;
		
		// Get new t0 from rdtsc function
		t1 = t0;
		t0 = rdtsc();
	}
	// Readapt the counter to the number of counted cycles
	counter = counter-NPT_NOCOUNTLOOP;

	// Print the statistics
	printf("%lld cycles done over %llu.\n", counter, globalArgs.loops);
	printf("Cycles duration:\n");
	printf("	min:	%f %s\n", minDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	max:	%f %s\n", maxDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	mean:	%f %s\n", sumDuration / (double)counter, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	sum:	%f %s\n", sumDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	
	return 0;
}

int print_histogram() {
	int i;
	
	printf("--------------------------\n");
	printf("duration (%s)	no. cycles\n", UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("--------------------------\n");
	for (i = 0; i < NPT_HISTOGRAM_SIZE; i++) {
		// Just print the lines for which we have data
		if (histogram[i] > 0) {
			printf("%d		%llu\n", i, histogram[i]);
		}
	}
	printf("--------------------------\n");
	printf("Overruns (%d+): %llu\n", NPT_HISTOGRAM_SIZE, histogramOverruns);
	
	return 0;
}

int main (int argc, char **argv) {
	int i;

	initopt();
	if (npt_getopt(argc, argv) != 0) exit(1);
	
	globalArgs.cpuHz = get_cpu_speed();
	if (globalArgs.cpuHz < 0) exit(1);
	
	for (i = 0; i < NPT_HISTOGRAM_SIZE; i++) histogram[i] = 0;
	histogramOverruns = 0;
	
	cycle();
	
	print_histogram();
		
	return 0;
}
