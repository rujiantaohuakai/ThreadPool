#include "thread_pool.h"

#include <atomic>
#include <chrono>
#include <exception>
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

} // namespace

int main() {
    try {
        test_basic_task_execution();
        test_future_return_value();
        test_enqueue_after_shutdown_throws();
        test_many_tasks_can_finish();
        std::cout << "All tests passed." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << std::endl;
        return 1;
    }
}
