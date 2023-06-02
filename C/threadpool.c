#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const int NUMBER = 2;
// ����ṹ��
typedef struct Task {
	void (*function) (void* arg);
	void* arg;
}Task;

// �̳߳ؽṹ��
struct ThreadPool {
	// �������
	Task *taskQ;
	int queueCapacity;		// ����
	int queueSize;			// ��ǰ�������
	int queueFront;			// ��ͷ->ȡ����
	int queueEnd;			// ��β->������

	pthread_t managerID;	// �������߳�ID
	pthread_t *threadIDs;   // �������߳�ID
	int minNum;				// ��С�̸߳���
	int maxNum;				// ����̸߳���
	int busyNum;			// æ���̸߳���
	int liveNum;			// �����̵߳ĸ���
	int exitNum;			// Ҫ���ٵ��̵߳ĸ���
	pthread_mutex_t mutexPool;		// ���������̳߳�
	pthread_mutex_t mutexBusy;		// ��busyNum����
	pthread_cond_t notFull;			// ��Ӧ�����ߣ����������������������
	pthread_cond_t notEmpty;		// ��Ӧ�����ߣ�������п�������������
	// ������в�Ϊ��ʱ�������߻��������ߣ�������в���ʱ�������߻���������

	int shutDown;			// �ǲ���Ҫ�����̳߳أ�Ҫ������1��������0

};

ThreadPool *threadPoolCreate(int min, int max, int queueSize)
{
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));		// pool����װ�ĵ�ַ��malloc����Ŀռ�ĵ�ַ
	do {
		if (pool == NULL) {
			printf("malloc threadpool fail...\n");
			break;		// ԭ����return NULL;������Ϊ�����һ��ifû��ִ�У�����һ��ifִ���ˣ���ô�ͻᵼ��û��free��
						// Ϊ�˷���ʹ��do whileѭ����ֻѭ��һ�Σ����ǿ���ʹ��break���return NULL;
		}
		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);	// threadIDs����װ�ĵ�ַ��malloc����Ŀռ�ĵ�ַ
		if (pool == NULL) {
			printf("malloc threadIDs fail...\n");
			break;
		}

		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);		// ������Ŀռ�ȫ����Ϊ0
		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;		// ����С�������
		pool->exitNum = 0;

		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
			pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
			pthread_cond_init(&pool->notFull, NULL) != 0)
		{
			printf("mutex or condif fail...\n");
			return 0;
		} 

		// �������
		pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
		pool->queueCapacity = queueSize;
		pool->queueSize = 0;
		pool->queueFront = 0;
		pool->queueEnd = 0;

		pool->shutDown = 0;

		// �����߳�
		pthread_create(&pool->managerID, NULL, manager, pool);		// &pool->managerID��ȡ�������̵߳ĵ�ַ
		for (int i = 0; i < min; i++) {
			pthread_create(&pool->threadIDs[i], NULL, worker, pool);
			//int pthread_create(pthread_t *thread,  ����һ��pthread_t���͵�ָ�������Ҳ����ֱ�Ӵ���ĳ��phthread_t���ͱ����ĵ�ַ
			//	const pthread_attr_t * attr,
			//	void* (*start_routine) (void*), �Ժ���ָ��ķ�ʽָ���½��߳���Ҫִ�еĺ������ú����Ĳ�������� 1 ��������ʡ�Բ�д�����βκͷ���ֵ�����Ͷ�����Ϊ void* ���͡�
			//	void* arg);	ָ������start_routine������ʵ�Σ�������Ҫ�����κ�����ʱ����Ϊ0������worker��ʵ��Ϊpool
		}

		return pool;
	} while(0);

	// ���������һ����˵���������쳣��break��������
	// �ͷ���Դ
	if (pool && pool->threadIDs) free(pool->threadIDs);
	if (pool && pool->taskQ) free(pool->taskQ);
	if(pool) free(pool);

	return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
	if (pool == NULL) return -1;
	// �ر��̳߳�
	pool->shutDown = 1;
	// �������չ������߳�
	pthread_join(pool->managerID, NULL);
	// �����������������߳�
	for (int i = 0; i < pool->liveNum; i++) {
		pthread_cond_signal(&pool->notEmpty);
	}
	// �ͷŶ��ڴ�
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
		// �����������߳�
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutDown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}
	// �������
	pool->taskQ[pool->queueEnd].function = func;
	pool->taskQ[pool->queueEnd].arg = arg;
	pool->queueEnd = (pool->queueEnd + 1) % pool->queueCapacity;
	pool->queueSize++;

	pthread_cond_signal(&pool->notEmpty);		// �����������˲�Ʒ֮�󣬻���������

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
	while (1) {		// ��ͣ�Ķ��������
		pthread_mutex_lock(&pool->mutexPool);
		// ��ǰ��������Ƿ�Ϊ��
		while (pool->queueSize == 0 && !pool->shutDown) {
			// ���������߳�
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);		// notEMpty�ܼ�¼��Щ�̱߳������ˣ������̶߳�ȥ��һ����

			// �ж��ǲ���Ҫ�����߳�
			if (pool->exitNum > 0) {
				pool->exitNum--;
				if (pool->liveNum > pool->minNum) {
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutexPool);
					threadExit(pool);
				}
			}
		}

		// �ж��̳߳��Ƿ�ر�
		if (pool->shutDown) {
			pthread_mutex_unlock(&pool->mutexPool);		// Ϊ�˱�������
			threadExit(pool);;		// ��ǰ�߳��˳�
		}

		// ��ʼ���ѣ������������ȡ��һ������
		Task task;
		task.function = pool->taskQ[pool->queueFront].function;
		task.arg = pool->taskQ[pool->queueFront].arg;
		// �ƶ�ͷ�ڵ㣬ѭ������
		pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;		// �����������ʹ�ã�������������ʱ���±�Ϊ0��û��ĩβ������1
		pool->queueSize--;
		// ����
		pthread_cond_signal(&pool -> notFull);		// �����������˲�Ʒ֮�󣬻���������
		pthread_mutex_unlock(&pool->mutexPool);
		
		printf("thread %ld start working...\n", pthread_self());
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);
		task.function(task.arg);		// task.arg��û��Ƕ��ڴ棬�����Լ������ͷ�
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
		// ÿ��������һ��
		sleep(3);

		// ȡ���̳߳�������������͵�ǰ�̵߳�����
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// ȡ��æ���̵߳�����
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);
		
		// ����߳�
		// ����ĸ���>�����̸߳��� && �����߳���<����߳���
		if (queueSize > liveNum && liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; i++) {	// ��0��ʼ�����߳�ID�������0�Ϳ�����
				if (pool->threadIDs[i] == 0) {
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
				
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}

		// �����߳�
		// æ���߳�*2 < �����߳��� && �����߳�>��С�߳���
		if (busyNum * 2 < liveNum && liveNum > pool->minNum) {
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// �ù������߳���ɱ
			for (int i = 0; i < NUMBER; i++) {
				pthread_cond_signal(&pool->notEmpty);		// �����߳�
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
