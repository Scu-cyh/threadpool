#include <iostream>
#include "task.h"
#include "threadpool.h"
#include <unistd.h>
using namespace std;
void taskFunc(void* arg) {  // 任务函数
    int num = *(int*)arg;
    cout<<"thread ID: "<<pthread_self()<<"    number="<<num<<endl;
    sleep(1);
}
int main()
{
    ThreadPool* pool = new ThreadPool(10, 20);
    for(int i=0; i<100; i++) {// 循环的向任务队列里加入1--个任务，等待消费者线程来消费
        int* num = (int*)malloc(sizeof(int));
        *num = i+100;
        Task task;
        task.function = taskFunc;
        task.arg = num;
        pool->addTask(task);        // 向任务队列里加入任务，模拟生产者，addTask函数调用thread_cond_signal函数唤醒一个工作线程
        cout<<"忙线程数: "<<pool->getBusyNumber()<<"    ";
        cout<<"活线程数: "<<pool->getAliveNumber()<<endl;
        // sleep(1);
    }
    sleep(3000);
    return 0;
}

