#include "../include/thread_pool.h"

#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// 打印线程池当前状态，用于展示 v2 新增的任务统计接口。
void printPoolStatus(const ThreadPool& pool, const std::string& title) {
    std::cout << "\n[" << title << "]\n";
    std::cout << "idle threads: " << pool.getIdleThreadCount() << '\n';
    std::cout << "pending tasks: " << pool.getPendingTasksCount() << '\n';
    std::cout << "active tasks: " << pool.getActiveTasksCount() << '\n';
    std::cout << "total tasks: " << pool.getTasksCount() << '\n';
    std::cout << "stopped: " << std::boolalpha << pool.stopped() << '\n';
}

int main() {
    try {
        std::cout << "ThreadPool demo started.\n";

        std::mutex coutMtx;

        ThreadPool mypool(4);
        printPoolStatus(mypool, "initial status");

        // 开启日志
        mypool.setLogDirectory("logs/demo");
        mypool.setLogEnabled(true);
        std::cout << "\nlog enabled: " << std::boolalpha << mypool.isLogEnabled() << '\n';
        std::cout << "log directory: " << mypool.getLogDirectory().string() << '\n';

        // 返回int的任务
        std::future<int> sumTask = mypool.enqueue([](int a, int b) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return a + b;
        }, 20, 22);

        // 返回string的任务
        std::future<std::string> stringTask = mypool.enqueue([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            return std::string("hello from thread pool");
        });

        // void任务
        std::future<void> voidTask = mypool.enqueue([&coutMtx]() {
            std::lock_guard<std::mutex> lock(coutMtx);
            std::cout << "void task running on thread: "
                      << std::this_thread::get_id() << '\n';
        });

        printPoolStatus(mypool, "after submitting base tasks");

        // 批量提交任务
        std::vector<std::future<int>> batchResults;
        batchResults.reserve(12);

        for (int i = 0; i < 12; ++i) {
            batchResults.emplace_back(mypool.enqueue([&coutMtx, i]() {
                {
                    std::lock_guard<std::mutex> lock(coutMtx);
                    std::cout << "batch task " << i
                              << " running on thread: "
                              << std::this_thread::get_id() << '\n';
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                return i * i;
            }));
        }

        printPoolStatus(mypool, "after submitting batch tasks");

        // waitAll
        std::cout << "\nwaiting for all submitted tasks...\n";
        mypool.waitAll();
        printPoolStatus(mypool, "after waitAll");

        // 获取任务结果
        std::cout << "\nresult of sum task: " << sumTask.get() << '\n';
        std::cout << "result of string task: " << stringTask.get() << '\n';
        voidTask.get();

        int batchSum = 0;
        for (auto& future : batchResults) {
            batchSum += future.get();
        }
        std::cout << "sum of batch task results: " << batchSum << '\n';

        // 证明 waitAll 后线程池仍然可用。
        std::future<std::string> afterWaitTask = mypool.enqueue([]() {
            return std::string("pool is still usable after waitAll");
        });
        std::cout << "\n" << afterWaitTask.get() << '\n';
        printPoolStatus(mypool, "after submitting task again");

        // shutdown 会停止接收新任务，并等待 worker 线程安全退出。
        mypool.shutdown();
        printPoolStatus(mypool, "after shutdown");

        // shutdown 后拒绝提交任务
        try {
            std::future<int> invalidTask = mypool.enqueue([]() {
                return 1;
            });
            (void)invalidTask;
        } catch (const std::runtime_error& ex) {
            std::cout << "\nenqueue after shutdown failed as expected: "
                      << ex.what() << '\n';
        }

        std::cout << "\nThreadPool demo finished.\n";
        std::cout << "log file path: logs/demo/thread_pool.log\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "demo failed: " << ex.what() << '\n';
        return 1;
    }
}
