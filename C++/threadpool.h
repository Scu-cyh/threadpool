#include "task.h"

class ThreadPool
{
public:
    ThreadPool(int min, int max);
    ~ThreadPool();
    void addTask(Task task);
    int getBusyNumber();
    int getAliveNumber();

private:
    int m_minNum;
    int m_maxNum;
    int m_busyNum;
    int m_aliveNum;
    int m_exitNum;
    bool m_shutdown = false;
    pthread_mutex_t m_lock;
    pthread_cond_t m_full;
    pthread_t* m_workID;
    pthread_t m_managerID;
    TaskQueue *m_taskQ;
private:
    static void* worker(void* arg);
    static void* manager(void* arg);
    void threadExit();
};

