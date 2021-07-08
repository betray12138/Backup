#pragma once
#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <iostream>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
using namespace std;

typedef struct ThreadPool ThreadPool;

// �����̳߳ز���ʼ��
ThreadPool *ThreadPoolCreate(const int min_cnt, const int max_cnt, const int queue_size);

// �����̳߳�
void ThreadPoolDestroy(ThreadPool *pool);

// ���̳߳��������
void ThreadPoolAdd(ThreadPool *pool, void(*func)(void *), void *arg);

// ��ȡ�̳߳��й����̸߳���
const int GetThreadPoolBusyNumber(ThreadPool *pool);

// ��ȡ�̳߳��л��ŵ��̸߳���
const int GetThreadPoolLiveNumber(ThreadPool *pool);

// �����̵߳�����
void *worker(void * arg);

// �����̵߳�����
void *manager(void *arg);

// �����߳�
void ThreadExit(ThreadPool *pool);

#endif // !_THREADPOOL_H
