#pragma once
#include <pthread.h>
#include <queue>
using namespace std;
using callback=void(*)(void*);

struct Task
{
    Task() {
        function=nullptr;
        arg=nullptr;
    }
    Task(callback function, void* arg) {
        this->function=function;
        this->arg = arg;
    }
    callback function;
    void* arg;
};

class TaskQueue {
private:
    pthread_mutex_t m_mutex;    // 互斥锁
    std::queue<Task> m_queue;   // 任务队列
public:
    TaskQueue();
    ~TaskQueue();
    // 添加一个任务
    void addTask(Task& task);
    void addTask(callback func, void* arg);
    // 获取任务
    Task takeTask();
    // 获取任务队列中的任务数量
    inline int taskNumber() {
        return m_queue.size();
    }
};

