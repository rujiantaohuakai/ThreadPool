#include <iostream>
#include <thread>
#include <utility>
#include <mutex>
#include <vector>
#include <chrono>

#include "thread_pool.h"

int main(int, char**){
    std::cout << "Hello, from ThreadPool!\n";
    std::mutex cout_mtx;
    ThreadPool mypool(4);

    // 用于保存所有任务的future
    std::vector<std::pair<std::future<int>, int>> results;

    // 批量提交任务
    for(int i = 0; i < 20; ++i){
        results.emplace_back(mypool.enqueue([&cout_mtx, i](int a, int b){
            {
                std::lock_guard<std::mutex> lock(cout_mtx);
                std::cout << "current task: " << i << " current thread: " << std::this_thread::get_id() << std::endl;
            }
            return a + b;
        }, 10*i, 10*i), i);
    }
    // 等一秒让上面执行完，都产生结果
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // 批量获取结果
    for(auto&& rsfuture : results){
        std::cout << "task: " << rsfuture.second << " thread result: " << rsfuture.first.get() << std::endl;
    }

    // void任务
    std::future<void> voidTask = mypool.enqueue([&cout_mtx](){
        std::lock_guard<std::mutex> lock(cout_mtx);
        std::cout << "void task running on thread: " 
                << std::this_thread::get_id() << std::endl;
    });

    voidTask.get();

    return 0;
}
