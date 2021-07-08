#pragma once
#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <iostream>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
using namespace std;

typedef struct ThreadPool ThreadPool;

// 创建线程池并初始化
ThreadPool *ThreadPoolCreate(const int min_cnt, const int max_cnt, const int queue_size);

// 销毁线程池
void ThreadPoolDestroy(ThreadPool *pool);

// 给线程池添加任务
void ThreadPoolAdd(ThreadPool *pool, void(*func)(void *), void *arg);

// 获取线程池中工作线程个数
const int GetThreadPoolBusyNumber(ThreadPool *pool);

// 获取线程池中活着的线程个数
const int GetThreadPoolLiveNumber(ThreadPool *pool);

// 工作线程的任务
void *worker(void * arg);

// 管理线程的任务
void *manager(void *arg);

// 销毁线程
void ThreadExit(ThreadPool *pool);

#endif // !_THREADPOOL_H
