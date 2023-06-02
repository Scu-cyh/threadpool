#include <iostream>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
using namespace std;
#include "threadpool.h"

ThreadPool::ThreadPool(int minNum, int maxNum) {
    m_taskQ = new TaskQueue;
    do{
        m_minNum = minNum;
        m_maxNum = maxNum;
        m_busyNum = 0;
        m_aliveNum = minNum;
        m_exitNum = 0;
        // 根据线程的最大值来分配线程号的内存
        m_workID = new pthread_t[maxNum];
        if(m_workID==nullptr) {
            cout<<"malloc thread_t[] fail..."<<endl;
            break;
        }
        // 初始化线程号数组
        memset(m_workID, 0, sizeof(pthread_t)*maxNum);
        // 初始化互斥锁，条件变量
        if(pthread_mutex_init(&m_lock, NULL)!=0||
           pthread_cond_init(&m_full, NULL)!=0) {
            cout<<"init mutex or condition fail..."<<endl;
            break;
        }
        // 创建线程
        /*根据最小线程数创建工作线程，调用pthread_create函数时，创建一个新线程，
         * m_workID数组返回新创建的线程号，然后执行回调函数worker，参数是这个初始化ThreadPool对象本身，
         * 用this指针传递，相当于执行函数void* worker(void* arg), arg=this*/
        for(int i=0; i<minNum; i++) {
            pthread_create(&m_workID[i], NULL, worker, this);
            cout<<"create work thread ID:"<<to_string(m_workID[i])<<endl;
            sleep(1);
        }
        // 创建一个管理线程
        pthread_create(&m_managerID, NULL, manager, this);
        cout<<"create manager thread ID:"<<to_string(m_managerID)<<endl;
    }while(0);
};

ThreadPool::~ThreadPool() {
    // 销毁管理者线程
    pthread_join(m_managerID, NULL);
    // 唤醒所有存活的消费者线程
    for(int i=0; i<m_aliveNum; i++) {
        pthread_cond_signal(&m_full);
    }
    if(m_taskQ) delete m_taskQ;
    if(m_workID) delete[] m_workID;
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_full);
};

void ThreadPool::addTask(Task task) {
    if(m_shutdown==1) {
        return ;
    }
    // 添加任务，不需要加锁，任务队列中有锁
    m_taskQ->addTask(task);
    // 唤醒工作的线程
    pthread_cond_signal(&m_full);
    return ;
};
int ThreadPool::getBusyNumber() {
    if(m_shutdown==1) {
        return 0;
    }
    int busynumber;
    pthread_mutex_lock(&m_lock);
    busynumber = m_busyNum;
    pthread_mutex_unlock(&m_lock);
    return busynumber;
}
int ThreadPool::getAliveNumber() {
    if(m_shutdown==1) {
        return 0;
    }
    int Alivenumber;
    pthread_mutex_lock(&m_lock);
    Alivenumber = m_aliveNum;
    pthread_mutex_unlock(&m_lock);
    return Alivenumber;
}

/*调用pthread_create函数后，多个线程调转执行work函数代码，
 * 多个线程竞争共享资源(m_taskQ)，刚开始任务对列为空，
 * 调用pthread_cond_wait对线程进行阻塞，
 * 等到生产者线程调用pthread_cond_signal唤醒阻塞线程上的某个线程，
 * 工作线程就可以从任务队列中取一个线程
 * */

void* ThreadPool::worker(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    // 一直不停的工作
    while(1) {
        pthread_mutex_lock(&pool->m_lock);
        // 判断任务队列是否为空，如果为空工作线程阻塞
        while(pool->m_taskQ->taskNumber()==0&&!pool->m_shutdown) {
            cout<<"thread: "<<to_string(pthread_self())<<"    waiting..."<<endl;
            // 阻塞线程
            pthread_cond_wait(&pool->m_full, &pool->m_lock);
            // 解除阻塞之后，判断是否要销毁线程
            if(pool->m_exitNum>0) {
                pool->m_exitNum--;
                if(pool->m_aliveNum > pool->m_minNum) {
                    pool->m_aliveNum--;
                    pthread_mutex_unlock(&pool->m_lock);
                    pool->threadExit();
                }
            }
        }
        if(pool->m_shutdown) {
            pthread_mutex_unlock(&pool->m_lock);
            pool->threadExit();
        }
        // 从任务队列中取出一个任务
        Task task=pool->m_taskQ->takeTask();
        pool->m_busyNum++;
        pthread_mutex_unlock(&pool->m_lock);
        cout<<"thread: "<<to_string(pthread_self())<<"    start working..."<<endl;
        task.function(task.arg);
        delete task.arg;
        task.arg = nullptr;

        // 任务处理结束
        cout<<"thread: "<<to_string(pthread_self())<<"    end working..."<<endl;
        pthread_mutex_lock(&pool->m_lock);
        pool->m_busyNum--;
        pthread_mutex_unlock(&pool->m_lock);
    }
    return nullptr;
}

// 管理者线程任务函数
void* ThreadPool::manager(void* arg) {
    cout<<"hi i am turn in manager thread......................."<<endl;
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    // 如果线程没有关闭，就一直检测
    while(!pool->m_shutdown) {
        // 每隔5s检查一次
        sleep(5);
        // 取出线程池中任务数和线程数量
        // 取出工作的线程池数量
        pthread_mutex_lock(&pool->m_lock);
        int queueSize = pool->m_taskQ->taskNumber();
        int aliveNum = pool->m_aliveNum;
        int busyNum = pool->m_busyNum;
        pthread_mutex_unlock(&pool->m_lock);
        // 创建线程
        const int NUMBER=2;
        // 当前任务个数 > 存活的线程数 && 存活的线程数<最大线程个数
        if(queueSize>aliveNum&&aliveNum<pool->m_maxNum) {
            pthread_mutex_lock(&pool->m_lock);
            // 循环便利线程ID数组，找出一个空位（即线程ID=0），创建的新线程号用这个空位传出，
            // 同时保证每次只创建NUMBER个线程
            for(int i=0, num=0; i<pool->m_maxNum&&num<NUMBER&&pool->m_aliveNum<pool->m_maxNum; i++) {
                if(pool->m_workID[i]==0) {
                    pool->m_aliveNum++;
                    num++;
                    pthread_create(&pool->m_workID[i], NULL, worker, pool);
                }
            }
            pthread_mutex_unlock(&pool->m_lock);
        }
        // 销毁多余的线程
        // 忙线程*2 < 存活的线程数目 && 存活的线程数 > 最小线程数量
        if(busyNum*2 < aliveNum && aliveNum > pool->m_aliveNum) {
            pthread_mutex_lock(&pool->m_lock);
            pool->m_exitNum = NUMBER;
            pthread_mutex_unlock(&pool->m_lock);
            for(int i=0; i<NUMBER; i++) {
                pthread_cond_signal(&pool->m_full);
            }
        }
    }
    return nullptr;
}
//线程退出
void ThreadPool::threadExit() {
    pthread_t tid = pthread_self();
    for(int i=0; i<m_maxNum; i++) {
        if(m_workID[i]==tid) {
            cout<<"threadExit function thread"<<to_string(pthread_self())<<"    exit..."<<endl;
            m_workID[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}
