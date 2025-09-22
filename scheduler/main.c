#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// BEGIN Scheduler Policy

enum scheduler_policy {
	FIFO = 0, // First-in, first-out
	SJF = 1, // Shortest-Job-First
	RR = 2 // Round-Robin
};
typedef enum scheduler_policy SchedulerPolicy;

SchedulerPolicy policyFromString(const char* policy) {
	if (!strcmpi(policy, "FIFO")) {
		return FIFO;
	} else if (!strcmpi(policy, "SJF")) {
		return SJF;
	} else if (!strcmpi(policy, "RR")) {
		return RR;
	}

	return FIFO;
}
const char* toString(SchedulerPolicy policy) {
	switch (policy) {
		case FIFO:
			return "FIFO";
		case SJF:
			return "SJF";
		case RR:
			return "RR";
		default:
			return "FIFO";
	}
}

// END Scheduler Policy

// BEGIN Job
struct job {
	int id;
	int runtime;
	struct job* next;
};
typedef struct job Job;

// END Job


// BEGIN Arguments / Options
struct arguments {
	int seed;
	int jobs;
	const int* jobList;
	int jobListLen;
	int maxLength;
	SchedulerPolicy policy;
	int quantum;
	int compute;
};

typedef struct arguments Options;

void printArguments(Options* opts) {
	printf("ARG policy %s\n", toString(opts->policy));
	if (opts->jobList == NULL) {
		printf("ARG jobs %d\n", opts->jobs);
		printf("ARG maxlen %d\n", opts->maxLength);
		printf("ARG seed %d\n", opts->seed);
	} else {
		printf("ARG jlist ");
		for (int i = 0; i < opts->jobListLen; i++) {
			printf("%d", opts->jobList[i]);
			if (i != opts->jobListLen - 1) {
				printf(",");
			}
		}
		printf("\n\n");
	}
}
void parseArguments(Options* opts, int argc, char** argv) {

	opts->seed = 0; // Default seed for random gen
	opts->jobs = 3; // Number of jobs to generate for random gen
	opts->maxLength = 10; // Max job length for random gen
	opts->jobList = NULL; // Job list overrides jobs, maxLength,
	opts-> policy = FIFO; // Default policy
	opts->quantum = 1; // Default quantum

	for (int i = 0; i < argc; i++) {
		const char* arg = argv[i];
		if (!strcmpi(arg, "-s") || !strcmpi(arg, "--seed")) {
			opts->seed =  atoi(argv[++i]);
		} else if (!strcmpi(arg, "-j") || !strcmpi(arg, "--jobs")) {
			opts->jobs = atoi(argv[++i]);
		} else if (!strcmpi(arg, "-m") || !strcmpi(arg, "--maxlen")) {
			opts->maxLength = atoi(argv[++i]);
		} else if (!strcmpi(arg, "-p") || !strcmpi(arg, "--policy")) {
			opts->policy = policyFromString(argv[++i]);
		} else if (!strcmpi(arg, "-q") || !strcmpi(arg, "--quantum")) {
			opts->quantum = atoi(argv[++i]);
		} else if (!strcmpi(arg, "-c")) {
			opts->compute = 1; // Set to true
		} else if (!strcmpi(arg, "-l") || !strcmpi(arg, "--jlist")) {
			const char* jobList = argv[++i];
			int listLen = strlen(jobList);

			int* jobs = malloc(sizeof(int) * listLen);
			int jobLen = 0;

			int k, last;
			for (k = 0, last = 0; k <= listLen; k++) {
				if (jobList[k] == ',' || k == listLen) {
					// When encountering a comma, get everything from the end of the last token to this
					char* slice = malloc(sizeof(char) * (k - last + 1));
					memcpy(slice, &jobList[last], k - last);
					slice[k - last] = '\0';
					int dur = atoi(slice);
					jobs[jobLen++] = dur;

					last = k + 1;

					free(slice);
				}
			}


			opts->jobListLen = jobLen;
			opts->jobList = malloc(sizeof(int) * jobLen);
			memcpy(opts->jobList, jobs, jobLen * sizeof(int));

			free(jobs);
		}
	}
}

// END Arguments / Options


int main(int argc, char** argv) {

	Options opts;
	parseArguments(&opts, argc, argv);
	printArguments(&opts);

	return 0;
}



