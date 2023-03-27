#include "pebs.h"
#include <sys/resource.h>

int main(int argc, char **argv)
{
	assert(argc == CPUNUM+1);
	for (int i=0;i<CPUNUM;++i) {
		pids[i] = atoi(argv[i+1]);
		assert(pids[i]>0);
	}
	init();
	// launch sampling thread
	assert(0 == pthread_create(&sample_thread_read_t, NULL, sample_thread_read_func, NULL));
	assert(0 == pthread_create(&sample_thread_write_t, NULL, sample_thread_write_func, NULL));
	// wait for sampling threads finish
	void *ret_thread;
	assert(0 == pthread_join(sample_thread_read_t, &ret_thread));
	assert(ret_thread==PTHREAD_CANCELED);
	assert(0 == pthread_join(sample_thread_write_t, &ret_thread));
	assert(ret_thread==PTHREAD_CANCELED);
	return 0;
}

