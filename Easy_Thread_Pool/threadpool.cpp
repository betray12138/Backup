#include "threadpool.h"

const int DELTA = 2;

// ��������ṹ��
typedef struct Task
{
	void(*func)(void *arg);
	void *arg;
}Task;

// �����̳߳ؽṹ��
struct ThreadPool {
	//�������
	Task *task;				// �������
	int queueCapacity;		// ��������
	int queueSize;			// ��ǰ�������
	int queueFront;			// ��ͷ ȡ����
	int queueRear;			// ��β ������

	pthread_t magagerID;	// �������߳�ID
	pthread_t *workIDs;		// �������߳�ID

	int min_thread_cnt;		// ��С�߳�����
	int max_thread_cnt;		// ����߳�����
	int busy_thread_cnt;	// æ���߳�����
	int live_thread_cnt;	// ���ŵ��߳�����
	int exit_thread_cnt;	// Ҫ���ٵ��̸߳���
	pthread_mutex_t mutexpool;	// ���������̳߳�
	pthread_mutex_t mutexbusy;	// ��busy_thread_cnt����
	pthread_cond_t notFull;		// ��¼��������Ƿ���
	pthread_cond_t notEmpty;	// ��¼��������Ƿ��

	bool shutdown;	//�Ƿ������̳߳�  1- ���� 0 - δ����

};

ThreadPool *ThreadPoolCreate(const int min_cnt, const int max_cnt, const int queue_size)
{
	ThreadPool *pool = new ThreadPool;
	do {
		if (pool == NULL) {
			cout << "�̳߳ش���ʧ��" << endl;
			break;
		}
		pool->workIDs = new pthread_t[max_cnt];		//����ʱ�������ռ�
		if (pool->workIDs == NULL) {
			cout << "�����̴߳���ʧ��" << endl;
			break;
		}
		memset(pool->workIDs, 0, sizeof(pthread_t) * max_cnt);

		pool->min_thread_cnt = min_cnt;
		pool->max_thread_cnt = max_cnt;
		pool->busy_thread_cnt = 0;
		pool->live_thread_cnt = min_cnt;	//��ʼ����min_cnt���߳�
		pool->exit_thread_cnt = 0;

		if (pthread_mutex_init(&pool->mutexpool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexbusy, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0) {
			cout << "����������������ʧ��" << endl;
			break;
		}
		// �������
		pool->task = new Task[queue_size];
		if (pool->task == NULL) {
			cout << "������д���ʧ��" << endl;
			break;
		}
		pool->queueCapacity = queue_size;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueRear = 0;

		pool->shutdown = false;


		// �����������߳� �������̵߳�ַ  �߳�����  �������߳�ִ�еĺ���  ����
		pthread_create(&pool->magagerID, NULL, manager, pool);

		for (int i = 0; i < min_cnt; i++) {
			pthread_create(&pool->workIDs[i], NULL, worker, pool);
		}

		return pool;
	} while (0);
	
	// �ͷ���Դ
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
	// ����ת����ȡThreadPool
	ThreadPool *pool = (ThreadPool *)arg;

	while (1) {
		pthread_mutex_lock(&pool->mutexpool);
		// �жϵ�ǰ��������Ƿ�Ϊ��
		while (pool->queueSize == 0 && pool->shutdown == false) {
			pthread_cond_wait(&pool->notEmpty, &pool->mutexpool);

			// �ж��ǲ���Ҫ����
			if (pool->exit_thread_cnt > 0) {
				pool->exit_thread_cnt--;

				// ������ֵ С��min_thread_cnt ���˳�
				if (pool->live_thread_cnt > pool->min_thread_cnt) {
					// �˳�ǰ�ǵý���
					pool->live_thread_cnt--;
					pthread_mutex_unlock(&pool->mutexpool);
					ThreadExit(pool);
				}
			}
		}

		// �ж��̳߳��Ƿ񱻹ر���
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexpool);
			ThreadExit(pool);
		}

		// �����������ȡ��һ������
		Task task;
		task.func = pool->task[pool->queueFront].func;
		task.arg = pool->task[pool->queueFront].arg;

		// ȡ��������ƶ�ͷ�ڵ� ���� ѭ������
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
		pool->queueSize--;

		// ����������
		pthread_cond_signal(&pool->notFull);

		pthread_mutex_unlock(&pool->mutexpool);

		// ������ʼִ�� æ�ĸ���+1
		pthread_mutex_lock(&pool->mutexbusy);
		pool->busy_thread_cnt++;
		pthread_mutex_unlock(&pool->mutexbusy);

		task.func(task.arg);  // ���ݶ��ڴ� ���н������ͷ���Դ
		delete task.arg;
		task.arg = NULL;

		// ����ִ�н��� æ�ĸ���-1
		pthread_mutex_lock(&pool->mutexbusy);
		pool->busy_thread_cnt--;
		pthread_mutex_unlock(&pool->mutexbusy);
	}
	return NULL;
}

void *manager(void *arg)
{
	// ����ת����ȡThreadPool
	ThreadPool *pool = (ThreadPool *)arg;

	while (!pool->shutdown) {
		// ÿ��3s���һ��
		sleep(3);

		// ȡ���߳�������������͵�ǰ�̵߳������Լ�æ�߳�����
		pthread_mutex_lock(&pool->mutexpool);
		int queueSize = pool->queueSize;
		int liveCnt = pool->live_thread_cnt;
		int busyCnt = pool->busy_thread_cnt;
		pthread_mutex_unlock(&pool->mutexpool);

		// ����߳�
		// �涨����ǰ������� > ������ && ����߳��� < ����߳���
		if (queueSize > liveCnt && liveCnt < pool->max_thread_cnt) {
			pthread_mutex_lock(&pool->mutexpool);
			int counter = 0;	// ÿ������deleta���߳�
			for (int i = 0; i < pool->max_thread_cnt && counter < DELTA && pool->live_thread_cnt < pool->max_thread_cnt; i++) {
				if (pool->workIDs[i] == 0) {
					//ֵΪ0 ��ʾ�˴�δ����߳�id
					pthread_create(&pool->workIDs[i], NULL, worker, pool);
					counter++;
					pool->live_thread_cnt++;
				}
			}
			pthread_mutex_unlock(&pool->mutexpool);
		}

		// �����߳�
		// �涨��æ���߳� * 2 < �����߳��� && �����߳��� > ��С���߳���
		if (busyCnt * 2 < liveCnt && liveCnt > pool->min_thread_cnt) {
			pthread_mutex_lock(&pool->mutexpool);
			pool->exit_thread_cnt = DELTA;
			pthread_mutex_unlock(&pool->mutexpool);

			// �ù������߳��Զ�����
			for (int i = 0; i < DELTA; i++) {
				// ÿ�λ�����Ϊ���˯��ԭ��������߳�
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
		// ��������� �����������߳�
		pthread_cond_wait(&pool->notFull, &pool->mutexpool);
	}
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexpool);
		return;
	}

	// ��ӵ�������ж�β
	pool->task[pool->queueRear].func = func;
	pool->task[pool->queueRear].arg = arg;

	// ���ζ����ƶ���β
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queueSize++;

	// ���ѵȴ��߳�
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

	// �ر��̳߳�
	pool->shutdown = 1;

	// �������չ������߳�
	pthread_join(pool->magagerID, NULL);

	// �����������������߳�
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