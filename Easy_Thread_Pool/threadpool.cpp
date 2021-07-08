#include "threadpool.h"

const int DELTA = 2;

// 定义任务结构体
typedef struct Task
{
	void(*func)(void *arg);
	void *arg;
}Task;

// 定义线程池结构体
struct ThreadPool {
	//任务队列
	Task *task;				// 任务队列
	int queueCapacity;		// 任务容量
	int queueSize;			// 当前任务个数
	int queueFront;			// 队头 取数据
	int queueRear;			// 队尾 放数据

	pthread_t magagerID;	// 管理者线程ID
	pthread_t *workIDs;		// 工作者线程ID

	int min_thread_cnt;		// 最小线程数量
	int max_thread_cnt;		// 最大线程数量
	int busy_thread_cnt;	// 忙的线程数量
	int live_thread_cnt;	// 活着的线程数量
	int exit_thread_cnt;	// 要销毁的线程个数
	pthread_mutex_t mutexpool;	// 锁整个的线程池
	pthread_mutex_t mutexbusy;	// 锁busy_thread_cnt变量
	pthread_cond_t notFull;		// 记录任务队列是否满
	pthread_cond_t notEmpty;	// 记录任务队列是否空

	bool shutdown;	//是否销毁线程池  1- 销毁 0 - 未销毁

};

ThreadPool *ThreadPoolCreate(const int min_cnt, const int max_cnt, const int queue_size)
{
	ThreadPool *pool = new ThreadPool;
	do {
		if (pool == NULL) {
			cout << "线程池创建失败" << endl;
			break;
		}
		pool->workIDs = new pthread_t[max_cnt];		//创建时限制最大空间
		if (pool->workIDs == NULL) {
			cout << "工作线程创建失败" << endl;
			break;
		}
		memset(pool->workIDs, 0, sizeof(pthread_t) * max_cnt);

		pool->min_thread_cnt = min_cnt;
		pool->max_thread_cnt = max_cnt;
		pool->busy_thread_cnt = 0;
		pool->live_thread_cnt = min_cnt;	//初始创建min_cnt个线程
		pool->exit_thread_cnt = 0;

		if (pthread_mutex_init(&pool->mutexpool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexbusy, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0) {
			cout << "互斥条件变量创建失败" << endl;
			break;
		}
		// 任务队列
		pool->task = new Task[queue_size];
		if (pool->task == NULL) {
			cout << "任务队列创建失败" << endl;
			break;
		}
		pool->queueCapacity = queue_size;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = false;


		// 创建管理者线程 管理者线程地址  线程属性  管理者线程执行的函数  参数
		pthread_create(&pool->magagerID, NULL, manager, pool);

		for (int i = 0; i < min_cnt; i++) {
			pthread_create(&pool->workIDs[i], NULL, worker, pool);
		}

		return pool;
	} while (0);
	
	// 释放资源
	if (pool && pool->workIDs) {
		delete []pool->workIDs;
	}
	if (pool && pool->task) {
		delete[]pool->task;
	}
	if (pool) {
		delete pool;
	}
	return NULL;
}

void *worker(void * arg)
{
	// 类型转换获取ThreadPool
	ThreadPool *pool = (ThreadPool *)arg;

	while (1) {
		pthread_mutex_lock(&pool->mutexpool);
		// 判断当前任务队列是否为空
		while (pool->queueSize == 0 && pool->shutdown == false) {
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// 判断是不是要销毁
			if (pool->exit_thread_cnt > 0) {
				pool->exit_thread_cnt--;

				// 若最后的值 小于min_thread_cnt 则不退出
				if (pool->live_thread_cnt > pool->min_thread_cnt) {
					// 退出前记得解锁
					pool->live_thread_cnt--;
					pthread_mutex_unlock(&pool->mutexpool);
					ThreadExit(pool);
				}
			}
		}

		// 判断线程池是否被关闭了
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexpool);
			ThreadExit(pool);
		}

		// 从任务队列中取出一个任务
		Task task;
		task.func = pool->task[pool->queueFront].func;
		task.arg = pool->task[pool->queueFront].arg;

		// 取出任务后移动头节点 —— 循环队列
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;

		// 唤醒生产者
		pthread_cond_signal(&pool->notFull);

		pthread_mutex_unlock(&pool->mutexpool);

		// 函数开始执行 忙的个数+1
		pthread_mutex_lock(&pool->mutexbusy);
		pool->busy_thread_cnt++;
		pthread_mutex_unlock(&pool->mutexbusy);

		task.func(task.arg);  // 传递堆内存 运行结束后释放资源
		delete task.arg;
		task.arg = NULL;

		// 函数执行结束 忙的个数-1
		pthread_mutex_lock(&pool->mutexbusy);
		pool->busy_thread_cnt--;
		pthread_mutex_unlock(&pool->mutexbusy);
	}
	return NULL;
}

void *manager(void *arg)
{
	// 类型转换获取ThreadPool
	ThreadPool *pool = (ThreadPool *)arg;

	while (!pool->shutdown) {
		// 每隔3s检测一次
		sleep(3);

		// 取出线程中任务的数量和当前线程的数量以及忙线程数量
		pthread_mutex_lock(&pool->mutexpool);
		int queueSize = pool->queueSize;
		int liveCnt = pool->live_thread_cnt;
		int busyCnt = pool->busy_thread_cnt;
		pthread_mutex_unlock(&pool->mutexpool);

		// 添加线程
		// 规定：当前任务个数 > 存活个数 && 存活线程数 < 最大线程数
		if (queueSize > liveCnt && liveCnt < pool->max_thread_cnt) {
			pthread_mutex_lock(&pool->mutexpool);
			int counter = 0;	// 每次增加deleta个线程
			for (int i = 0; i < pool->max_thread_cnt && counter < DELTA && pool->live_thread_cnt < pool->max_thread_cnt; i++) {
				if (pool->workIDs[i] == 0) {
					//值为0 表示此处未存放线程id
					pthread_create(&pool->workIDs[i], NULL, worker, pool);
					counter++;
					pool->live_thread_cnt++;
				}
			}
			pthread_mutex_unlock(&pool->mutexpool);
		}

		// 销毁线程
		// 规定：忙的线程 * 2 < 存活的线程数 && 存活的线程数 > 最小的线程数
		if (busyCnt * 2 < liveCnt && liveCnt > pool->min_thread_cnt) {
			pthread_mutex_lock(&pool->mutexpool);
			pool->exit_thread_cnt = DELTA;
			pthread_mutex_unlock(&pool->mutexpool);

			// 让工作的线程自动销毁
			for (int i = 0; i < DELTA; i++) {
				// 每次唤醒因为这个睡眠原因的所有线程
				pthread_cond_signal(&pool->notEmpty);
			}
		}
	}
	return NULL;
}

void ThreadExit(ThreadPool *pool)
{
	pthread_t tid = pthread_self();

	for (int i = 0; i < pool->max_thread_cnt; i++) {
		if (pool->workIDs[i] == tid) {
			pool->workIDs[i] = 0;
			cout << "threadExit called, " << tid << " existing" << endl;
			break;
		}
	}
	pthread_exit(NULL);
}

void ThreadPoolAdd(ThreadPool *pool, void(*func)(void *), void *arg)
{
	pthread_mutex_lock(&pool->mutexpool);
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
		// 任务队列满 阻塞生产者线程
		pthread_cond_wait(&pool->notFull, &pool->mutexpool);
	}
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexpool);
		return;
	}

	// 添加到任务队列队尾
	pool->task[pool->queueRear].func = func;
	pool->task[pool->queueRear].arg = arg;

	// 环形队列移动队尾
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	// 唤醒等待线程
	pthread_cond_signal(&pool->notEmpty);

	pthread_mutex_unlock(&pool->mutexpool);
}

const int GetThreadPoolBusyNumber(ThreadPool *pool)
{
	pthread_mutex_lock(&pool->mutexbusy);
	int busyCnt = pool->busy_thread_cnt;
	pthread_mutex_unlock(&pool->mutexbusy);
	return busyCnt;
}

const int GetThreadPoolLiveNumber(ThreadPool *pool)
{
	pthread_mutex_lock(&pool->mutexpool);
	int liveCnt = pool->live_thread_cnt;
	pthread_mutex_unlock(&pool->mutexpool);
	return liveCnt;
}

void ThreadPoolDestroy(ThreadPool *pool)
{
	if (pool == NULL) {
		return ;
	}

	// 关闭线程池
	pool->shutdown = 1;

	// 阻塞回收管理者线程
	pthread_join(pool->magagerID, NULL);

	// 唤醒阻塞的消费者线程
	for (int i = 0; i < pool->live_thread_cnt; i++) {
		pthread_cond_signal(&pool->notEmpty);
	}

	if (pool && pool->task) {
		delete[]pool->task;
	}
	if (pool && pool->workIDs) {
		delete[]pool->workIDs;
	}
	pthread_mutex_destroy(&pool->mutexpool);
	pthread_mutex_destroy(&pool->mutexbusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);
	if (pool) {
		delete pool;
		pool = NULL;
	}
	return ;
}