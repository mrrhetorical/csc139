#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
		default:
			insert_back(head, element);
			break;
		case SJF:
			insert_sorted(head, element);
			break;
	}
}

// Always pop's from the head of the linked list. Will be sorted by policy based on the
Job* pop(Job** head) {
	if (head == NULL || *head == NULL) {
		return NULL;
	}
	Job* popped = *head;
	*head = (*head)->next;
	popped->next = NULL;
	return popped;
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
};

typedef struct options Options;

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
void parseArguments(Options* opts, int argc, char** argv) {

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
		if (!strcmpi(arg, "-s") || !strcmpi(arg, "--seed")) {
			opts->seed =  atoi(argv[++i]);
		} else if (!strcmpi(arg, "-j") || !strcmpi(arg, "--jobs")) {
			opts->jobs = atoi(argv[++i]);
		} else if (!strcmpi(arg, "-m") || !strcmpi(arg, "--maxlen")) {
			opts->maxLength = atoi(argv[++i]);
		} else if (!strcmpi(arg, "-p") || !strcmpi(arg, "--policy")) {
			const char* policy = argv[++i];
			opts->policyString = policy;
			opts->policy = policyFromString(policy);
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

// END Options

void compute(Job** readyQueue, const Options* opts);
void createJobs(Job** readyQueue, const Options* opts);

int main(int argc, char** argv) {

	Options opts;
	parseArguments(&opts, argc, argv);
	printArguments(&opts);

	Job* readyQueue = NULL;

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
			insert(readyQueue, job, opts->policy);
		}
	} else {
		for (int i = 0; i < opts->jobListLen; i++) {
			Job* job = malloc(sizeof(Job));
			job->id = i;
			job->runtime = opts->jobList[i];
			insert(readyQueue, job, opts->policy);
		}
	}

	printf("Here is the job list, with the run time of each job:\n");
	// Separated printing here to reduce repetition of code. Just has to iterate over list once more.
	for (Job* p = *readyQueue; p; p = p->next) {
		printf("  Job %d ( length = %d )\n", p->id, p->runtime);
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

struct JobStatus {
	int id;
	int turnaround;
	int response;
	int lastRan;
	int wait;
};
typedef struct JobStatus JobStatus;

void computeRR(const Job** jobs, const Options* opts) {
	printf("Execution trace:\n");
	const int totalJobs = opts->jobList != NULL ? opts->jobListLen : opts->jobs;

	JobStatus** statuses = malloc(sizeof(JobStatus*) * totalJobs);
	int quantum = opts->quantum;
	int jobCount = totalJobs;

	for (int i = 0; i < totalJobs; i++) {
		statuses[i] = malloc(sizeof(JobStatus));
		JobStatus* status = statuses[i];
		status->id = i;
		status->lastRan = 0.0f;
		status->wait = 0.0f;
		status->turnaround = 0.0f;
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
}

void compute(Job** readyQueue, const Options* opts) {
	printf("** Solutions **\n\n");
	switch (opts->policy) {
		case SJF:
		case FIFO:
			// This works because it's already sorted when inserted with a SJF policy and not when in FIFO.
			computeFIFO(readyQueue, opts);
			break;
		case RR:
			computeRR(readyQueue, opts);
			break;
		default:
			fprintf(stderr, "Error: Policy %s is not available.\n", opts->policyString);
	}
}

