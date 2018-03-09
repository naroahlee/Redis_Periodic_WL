#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <hiredis.h>

/*
 * Read the rdtsc value
 */
#if defined(__x86_64__)
static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif

/*
 * Useful CPU related paramters
 * These are based on specific CPU parameters.
 * You need to change them according to your machine
 * cat /proc/cpuinfo    for more info
 */
#define HZ      2099999000      /* second */
#define MHZ     2099999         /* millic second */

typedef struct trace_event_t
{
	unsigned long long release;
	unsigned long long finish;
} trace_event;

int  gs32period = 0;
int  gs32count  = 0;
int  gs32idx    = 0;
char gaccmd[255];
trace_event *gpstrData;
redisContext* gpstrconn;


int dump_data(trace_event* ptr, int s32cnt)
{
	int i;
	double d64response;
	FILE* fp;
	fp = fopen("res.csv", "w");
	if(NULL == fp)
	{
		exit(-3);
	}

	for(i = 0; i < s32cnt; i++)
	{
		d64response = (double)(ptr[i].finish - ptr[i].release) / MHZ;
		fprintf(fp, "%lf\n", d64response);
	}
	
	fclose(fp);
	return 0;
}

/*
 * each job's work
 * record start time, finish time
 */
void release_query(int sig, siginfo_t *extra, void *cruft)
{
	int i;
    redisReply *reply;

    /* We have reached the count. Print res and quit */
	if (gs32idx >= gs32count) {
        //sleep(2);              /* sleep for 10 sec, wait for other task to finish */
		printf("Free Redis Connection...\n");
		dump_data(gpstrData, gs32count);
		free(gpstrData);
		redisFree(gpstrconn);
        exit(1);
    }
	i = gs32idx;
	gs32idx++;

    /* HSet a key */
	sprintf(gaccmd, "HSET myset:%d element:%d xxx", rand(), rand());

	gpstrData[i].release = rdtsc();
    reply = redisCommand(gpstrconn, gaccmd);
	gpstrData[i].finish  = rdtsc();

	/* printf("[PYCLI] %d HSET: %lld\n", i, reply->integer); */
    freeReplyObject(reply);

}

/*
 * Set affinity of the task, alwasy pin it to core 0
 */
void set_sched(int prio)
{
/*
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) < 0) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
*/
    struct sched_param sched;
    sched.sched_priority = prio;
    if (sched_setscheduler(getpid(), SCHED_FIFO, &sched) < 0) {
        perror("sched_setscheduler");
        exit(EXIT_FAILURE);
    }
}

void Usage(void)
{
    fprintf(stderr, "Usage: ./periodic_client -p period -n count\n");
    exit(EXIT_FAILURE);
}

redisContext* establish_redis_connection(void)
{
	redisContext* c;
    const char hostname[20] = "192.168.1.11";
    int port = 6379;
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        }
        else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(-1);
    }

	return c;
}

int flush_redis(redisContext* c)
{
	int res;
    redisReply *reply;
    reply = redisCommand(c,"FLUSHALL");
	res = strcmp(reply->str, "OK");
	freeReplyObject(reply);
	return res;
}

int main(int argc, char **argv) 
{
    int cur_val = 0;
    while ((cur_val = getopt(argc, argv, "p:n:")) != -1) {
        switch (cur_val) {
            case 'p':
                gs32period = atoi(optarg);
                if ( gs32period <= 0 ) {
                    printf("period must be greater than zero\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'n':
                gs32count = atol(optarg);
                if ( gs32count <= 0 ) {
                    printf("count must be greater than zero\n");
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                printf("Error Input!\n");
                Usage();
                exit(EXIT_FAILURE);
                break;
        }
    }

    if (gs32period == 0 || gs32count == 0) {
        Usage();
        exit(1);
    }

    printf("period: %d, count: %d\n", gs32period, gs32count);

	
	gpstrData = (trace_event *)malloc(sizeof(trace_event) * gs32count);
	if(NULL == gpstrData)
	{
		printf("No Enough Memory");
		exit(-3);
	}	

	/* Set Sched */
	set_sched(98);

	/* Generate Random Seed */
    srand(rdtsc());

	/* Establish Connection */
	gpstrconn = establish_redis_connection();
	/* Flush Redis */
    printf("Flush Redis...\n");
	if(0 != flush_redis(gpstrconn))
	{
		printf("Flush Error!\n");
		exit(-2);
	}

	/* =============== SET SIGNAL CALLBACK =============== */
    sigset_t allsigs;
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = release_query;

    if (sigaction(SIGRTMIN, &sa, NULL) < 0) {
        perror("sigaction error");
        exit(EXIT_FAILURE);
    }


	/* =============== SET TIMER ========================== */
    struct itimerspec timerspec;
    timerspec.it_interval.tv_sec = gs32period / 1000;
	/* Correct the Timer Interval Error Regardless of rdtsc*/
    timerspec.it_interval.tv_nsec = (gs32period % 1000) * 1000000;

    /* the start time */
    struct timespec now;
    if(clock_gettime(CLOCK_REALTIME, &now) < 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    timerspec.it_value.tv_sec = now.tv_sec + 1;
    timerspec.it_value.tv_nsec = now.tv_nsec;

	//start_time = rdtsc();
	//start_time += HZ;

    struct sigevent timer_event;
    timer_t timer;
    timer_event.sigev_notify = SIGEV_SIGNAL;
    timer_event.sigev_signo = SIGRTMIN;
    timer_event.sigev_value.sival_ptr = (void *)&timer;

    if (timer_create(CLOCK_REALTIME, &timer_event, &timer) < 0) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    if (timer_settime(timer, TIMER_ABSTIME, &timerspec, NULL) < 0) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    sigemptyset(&allsigs);

    while(1) {
        sigsuspend(&allsigs);
    }

    /* Disconnects and frees the context */
	/* Never reach this */
    redisFree(gpstrconn);

    return 0;
}
