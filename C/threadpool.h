# ifndef _THREADPOOL_H		// �Ȳ���_THREADPOOL_H�Ƿ񱻺궨���
# define _THREADPOOL_H		// ���û�к궨������ͺ궨��_THREADPOOL_H��������������

typedef struct ThreadPool ThreadPool;
// �����̳߳ز���ʼ��
ThreadPool *threadPoolCreate(int min, int max, int queueSize);

// �����̳߳�
int threadPoolDestroy(ThreadPool* pool);

// ���̳߳��������
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void *arg);

// ��ȡ�̳߳��й������̵߳ĸ���
int threadPoolBusyNum(ThreadPool* pool);

// ��ȡ�̳߳��л��ŵ��̵߳ĸ���
int threadPoolLiveNum(ThreadPool* pool);

///////////////////////////////
void *worker(void* arg);
void *manager(void* arg);
void threadExit(ThreadPool* pool);
# endif		// ����Ѿ����������#endif��������