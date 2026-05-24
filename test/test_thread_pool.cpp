#include "thread_pool.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace { // 当前文件私有，外部不可见，防止重名冲突

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// 测试1：基础任务提交后能否被全部执行
void test_basic_task_execution() {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 20; ++i) {
        futures.emplace_back(pool.enqueue([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    for (auto& future : futures) {
        future.get();
    }

    expect(counter.load(std::memory_order_relaxed) == 20,
           "basic task execution failed");
}

// 测试2：future 返回值是否正确
void test_future_return_value() {
    ThreadPool pool(4);
    auto future = pool.enqueue([](int a, int b) {
        return a + b;
    }, 20, 22);

    expect(future.get() == 42, "future return value is incorrect");
}

// 测试3：shutdown 后继续提交任务是否会抛出异常
void test_enqueue_after_shutdown_throws() {
    ThreadPool pool(2);
    pool.shutdown();

    bool threw = false;
    try {
        auto future = pool.enqueue([]() { return 1; });
        (void)future;
    } catch (const std::runtime_error&) {
        threw = true;
    }

    expect(threw, "enqueue after shutdown should throw");
}

// 测试4：大量任务提交后线程池能否安全退出
void test_many_tasks_can_finish() {
    ThreadPool pool(8);
    std::atomic<int> sum{0};
    std::vector<std::future<void>> futures;
    futures.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        futures.emplace_back(pool.enqueue([&sum, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            sum.fetch_add(i, std::memory_order_relaxed);
        }));
    }

    for (auto& future : futures) {
        future.get();
    }

    expect(sum.load(std::memory_order_relaxed) == 999 * 1000 / 2,
           "many tasks execution failed");
}

// 测试5：waitAll 能等待当前任务全部完成
void test_wait_all_waits_for_submitted_tasks() {
    ThreadPool pool(4);
    std::atomic<int> finished{0};

    for (int i = 0; i < 16; ++i) {
        pool.enqueue([&finished]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            finished.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.waitAll();

    expect(finished.load(std::memory_order_relaxed) == 16,
           "waitAll should wait until all submitted tasks finish");
    expect(pool.getPendingTasksCount() == 0,
           "pending task count should be 0 after waitAll");
    expect(pool.getActiveTasksCount() == 0,
           "active task count should be 0 after waitAll");
}

// 测试6：waitAll 返回后线程池仍可继续提交任务
void test_wait_all_keeps_pool_usable() {
    ThreadPool pool(2);

    for (int i = 0; i < 8; ++i) {
        pool.enqueue([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });
    }

    pool.waitAll();

    auto future = pool.enqueue([]() {
        return 42;
    });

    expect(future.get() == 42,
           "pool should still accept new tasks after waitAll");
}

// 测试7：日志目录可配置，且默认值存在
void test_log_directory_can_be_configured() {
    ThreadPool pool(2);
    expect(pool.getLogDirectory() == std::filesystem::path("logs"),
           "default log directory should be logs");

    const std::filesystem::path customDirectory = "test_logs/custom";
    pool.setLogDirectory(customDirectory);
    expect(pool.getLogDirectory() == customDirectory,
           "custom log directory should be applied");
}

// 测试8：日志写入失败时不应影响线程池核心功能
void test_logging_failure_does_not_break_pool() {
    const std::filesystem::path tempRoot = "test_logs";
    const std::filesystem::path blockedPath = tempRoot / "blocked_path";

    std::filesystem::create_directories(tempRoot);
    {
        std::ofstream blockedFile(blockedPath);
        blockedFile << "this path is intentionally a file";
    }

    ThreadPool pool(2);
    pool.setLogDirectory(blockedPath);
    pool.setLogEnabled(true);

    auto future = pool.enqueue([]() {
        return 7;
    });

    expect(future.get() == 7,
           "logging failure should not break task execution");

    pool.waitAll();

    std::error_code ec;
    std::filesystem::remove(blockedPath, ec);
    std::filesystem::remove(tempRoot, ec);
}

// 测试9：任务内部抛出的异常应该通过 future 传递给调用者
void test_task_exception_propagates_through_future() {
    ThreadPool pool(2);

    auto failingFuture = pool.enqueue([]() -> int {
        throw std::runtime_error("task failed");
    });

    bool threw = false;
    try {
        (void)failingFuture.get();
    } catch (const std::runtime_error& ex) {
        threw = std::string(ex.what()) == "task failed";
    }

    expect(threw, "task exception should propagate through future");

    auto nextFuture = pool.enqueue([]() {
        return 99;
    });

    expect(nextFuture.get() == 99,
           "pool should remain usable after a task throws");
}

} // namespace

int main() {
    try {
        test_basic_task_execution();
        test_future_return_value();
        test_enqueue_after_shutdown_throws();
        test_many_tasks_can_finish();
        test_wait_all_waits_for_submitted_tasks();
        test_wait_all_keeps_pool_usable();
        test_log_directory_can_be_configured();
        test_logging_failure_does_not_break_pool();
        test_task_exception_propagates_through_future();
        std::cout << "All tests passed." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << std::endl;
        return 1;
    }
}
