# ifndef _THREADPOOL_H		// 先测试_THREADPOOL_H是否被宏定义过
# define _THREADPOOL_H		// 如果没有宏定义下面就宏定义_THREADPOOL_H并编译下面的语句

typedef struct ThreadPool ThreadPool;
// 创建线程池并初始化
ThreadPool *threadPoolCreate(int min, int max, int queueSize);

// 销毁线程池
int threadPoolDestroy(ThreadPool* pool);

// 给线程池添加任务
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void *arg);

// 获取线程池中工作的线程的个数
int threadPoolBusyNum(ThreadPool* pool);

// 获取线程池中活着的线程的个数
int threadPoolLiveNum(ThreadPool* pool);

///////////////////////////////
void *worker(void* arg);
void *manager(void* arg);
void threadExit(ThreadPool* pool);
# endif		// 如果已经定义则编译#endif后面的语句