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
	if (!strcmp(policy, "FIFO")) {
		return FIFO;
	} else if (!strcmp(policy, "SJF")) {
		return SJF;
	} else if (!strcmp(policy, "RR")) {
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

// basic tail-insert linked list implementation
void insert_back(Job** head, Job* element) {
	element->next = NULL;

	if (*head == NULL) {
		*head = element;
		return;
	}

	Job* p = *head;
	while (p->next) {
		p = p->next;
	}

	p->next = element;
}

// Sorted linked list insert for SJF scheduling
void insert_sorted(Job** head, Job* element) {
	element->next = NULL; // initialize even tho I mgiht overwrite
	// If no head only one element
	if (*head == NULL) {
		*head = element;
		return;
	}

	// If smaller than head, insert and become new head
	if (element->runtime < (*head)->runtime) {
		element->next = *head;
		*head = element;
		return;
	}

	// Walk until finding a job w/ a larger runtime. If it doesn't find one it appends to end of the list
	Job* prev = *head;
	for (Job* p = (*head)->next; p; p = p->next) {
		if (element->runtime < p->runtime) {
			element->next = p;
			prev->next = element;
			return;
		}
		prev = p;
	}

	prev->next = element;
}

// Inserts into the linked list based on the policy
void insert(Job** head, Job* element, SchedulerPolicy policy) {
	switch (policy) {
		case FIFO:
		case RR:
			insert_back(head, element);
			break;
		case SJF:
			insert_sorted(head, element);
			break;
		default:
			insert_back(head, element);
			break;
	}
}

// Always pop's from the head of the linked list. Will be sorted by policy based on the runtime
Job* pop(Job** head) {
	if (head == NULL || *head == NULL) {
		return NULL;
	}
	Job* popped = *head;
	*head = (*head)->next;
	popped->next = NULL;
	return popped;
}

// Cleanly free memory from all objects in this list
void dispose(Job** head) {
	if (head == NULL || *head == NULL)
		return;

	Job* p;
	while ((p = pop(head))) {
		free(p);
	}
	*head = NULL;
}

// Makes a (symbolic?) copy of the list
void clone(Job** dest, const Job** src) {
	*dest = NULL;
	if (!src || !*src) {
		return;
	}

	for (const Job* p = *src; p; p = p->next) {
		Job* job = malloc(sizeof(Job));
		job->id = p->id;
		job->runtime = p->runtime;
		job->next = NULL;
		insert_back(dest, job);
	}
}

// END Job

// BEGIN Options
// this struct is used to store all the options like in the python version
struct options {
	int seed;
	int jobs;
	const int* jobList;
	int jobListLen;
	int maxLength;
	SchedulerPolicy policy;
	const char* policyString;
	int quantum;
	int compute;
	int help;
};

typedef struct options Options;

void printHelp() {
	// I tried to get the spacing just right to match the python impl
	printf("Usage: scheduler [options]\n\n");
	printf("Options:\n");
	printf("  %-22s%s\n", "-h, --help", "show this help message and exit");
	printf("  %-22s%s\n", "-s SEED, --seed=SEED", "the random seed");
	printf("  %-22s%s\n", "-j JOBS, --jobs=JOBS", "number of jobs in the system");
	printf("  %s\n%-24s%s\n%-24s%s\n" , "-l JLIST, --jlist=JLIST", "", "instead of random jobs, provide a comma-separated list", "", "of run times");
	printf("  %s\n%-24s%s\n", "-m MAXLEN, --maxlen=MAXLEN", "", "max length of job");
	printf("  %s\n%-24s%s\n", "-p POLICY, --policy=POLICY", "", "sched policy to use: SJF, FIFO, RR");
	printf("  %s\n%-24s%s\n", "-q QUANTUM, --quantum=QUANTUM", "", "length of time slice for RR policy");
	printf("  %-22s%s\n", "-c", "compute answers for me");

}
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
		printf("\n");
	}
	printf("\n");
}

/**
 * This method parses the arguments passed into the program
 * and puts values into the location provided opts
 */
void parseArguments(Options* opts, int argc, char** argv) {

	opts->help = 0;
	opts->seed = 0; // Default seed for random gen
	opts->jobs = 3; // Number of jobs to generate for random gen
	opts->maxLength = 10; // Max job length for random gen
	opts->jobList = NULL; // Job list overrides jobs, maxLength,
	opts-> policy = FIFO; // Default policy
	opts->policyString = toString(FIFO); // default policy
	opts->quantum = 1; // Default quantum
	opts->compute = 0; // Default to false

	for (int i = 0; i < argc; i++) {
		const char* arg = argv[i];
		if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
			opts->help = 1;
		} else if (!strcmp(arg, "-s") || !strcmp(arg, "--seed")) {
			opts->seed =  atoi(argv[++i]);
		} else if (!strcmp(arg, "-j") || !strcmp(arg, "--jobs")) {
			opts->jobs = atoi(argv[++i]);
		} else if (!strcmp(arg, "-m") || !strcmp(arg, "--maxlen")) {
			opts->maxLength = atoi(argv[++i]);
		} else if (!strcmp(arg, "-p") || !strcmp(arg, "--policy")) {
			const char* policy = argv[++i];
			opts->policyString = policy;
			opts->policy = policyFromString(policy);
		} else if (!strcmp(arg, "-q") || !strcmp(arg, "--quantum")) {
			opts->quantum = atoi(argv[++i]);
		} else if (!strcmp(arg, "-c")) {
			opts->compute = 1; // Set to true
		} else if (!strcmp(arg, "-l") || !strcmp(arg, "--jlist")) {
			const char* jobList = argv[++i];
			int listLen = strlen(jobList);

			// Idk how many slots I need so this is probably wasteful on space, but more efficient than first counting all of the commas
			int* jobs = malloc(sizeof(int) * listLen);
			int jobLen = 0;

			// tl;dr is I wait until i see a comma and take everything up until then and parse into an int and set the
			// start of the next section (last) as where the comma was
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


			// Copy the list of jobs into a newly allocated list on the Options struct and free the temp array
			opts->jobListLen = jobLen;
			opts->jobList = malloc(sizeof(int) * jobLen);
			memcpy(opts->jobList, jobs, jobLen * sizeof(int));

			free(jobs);
		}
	}
}

// END Options

void compute(Job** readyQueue, const Options* opts);
void createJobs(Job** readyQueue, const Options* opts);

int main(int argc, char** argv) {

	Options opts;
	parseArguments(&opts, argc, argv);

	if (opts.help) {
		printHelp();
		return 0;
	}

	printArguments(&opts);

	Job* readyQueue = NULL;

	// I'm passing a reference to the array which makes it way easier to manage the memory in the list
	createJobs(&readyQueue, &opts);

	if (opts.compute) {
		compute(&readyQueue, &opts);
	} else {
		printf("Compute the turnaround time, response time, and wait time for each job.\n");
		printf("When you are done, run this program again, with the same arguments,\n");
		printf("but with -c, which will thus provide you with the answers. You can use\n");
		printf("-s <somenumber> or your own job list (-l 10,15,20 for example)\n");
		printf("to generate different problems for yourself.\n\n");
	}


	// No longer using the jobs queue so free up the memory again
	dispose(&readyQueue);

	if (opts.jobList != NULL) {
		free(opts.jobList);
	}

	return 0;
}

// Creates jobs and adds them to the ready queue. Also prints them.
void createJobs(Job** readyQueue, const Options* opts) {
	srand(opts->seed);
	if (opts->jobList == NULL) {
		// Generate random jobs
		for (int i = 0; i < opts->jobs; i++) {
			Job* job = malloc(sizeof(Job));
			job->id = i;
			job->runtime = rand() % opts->maxLength + 1;
			insert_back(readyQueue, job);
		}
	} else {
		for (int i = 0; i < opts->jobListLen; i++) {
			Job* job = malloc(sizeof(Job));
			job->id = i;
			job->runtime = opts->jobList[i];
			insert_back(readyQueue, job);
		}
	}

	printf("Here is the job list, with the run time of each job: \n");
	// Separated printing here to reduce repetition of code. Just has to iterate over list once more.
	for (Job* p = *readyQueue; p; p = p->next) {
		printf("  Job %d ( length = %.1f )\n", p->id, (float) p->runtime);
	}
	printf("\n\n");
}

void computeFIFO(Job** readyQueue, const Options* opts) {
	int theTime = 0;
	printf("Execution trace:\n");
	for (const Job* job = *readyQueue; job; job = job->next) {
		printf("  [ time %3d ] Run job %d for %.2f secs ( DONE at %.2f )\n", theTime, job->id, (float) job->runtime, (float) theTime + (float) job->runtime);
		theTime += job->runtime;
	}
	printf("\nFinal statistics:\n");

	float t = 0.0f;
	int count = 0;
	float turnaroundSum = 0.0f;
	float waitSum = 0.0f;
	float responseSum = 0.0f;
	for (const Job* job = *readyQueue; job; job = job->next) {
		const int jobId = job->id;
		const float runtime = (float) job->runtime;
		const float response = t;
		const float turnaround = t + runtime;
		const float wait = t;
		printf("  Job %3d -- Response: %3.2f  Turnaround %3.2f  Wait %3.2f\n", jobId, response, turnaround, wait);
		responseSum += response;
		turnaroundSum += turnaround;
		waitSum += wait;
		t += runtime;
		count++;
	}
	printf("\n  Average -- Response: %3.2f  Turnaround %3.2f  Wait %3.2f\n\n", responseSum / (float) count, turnaroundSum / (float) count, waitSum / (float) count);
}

// This structure is used to store the data of each job status instead of having 5 different arrays
struct JobStatus {
	int id;
	int turnaround;
	int response;
	int lastRan;
	int wait;
};
typedef struct JobStatus JobStatus;

void computeRR(Job** jobs, const Options* opts) {
	printf("Execution trace:\n");
	const int totalJobs = opts->jobList != NULL ? opts->jobListLen : opts->jobs;

	// Using a struct for job status info instead of handling 5 different arrays
	JobStatus** statuses = malloc(sizeof(JobStatus*) * totalJobs);
	int quantum = opts->quantum;
	int jobCount = totalJobs;

	for (int i = 0; i < totalJobs; i++) {
		statuses[i] = malloc(sizeof(JobStatus));
		JobStatus* status = statuses[i];
		status->id = i;
		status->lastRan = 0;
		status->wait = 0;
		status->turnaround = 0;
		status->response = -1;
	}

	// Copy the job list
	Job* runList;
	clone(&runList, jobs);

	int theTime = 0;
	while (jobCount > 0) {
		Job* job = pop(&runList);
		const int jobId = job->id;
		JobStatus* status = statuses[jobId];
		if (status->response == -1) {
			status->response = theTime;
		}
		const int currentWait = theTime - status->lastRan;
		status->wait += currentWait;
		int ranFor;
		if (job->runtime > quantum) {
			job->runtime -= quantum;
			ranFor = quantum;
			insert(&runList, job, opts->policy);
			printf("  [ time %3d ] Run job %3d for %.2f secs\n", theTime, jobId, (float) ranFor);
		} else {
			ranFor = job->runtime;
			printf("  [ time %3d ] Run job %3d for %.2f secs ( DONE at %.2f )\n", theTime, jobId, (float) ranFor, (float) theTime + (float) ranFor);
			status->turnaround = theTime + ranFor;
			jobCount--;
		}
		theTime += ranFor;
		status->lastRan = theTime;
	}

	// Method to clean up job linked list now that it's no longer used
	dispose(&runList);

	printf("\nFinal statistics:\n");
	float turnaroundSum = 0.0f;
	float waitSum = 0.0f;
	float responseSum = 0.0f;
	for (const Job* job = *jobs; job; job = job->next) {
		const JobStatus* status = statuses[job->id];
		turnaroundSum += (float) status->turnaround;
		responseSum += (float) status->response;
		waitSum += (float) status->wait;
		printf("  Job %3d -- Response: %3.2f  Turnaround %3.2f  Wait %3.2f\n", job->id, (float) status->response, (float) status->turnaround, (float) status->wait);
	}

	printf("\n  Average -- Response: %3.2f  Turnaround %3.2f  Wait %3.2f\n\n", responseSum / (float) totalJobs, turnaroundSum / (float) totalJobs, waitSum / (float) totalJobs);

	// Free memory from statuses now that they're no longer used. Only place this struct is used so I didn't bother w/ a function
	for (int i = 0; i < totalJobs; i++) {
		free(statuses[i]);
	}
	free(statuses);
}

void compute(Job** readyQueue, const Options* opts) {
	printf("** Solutions **\n\n");
	switch (opts->policy) {
		case SJF: {
			// Sort the queue and assign the readyQueue to the new one
			Job* sorted = NULL;
			Job* p;
			while ((p = pop(readyQueue)) != NULL) {
				insert_sorted(&sorted, p);
			}
			*readyQueue = sorted;
			// No break to fall through into FIFO case
		}
		case FIFO:
			// This works because it's already sorted when inserted with a SJF policy and not when in FIFO.
			computeFIFO(readyQueue, opts);
			break;
		case RR:
			computeRR(readyQueue, opts);
			break;
		default:
			fprintf(stderr, "Error: Policy %s is not available.\n", opts->policyString);
			break;
	}
}

