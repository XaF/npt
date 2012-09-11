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
#include <getopt.h>

#include <config.h>

/**
 * Create a structure to store the variables
 */
struct globalArgs_t {
#	ifdef NPT_ALLOW_VERBOSITY
    int verbosity;		/* -v option */
#	endif /* NPT_ALLOW_VERBOSITY */

	unsigned long long loops;	/* -l option */
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
extern __inline__ unsigned long long rdtsc() {
	unsigned long long x;
	__asm__ volatile ("rdtsc" : "=A" (x));
	return x;
}
#elif defined __amd64
extern __inline__ unsigned long long rdtsc() {
	unsigned long long a, d;
	__asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
	return (d<<32) | a;
}
#endif

/**
 * The array used to store the histogram
 */
unsigned long long histogram[NPT_HISTOGRAM_SIZE];

/**
 * Treat line command options
 */
int npt_getopt(int argc, char **argv) {
	
	int c;
     
	while (1) {
		static struct option long_options[] = {
#			ifdef NPT_ALLOW_VERBOSITY
			{"verbose",	no_argument,		0,	'v'},
#			endif /* NPT_ALLOW_VERBOSITY */
			{"loops",	required_argument,	0,	'l'},
			{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;

#		ifdef NPT_ALLOW_VERBOSITY		
		char* shortopt = {"l:v"};
#		else /* NPT_ALLOW_VERBOSITY */
		char* shortopt = {"l:"};
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
		return (unsigned long)(atof(buf) * 1000000.0);
	}
	
	return 0;
}

/**
 * Function to calculate the time difference between two rdtsc
 */
#define MAXULL ((unsigned long long)(~0ULL))
double diff(unsigned long long start, unsigned long long end) {
	if (globalArgs.cpuHz == 0) return 0.0;
	else if (end < start) return (double)(MAXULL-start+end+1ULL) / (double)globalArgs.cpuHz;
	else return (double)(end-start)/(double)globalArgs.cpuHz;
}

/**
 * The loop
 */
int cycle() {
	double duration = 0;
	volatile unsigned long long counter = 0;
	unsigned long long t0, t1;
	double minDuration = 99999.0;
	double maxDuration = 0.0;
	double sumDuration = 0.0;
	
	t0 = rdtsc();
	t1 = t0;
	
	#define NOCOUNTLOOP 5
	
	while (counter < globalArgs.loops+NOCOUNTLOOP) {
		// Calculate diff between t0 and t1
		duration = diff(t1, t0);
		
		// Update stat variables
		if (counter > NOCOUNTLOOP-1) {
			if (duration < minDuration) minDuration = duration;
			if (duration > maxDuration) maxDuration = duration;
			sumDuration += duration;
		}
		
		// Increment counter as we have done one more cycle
		counter++;
		
		// Get new t0 from rdtsc function
		t1 = t0;
		t0 = rdtsc();
	}
	counter = counter-NOCOUNTLOOP;
	
	printf("min: %f us\n", minDuration);
	printf("max: %f us\n", maxDuration);
	printf("mean: %f us\n", sumDuration / (double)counter);
	printf("sum: %f us\n", sumDuration);
	printf("%lld cycles done over %llu.\n", counter, globalArgs.loops);
	
	return 0;
}

int main (int argc, char **argv) {
	initopt();
	if (npt_getopt(argc, argv) != 0) exit(1);
	
	globalArgs.cpuHz = get_cpu_speed();
	if (globalArgs.cpuHz < 0) exit(1);
	
	cycle();
		
	return 0;
}
