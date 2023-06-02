#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const int NUMBER = 2;
// 任务结构体
typedef struct Task {
	void (*function) (void* arg);
	void* arg;
}Task;

// 线程池结构体
struct ThreadPool {
	// 任务队列
	Task *taskQ;
	int queueCapacity;		// 容量
	int queueSize;			// 当前任务个数
	int queueFront;			// 队头->取数据
	int queueEnd;			// 队尾->放数据

	pthread_t managerID;	// 管理者线程ID
	pthread_t *threadIDs;   // 工作的线程ID
	int minNum;				// 最小线程个数
	int maxNum;				// 最大线程个数
	int busyNum;			// 忙的线程个数
	int liveNum;			// 存活的线程的个数
	int exitNum;			// 要销毁的线程的个数
	pthread_mutex_t mutexPool;		// 锁整个的线程池
	pthread_mutex_t mutexBusy;		// 锁busyNum变量
	pthread_cond_t notFull;			// 对应生产者，任务队列满了阻塞生产者
	pthread_cond_t notEmpty;		// 对应消费者，任务队列空了阻塞消费者
	// 任务队列不为空时，生产者唤醒消费者；任务队列不满时，消费者唤醒生产者

	int shutDown;			// 是不是要销毁线程池，要销毁是1，不则是0

};

ThreadPool *threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));		// pool里面装的地址是malloc申请的空间的地址
	do {
		if (pool == NULL) {
			printf("malloc threadpool fail...\n");
			break;		// 原本是return NULL;但是因为如果这一个if没有执行，但下一个if执行了，那么就会导致没有free，
						// 为了方便使用do while循环，只循环一次，但是可以使用break替代return NULL;
		}
		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);	// threadIDs里面装的地址是malloc申请的空间的地址
		if (pool == NULL) {
			printf("malloc threadIDs fail...\n");
			break;
		}

		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);		// 把申请的空间全部置为0
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;		// 和最小个数相等
		pool->exitNum = 0;

		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)
		{
			printf("mutex or condif fail...\n");
			return 0;
		} 

		// 任务队列
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueEnd = 0;

		pool->shutDown = 0;

		// 创建线程
		pthread_create(&pool->managerID, NULL, manager, pool);		// &pool->managerID，取管理者线程的地址
		for (int i = 0; i < min; i++) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
			//int pthread_create(pthread_t *thread,  传递一个pthread_t类型的指针变量，也可以直接传递某个phthread_t类型变量的地址
			//	const pthread_attr_t * attr,
			//	void* (*start_routine) (void*), 以函数指针的方式指明新建线程需要执行的函数，该函数的参数最多有 1 个（可以省略不写），形参和返回值的类型都必须为 void* 类型。
			//	void* arg);	指定传给start_routine函数的实参，当不需要传递任何数据时，赋为0，这里worker的实参为pool
		}

		return pool;
	} while(0);

	// 如果到了这一步，说明出现了异常，break跳出来了
	// 释放资源
	if (pool && pool->threadIDs) free(pool->threadIDs);
	if (pool && pool->taskQ) free(pool->taskQ);
	if(pool) free(pool);

	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL) return -1;
	// 关闭线程池
	pool->shutDown = 1;
	// 阻塞回收管理者线程
	pthread_join(pool->managerID, NULL);
	// 唤醒阻塞的消费者线程
	for (int i = 0; i < pool->liveNum; i++) {
		pthread_cond_signal(&pool->notEmpty);
	}
	// 释放堆内存
	if (pool->taskQ) {
		free(pool->taskQ);
	}
	if (pool->threadIDs) {
		free(pool->threadIDs);
	}

	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);
	
	free(pool);
	pool = NULL;

	return 0;
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexPool);
	while (pool->queueSize == pool->queueCapacity && !pool->shutDown) {
		// 阻塞生产者线程
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutDown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	// 添加任务
	pool->taskQ[pool->queueEnd].function = func;
	pool->taskQ[pool->queueEnd].arg = arg;
	pool->queueEnd = (pool->queueEnd + 1) % pool->queueCapacity;
	pool->queueSize++;

	pthread_cond_signal(&pool->notEmpty);		// 生产者生产了产品之后，唤醒消费者

	pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}

int threadPoolLiveNum(ThreadPool* pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return liveNum;
}

void *worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (1) {		// 不停的读任务队列
		pthread_mutex_lock(&pool->mutexPool);
		// 当前任务队列是否为空
		while (pool->queueSize == 0 && !pool->shutDown) {
			// 阻塞工作线程
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);		// notEMpty能记录哪些线程被阻塞了，所有线程都去抢一把锁

			// 判断是不是要销毁线程
			if (pool->exitNum > 0) {
				pool->exitNum--;
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}

		// 判断线程池是否关闭
		if (pool->shutDown) {
			pthread_mutex_unlock(&pool->mutexPool);		// 为了避免死锁
			threadExit(pool);;		// 当前线程退出
		}

		// 开始消费，从任务队列中取出一个任务
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;
		// 移动头节点，循环队列
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;		// 这个操作可以使得，到达容量极限时重新变为0，没到末尾继续加1
		pool->queueSize--;
		// 解锁
		pthread_cond_signal(&pool -> notFull);		// 消费者消耗了产品之后，唤醒生产者
		pthread_mutex_unlock(&pool->mutexPool);
		
		printf("thread %ld start working...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		task.function(task.arg);		// task.arg最好还是堆内存，可以自己主动释放
		free(task.arg);
		task.arg = NULL;

		printf("thread %ld end working...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	return NULL;
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutDown) {
		// 每隔三秒检测一次
		sleep(3);

		// 取出线程池中任务的数量和当前线程的数量
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// 取出忙的线程的数量
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);
		
		// 添加线程
		// 任务的个数>存活的线程个数 && 存活的线程数<最大线程数
		if (queueSize > liveNum && liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; i++) {	// 从0开始遍历线程ID，如果是0就可以用
				if (pool->threadIDs[i] == 0) {
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
				
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		// 销毁线程
		// 忙的线程*2 < 存活的线程数 && 存活的线程>最小线程数
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// 让工作的线程自杀
			for (int i = 0; i < NUMBER; i++) {
				pthread_cond_signal(&pool->notEmpty);		// 唤醒线程
			}
		}
	}
	return NULL;
}

void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; i++) {
		if (pool->threadIDs[i] == tid) {
			pool->threadIDs[i] = 0;
			printf("threadExit() called, %ld exiting...\n", tid);
			break;
		}
	}
	pthread_exit(NULL);
}
