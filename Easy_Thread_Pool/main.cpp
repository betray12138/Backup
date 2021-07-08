#include <stdio.h>
#include "threadpool.h"

void taskFunc(void *arg)
{
	int num = *(int *)arg;
	printf("thread %ld is working, number = %d\n", pthread_self(), num);
	sleep(1);
}

int main()
{
	ThreadPool *pool = ThreadPoolCreate(3, 10, 100);
	for (int i = 0; i < 100; i++) {
		int *tmp = new int;
		*tmp = i + 100;
		ThreadPoolAdd(pool, taskFunc, tmp);
	}
	sleep(30);
	ThreadPoolDestroy(pool);
    return 0;
}