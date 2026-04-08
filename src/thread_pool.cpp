#include "thread_pool.h"
#include <thread>
#include <mutex>
#include <future>
#include <queue>
#include <functional>
#include <condition_variable>
#include <type_traits>
#include <stdexcept>

ThreadPool::ThreadPool(std::size_t threadNums): isstop(false){
    if (threadNums == 0) {
        throw std::invalid_argument("threadNums must be greater than 0");
    }
    for(std::size_t i = 0; i < threadNums; ++i){
        workers.emplace_back([this](){
            this->worker();
        });
    }
}

ThreadPool::~ThreadPool(){
    
    shutdown();
}

void ThreadPool::shutdown() {
    // 更改停止标志
    {
        std::unique_lock<std::mutex> lock(this->mtx);
        if (this->isstop) {
            return;
        }
        isstop = true;
    }

    // 通知所有阻塞中的线程
    this->cv.notify_all();

    // 确保线程执行完成
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::worker(){
    while(true){
        // 定义任务
        std::function<void()> task;

        // 从队列中获取一个任务
        {
            std::unique_lock<std::mutex> lock(this->mtx);
            // 获取锁，同时线程池停止或者任务队列不为空时继续
            cv.wait(lock, [this](){ return this->isstop || !this->taskque.empty(); });
            if(this->isstop && this->taskque.empty()) return; // 线程池停止且任务队列为空，退出线程
            task = std::move(this->taskque.front());
            this->taskque.pop();
        }

        // 执行任务
        task();
    }
}

