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
#include <config.h>

#include <ctype.h>
#include <errno.h>	// errno
#include <getopt.h>	// getopt_long
#include <inttypes.h>	// PRIu64
#include <limits.h>	// INT_MAX
#include <math.h>	// sqrt
#include <sched.h>	// sched_*
#include <stdbool.h>	// bool, true, false
#include <stdio.h>
#include <stdint.h>	// int64_t, INT64_MAX
#include <stdlib.h>
#include <string.h>	// strerror, strcmp
#include <sys/io.h>	// iopl
#include <sys/mman.h>	// mlockall
#include <time.h>	// struct timespec, clock_gettime
#include <unistd.h>	// getuid

#include <npt/npt.h>
#include <version.h>

/**
 * Initialize options
 */
void initopt() {
	VERBOSE_OPTION_INIT
	CLI_STI_OPTION_INIT

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

/** The array used to store the histogram */
uint64_t histogram[NPT_HISTOGRAM_SIZE];

/** counter for bigger values than histogram size */
uint64_t histogramOverruns;

/**
 * Show help message
 */
void npt_help() {
	printf("non-preempt test (npt) %s\n", FULL_VERSION);
	printf(	"usage: npt <options>\n\n"
		"	-a CPU		--affinity=CPU		pin the process to the processor CPU (default: %d)\n"
		"	-e		--eval-cpu-speed	evaluate the CPU speed instead of reading it\n"
		"						from /proc/cpuinfo\n"
		"	-h		--help			show this message\n"
		CLI_STI_OPTION_HELP
		"	-l LOOPS	--loops=LOOPS		define the number of loops to do (default: %" PRIu64 ")\n"
		"	-o OUTPUT	--output=OUTPUT		output file for storing the report and histogram\n"
		"			--nanoseconds		do the report and the histogram in nanoseconds\n"
		"			--picoseconds		do the report and the histogram in picoseconds\n"
		"	-p PRIO		--prio=PRIO		priority to use as high prio process (default: %d)\n"
		VERBOSE_OPTION_HELP
		"	-V		--version		show the tool version\n",
		globalArgs.affinity,
		globalArgs.loops,
		globalArgs.priority
	      );
}

/**
 * Treat line command options
 */
int npt_getopt(int argc, char **argv) {

	int c;

	while (1) {
		static struct option long_options[] = {
			// Regular options
			{"affinity",		required_argument,	0,	'a'},
			{"eval-cpu-speed",	no_argument,		0,	'e'},
			{"help",		no_argument,		0,	'h'},
			CLI_STI_OPTION_LONG
			{"loops",		required_argument,	0,	'l'},
			{"output",		required_argument,	0,	'o'},
			{"prio",		required_argument,	0,	'p'},
			{"trace",		required_argument,	0,	't'},
			VERBOSE_OPTION_LONG
			{"version",		no_argument,		0,	'V'},

			// Flags options
			{"nanoseconds",		no_argument, &globalArgs.nanoseconds, true},
			{"picoseconds",		no_argument, &globalArgs.picoseconds, true},
			{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;

		char* shortopt = {"a:eh" CLI_STI_OPTION_SHORT "l:o:p:t:" VERBOSE_OPTION_SHORT "V" };

		c = getopt_long(argc, argv, shortopt,
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1) break;

		switch (c) {
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0) break;

#ifdef DEBUG
				printf ("option %s", long_options[option_index].name);
				if (optarg) printf (" with arg %s", optarg);
				printf (" not available\n");
#endif /* DEBUG */
				break;

			// Option --affinity (-a)
			case 'a':
				if (sscanf(optarg, "%u", &globalArgs.affinity) == 0
					|| globalArgs.affinity >= sysconf(_SC_NPROCESSORS_ONLN)) {
					fprintf(stderr,
						"--affinity: argument must be an integer between 0 and %ld\n",
						sysconf(_SC_NPROCESSORS_ONLN)-1
					       );
					return 1;
				}
				break;

			// Option --eval-cpu-speed (-e)
			case 'e':
				globalArgs.evaluateSpeed = true;
				break;

			// Option --help (-h)
			case 'h':
				npt_help();
				exit(0);
				break;

			// Option --allow-interrupts (-i)
			CLI_STI_OPTION_CASE

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

			// Option --prio (-p)
			case 'p':
				if (sscanf(optarg, "%u", &globalArgs.priority) == 0
					|| globalArgs.priority > 99) {
					fprintf(stderr, "--prio: argument must be an unsigned int between 0 and 99\n");
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
			VERBOSE_OPTION_CASE

			// Option --version (-V)
			case 'V':
				printf("non-preempt test (npt) %s\n", FULL_VERSION);
				printf("Built on %s\n", BUILD_DATE);
				printf("%s", BUILD_OPTIONS);
				exit(0);
				break;

			case '?':
				/* getopt_long already printed an error message. */
				return 1;
				break;

			default:
				abort();
				break;
		}
	}

#ifdef DEBUG
	/* Print any remaining command line arguments (not options). */
	if (optind < argc) {
		printf ("non-option ARGV-elements: ");
		while (optind < argc)
			printf ("%s ", argv[optind++]);
		putchar ('\n');
	}
#endif /* DEBUG */

	return 0;
}

/**
 * Function taken from
 * http://aufather.wordpress.com/2010/09/08/high-performance-time-measuremen-in-linux/
 */
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
	volatile uint64_t i;
	for (i = 0; i < 1e8; i++);
}

/**
 * Gets the cpu speed from /proc/cpuinfo
 */
unsigned long _get_cpu_speed_from_proc_cpuinfo() {
	FILE *pp;
	char buf[30];
	char* cmdLine;

	int ret = asprintf(&cmdLine, "grep \"cpu MHz\" /proc/cpuinfo \
			| head -n%d | tail -n1 | cut -d':' -f2 | tr -d ' \n\r'",
			(globalArgs.affinity+1));

	if (!ret)
		return -1;

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
	clock_gettime(NPT_CLOCK_MONOTONIC, &ts0);
	t0 = rdtsc();

	_cpu_stress();

	// Time after CPU stress
	t1 = rdtsc();
	clock_gettime(NPT_CLOCK_MONOTONIC, &ts1);

	// We compare clock_gettime and rdtsc values
	struct timespec *ts = _timespec_diff(&ts1, &ts0);
	uint64_t ts_nsec = ts->tv_sec * 1000000000LL + ts->tv_nsec;
	return (unsigned long)((double)(t1 - t0) / (double)ts_nsec * 1.0e9);
}

/**
 * Return cpu speed with respect to the choice of evaluation or not
 */
unsigned long get_cpu_speed() {
	if (globalArgs.evaluateSpeed) return _evaluate_cpu_speed();
	else return _get_cpu_speed_from_proc_cpuinfo();
}

/**
 * Function to calculate the time difference between two rdtsc
 */
static __inline__ double diff(uint64_t start, uint64_t end) {
	if (globalArgs.cpuHz == 0) return 0.0;
	else if (end < start)
		return (double)(UINT64_MAX-start+end+1) * globalArgs.cpuPeriod;
	else return (double)(end-start) * globalArgs.cpuPeriod;
}

/**
 * The loop
 */
int cycle() {
	double duration = 0;
	counter = 0;
	uint64_t t0, t1;

	// General statistics
	minDuration = 99999.0;
	maxDuration = 0.0;
	sumDuration = 0.0;

	// For variance and standard deviation
	double deltaDuration = 0.0;
	meanDuration = 0.0;
	double meanSquared = 0.0;
	variance_n = 0.0;
	stdDeviation = 0.0;

	// Time declaration for the first loop
	t0 = rdtsc();
	t1 = t0;

	UST_TRACE_START

	// We are cycling NPT_NOCOUNTLOOP more times to let the system
	// enters in the cycle period we want to analyze
	while (counter < globalArgs.loops+NPT_NOCOUNTLOOP) {
		// Calculate diff between t0 and t1
		duration = diff(t1, t0);

		UST_TRACE_LOOP

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
			meanDuration = meanDuration
				+ deltaDuration / (double)(counter-NPT_NOCOUNTLOOP);
			meanSquared = meanSquared
				+ deltaDuration * (duration - meanDuration);

			// Store data in the histogram if duration < max size
			if (duration < NPT_HISTOGRAM_SIZE)
				histogram[(int)duration]++;
			else histogramOverruns++;
		}

		// Get new t0 from rdtsc function
		t1 = t0;
		t0 = rdtsc();
	}

	UST_TRACE_STOP

	// Readapt the counter to the number of counted cycles
	counter = counter-NPT_NOCOUNTLOOP;

	// Calcul of variance dans standard deviation
	variance_n = meanSquared / (double)counter;
	stdDeviation = sqrt(variance_n);

	return 0;
}

int print_results() {
	int i;
	FILE *hfd = NULL;

	// Print the statistics
	printf("%" PRIu64 " cycles done over %" PRIu64 ".\n", counter, globalArgs.loops);
	printf("Cycles duration:\n");
	printf("	min:		%.6f %s\n", minDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	max:		%.6f %s\n", maxDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	mean:		%.6f %s\n", meanDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	sum:		%.6f %s\n", sumDuration, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	variance:	%g %s\n", variance_n, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
	printf("	std dev:	%.6f %s\n", stdDeviation, UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));

	// If we want to save the histogram in a file
	if (globalArgs.output != NULL) {
		hfd = fopen(globalArgs.output, "w");
		if (hfd == NULL) {
			fprintf(stderr, "Error: unable to open '%s' in write mode.\n", globalArgs.output);
			globalArgs.output = NULL;
		} else {
			fprintf(hfd, "# Data generated by NPT for %" PRIu64 " cycles\n", globalArgs.loops);
			fprintf(hfd, "# The time values are expressed in %s.\n", UNITE(globalArgs.picoseconds, globalArgs.nanoseconds));
			fprintf(hfd, "#\n");
			fprintf(hfd, "#General statistics of cycles duration:\n");
			fprintf(hfd, "#	min:		%.6f\n", minDuration);
			fprintf(hfd, "#	max:		%.6f\n", maxDuration);
			fprintf(hfd, "#	mean:		%.6f\n", meanDuration);
			fprintf(hfd, "#	sum:		%.6f\n", sumDuration);
			fprintf(hfd, "#	variance:	%g\n", variance_n);
			fprintf(hfd, "#	std dev:	%.6f\n", stdDeviation);
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
 * Set the different parameters to favor RT
 */
int setrtmode(bool rt) {
	int ret = EXIT_SUCCESS;

	if (rt) {
		VERBOSE(1, "Enable RT mode");

		// Set CPU affinity
		cpu_set_t cpuMask;
		CPU_ZERO(&cpuMask);
		CPU_SET(globalArgs.affinity, &cpuMask);
		if (sched_setaffinity(getpid(), sizeof(cpuMask), &cpuMask) == EXIT_SUCCESS)
			printf("# CPU affinity set on CPU %d\n", globalArgs.affinity);
		else {
			fprintf(stderr, "Error: unable to set CPU affinity, %s (%d)\n", strerror(errno), errno);
			return EXIT_FAILURE;
		}

		// Set RT scheduler
		if (setrtpriority(globalArgs.priority, SCHED_FIFO) == EXIT_SUCCESS)
			printf("# Application priority set to %d\n", globalArgs.priority);
		else return EXIT_FAILURE;

		// Lock the memory to disable swapping
		if (mlockall(MCL_CURRENT | MCL_FUTURE) == EXIT_SUCCESS)
			VERBOSE(1, "Current and future memory locked");
		else {
			fprintf(stderr, "Error: unable to lock memory, %s (%d)\n", strerror(errno), errno);
			return EXIT_FAILURE;
		}

		// Disable local IRQs
		cli();
	} else {
		VERBOSE(1, "Disable RT mode");

		// Reset scheduler to default, don't "return" directly
		// as we don't want to prevent the enable of the local IRQs
		if (setrtpriority(0, SCHED_OTHER) != EXIT_SUCCESS)
			ret = EXIT_FAILURE;

		// Enable local IRQs
		sti();
	}

	return ret;
}

int main (int argc, char **argv) {
	int i, ret = 0;

	// Init options and load command line arguments
	initopt();
	if (npt_getopt(argc, argv) != EXIT_SUCCESS) exit(1);

	// Running as root ?
	if (getuid() != 0) {
			fprintf(stderr, "Root access is needed. -- Aborting!\n");
			return EXIT_FAILURE;
	}

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

	// Generate and print the results & histogram
	print_results();

end:
	// Free variables
	free(globalArgs.output);
	return ret;

err:
	ret = 1;
	goto end;

}
