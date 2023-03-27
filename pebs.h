#ifndef PEBS_H
#define PEBS_H

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/mman.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <cstring>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <err.h>
#include <signal.h>
#include <cerrno>
#include <cstddef>

#define PERF_PAGES (1 + (1<<18))
#define SAMPLE_PERIOD_READ 200
#define SAMPLE_PERIOD_WRITE 200
#define CPUNUM 5
//#define CPUNUM 4
#define SAMPLE_READ_CPUID 32
#define SAMPLE_WRITE_CPUID 33

struct perf_sample {
	struct perf_event_header header;
	__u64 ip; /* if PERF_SAMPLE_IP */
	__u32 pid, tid; /* if PERF_SAMPLE_TID */
	__u64 time; /* if PERF_SAMPLE_TIME */
	__u64 addr; /* if PERF_SAMPLE_ADDR */
	//__u64 weight; /* if PERF_SAMPLE_WEIGHT */
	// __u64 phys_addr; /* if PERF_SAMPLE_PHYS_ADDR */
};

enum pbuftype {
	READ = 0,
	WRITE = 1,
	NPBUFTYPES
};
extern int cpuids[CPUNUM];
extern int pids[CPUNUM];

extern void *sample_thread_read_func(void *);
extern pthread_t sample_thread_read_t;
extern void *sample_thread_write_func(void *);
extern pthread_t sample_thread_write_t;

extern __u64 read_config;
extern __u64 write_config;
extern struct perf_event_mmap_page *perf_page[CPUNUM][NPBUFTYPES];
extern int pfd[CPUNUM][NPBUFTYPES];

extern char rfilename[64];
extern FILE *rfp;
extern char wfilename[64];
extern FILE *wfp;

extern void INThandler(int);
extern int _perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags);
extern __u64 _get_read_config();
extern __u64 _get_write_config();
extern struct perf_event_mmap_page *_get_perf_page(int pfd);

extern void init();
extern void perf_setup();

#endif
