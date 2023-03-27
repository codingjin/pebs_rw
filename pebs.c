#define _GNU_SOURCE
#include "pebs.h"

int cpuids[CPUNUM] = {34, 35, 36, 37, 38};
int pids[CPUNUM] = {-1, -1, -1, -1, -1};
__u64 read_config, write_config;

struct perf_event_mmap_page *perf_page[CPUNUM][NPBUFTYPES];
int pfd[CPUNUM][NPBUFTYPES];

char rfilename[64];
FILE *rfp;
char wfilename[64];
FILE *wfp;
pthread_t sample_thread_read_t;
pthread_t sample_thread_write_t;

void init()
{
	unsigned long ts = time(NULL);
	snprintf(rfilename, 64, "prof_%lu_r", ts);
	rfp = fopen(rfilename, "w");
	assert(rfp != NULL);
	snprintf(wfilename, 64, "prof_%lu_w", ts);
	wfp = fopen(wfilename, "w");
	assert(wfp != NULL);
	for (int i=0;i<CPUNUM;++i) {
		for (int j=0;j<NPBUFTYPES;++j) {
			perf_page[i][j] = NULL;
			pfd[i][j] = -1;
		}
	}
	perf_setup();
}

void perf_setup()
{
	assert(pfm_initialize() == PFM_SUCCESS);
	struct perf_event_attr attrs[NPBUFTYPES];
	size_t attr_size = sizeof(struct perf_event_attr);
	read_config = _get_read_config();
	write_config = _get_write_config();
	for (int j=0;j<NPBUFTYPES;++j) {
		memset(&attrs[j], 0, attr_size);
		attrs[j].type = PERF_TYPE_RAW;
		attrs[j].size = attr_size;
		if (j == READ) {
			attrs[j].config = read_config;
			attrs[j].sample_period = SAMPLE_PERIOD_READ;
		}else if (j == WRITE) {
			attrs[j].config = write_config;
			attrs[j].sample_period = SAMPLE_PERIOD_WRITE;
		}
		attrs[j].config1 = 0;
		attrs[j].sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_ADDR | PERF_SAMPLE_TIME;
		attrs[j].disabled = 0;
		attrs[j].exclude_kernel = 0;//1;
		attrs[j].exclude_hv = 0;//1;
		attrs[j].exclude_idle = 1;
		attrs[j].exclude_callchain_kernel = 1;
		attrs[j].exclude_callchain_user = 1;
		attrs[j].precise_ip = 1;
		attrs[j].inherit = 1;
	}
	for (int i=0;i<CPUNUM;++i) {
		for (int j=0;j<NPBUFTYPES;++j) {
			pfd[i][j] = _perf_event_open(&attrs[j], pids[i], cpuids[i], -1, 0);
			assert(pfd[i][j] != -1);
			perf_page[i][j] = _get_perf_page(pfd[i][j]);
			assert(perf_page[i][j] != NULL);
		}
	}
	signal(SIGINT, INThandler);
}

__u64 _get_read_config()
{
	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	assert(PFM_SUCCESS == pfm_get_perf_event_encoding("MEM_LOAD_RETIRED.L3_MISS", PFM_PLMH, &attr, NULL, NULL));
	return attr.config;
}

__u64 _get_write_config()
{
	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	assert(PFM_SUCCESS == pfm_get_perf_event_encoding("MEM_INST_RETIRED.STLB_MISS_STORES", PFM_PLMH, &attr, NULL, NULL));
	return attr.config;
}

int _perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

struct perf_event_mmap_page *_get_perf_page(int pfd)
{
	size_t mmap_size = sysconf(_SC_PAGESIZE) * PERF_PAGES;
	struct perf_event_mmap_page *p =
		reinterpret_cast<struct perf_event_mmap_page *>(mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
																MAP_SHARED, pfd, 0));
	assert(p != MAP_FAILED);
	return p;
}

void *sample_thread_read_func(void *arg)
{
	assert(0 == pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
	assert(0 == pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL));

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(SAMPLE_READ_CPUID, &cpuset);
	pthread_t thread = pthread_self();
	assert(0 == pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset));

	while (true) {
		for (int i=0; i<CPUNUM;++i) {
			struct perf_event_mmap_page *p = perf_page[i][READ];
			char *pbuf = (char*)p + p->data_offset;
			//__sync_synchronize();
			if (p->data_head == p->data_tail)	continue;
			struct perf_event_header *ph = 
				reinterpret_cast<struct perf_event_header*>(pbuf + (p->data_tail % p->data_size));
			assert(ph != NULL);

			struct perf_sample *ps;
			switch (ph->type) {
				case PERF_RECORD_SAMPLE:
					ps = (struct perf_sample*)ph;
					assert(ps != NULL);
					if (ps->addr && ps->time) {
						fprintf(rfp, "%llu %llu\n", ps->addr>>12, ps->time);
					}else
						fprintf(stderr, "ZERO_PAGE\n");
					break;
				case PERF_RECORD_THROTTLE:
					fprintf(stderr, "PERF_RECORD_THROTTLE\n");
					break;
				case PERF_RECORD_UNTHROTTLE:
					fprintf(stderr, "PERF_RECORD_UNTHROTTLE\n");
					break;
				default:
					fprintf(stderr, "default\n");
					break;
			}
			p->data_tail += ph->size;
		} // cpu iteration
	} // Repeated Sampling
}

void *sample_thread_write_func(void *arg)
{
	assert(0 == pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
	assert(0 == pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL));

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(SAMPLE_WRITE_CPUID, &cpuset);
	pthread_t thread = pthread_self();
	assert(0 == pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset));

	while (true) {
		for (int i=0; i<CPUNUM;++i) {
			struct perf_event_mmap_page *p = perf_page[i][WRITE];
			char *pbuf = (char*)p + p->data_offset;
			//__sync_synchronize();
			if (p->data_head == p->data_tail)	continue;
			struct perf_event_header *ph = 
				reinterpret_cast<struct perf_event_header*>(pbuf + (p->data_tail % p->data_size));
			assert(ph != NULL);

			struct perf_sample *ps;
			switch (ph->type) {
				case PERF_RECORD_SAMPLE:
					ps = (struct perf_sample*)ph;
					assert(ps != NULL);
					if (ps->addr && ps->time) {
						fprintf(wfp, "%llu %llu\n", ps->addr>>12, ps->time);
					}else
						fprintf(stderr, "ZERO_PAGE\n");
					break;
				case PERF_RECORD_THROTTLE:
					fprintf(stderr, "PERF_RECORD_THROTTLE\n");
					break;
				case PERF_RECORD_UNTHROTTLE:
					fprintf(stderr, "PERF_RECORD_UNTHROTTLE\n");
					break;
				default:
					fprintf(stderr, "default\n");
					break;
			}
			p->data_tail += ph->size;
		} // cpu iteration
	} // Repeated Sampling
}

void INThandler(int sig)
{
	signal(sig, SIG_IGN);
	for (int i=0;i<CPUNUM;++i)
		for (int j=0;j<NPBUFTYPES;++j)
			ioctl(pfd[i][j], PERF_EVENT_IOC_DISABLE, 0);

	assert(0 == pthread_cancel(sample_thread_read_t));
	assert(0 == pthread_cancel(sample_thread_write_t));
	for (int i=0;i<CPUNUM;++i) {
		for (int j=0;j<NPBUFTYPES;++j) {
			if (perf_page[i][j]) {
				munmap(perf_page[i][j], sysconf(_SC_PAGESIZE)*PERF_PAGES);
				perf_page[i][j] = NULL;
			}
			if (pfd[i][j] != -1) {
				close(pfd[i][j]);
				pfd[i][j] = -1;
			}
		}
	}
	fclose(rfp);
	rfp = NULL;
	fclose(wfp);
	wfp = NULL;
}
