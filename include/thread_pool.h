#pragma once

#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <functional>
#include <memory>
#include <queue>
#include <type_traits>
#include <vector>
#include <string>
#include <stdexcept>
#include <filesystem>

class ThreadPool{
public:
    explicit ThreadPool(std::size_t threadNums);
    ~ThreadPool();

    /*
    禁用四种构造：
    拷贝构造：用一个已有对象创建新对象
    拷贝赋值：用 = 把一个对象赋值给另一个对象
    移动构造：转移一个临时对象的资源到新对象
    移动赋值：转移一个临时对象的资源到已有对象
    */
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    // 添加任务到队列
    template <typename F, typename ...Arg>
    auto enqueue(F&& f, Arg&&... arg) -> std::future<typename std::invoke_result<F, Arg...>::type>;
    
    void shutdown(); // 关闭线程池

    std::size_t getIdleThreadCount() const; // 获取空闲线程数量
    std::size_t getPendingTasksCount() const; // 获取等待执行的任务数量
    std::size_t getActiveTasksCount() const; // 获取正在执行的任务数量
    std::size_t getTasksCount() const; // 获取任务数量
    bool stopped() const; // 判断线程池是否停止
    void waitAll(); // 等待队列清空且没有工作线程仍在执行任务。函数返回后，线程池仍可使用。
    void setLogEnabled(bool enabled);
    bool isLogEnabled() const;
    void setLogDirectory(const std::filesystem::path& directory);
    std::filesystem::path getLogDirectory() const;
    
    
private:
    void worker();  // 线程的执行内容
    void log(const std::string&); // 日志
    static std::filesystem::path defaultLogDirectory();
    bool isstop;    // 表示当前线程池是否停止，true是停止
    bool logEnabled; // 是否开启日志功能
    std::filesystem::path logDirectory; // 日志目录
    std::condition_variable cv;     // 获取任务用
    std::condition_variable doneCv; // 任务执行完成用
    mutable std::mutex mtx;                 // 互斥锁
    mutable std::mutex logMtx;              // 打印日志使用
    std::vector<std::thread> workers;          // 线程池（线程队列）
    std::queue<std::function<void()>> taskque;    // 任务队列

    std::size_t idleThreadCount; // 空闲线程数量
    std::size_t pendingTasks; // 等待执行的任务数量
    std::size_t activeTasks; // 正在执行的任务数量
    
};

template<typename F, typename ...Arg>
auto ThreadPool::enqueue(F&& f, Arg&&... arg) -> std::future<typename std::invoke_result<F, Arg...>::type>{
    // 获取f执行后的类型
    using functype = typename std::invoke_result<F, Arg...>::type;

    // 类型擦除和任务封装
    auto task = std::make_shared<std::packaged_task<functype()>>(
        // 将函数和参数绑定在一起，产生的新对象调用时就不再需要任何参数了
        std::bind(std::forward<F>(f), std::forward<Arg>(arg)...)
    );

    // 获得future
    std::future<functype> resfuture = task->get_future();

    // 将任务添加到队列
    {
        std::lock_guard<std::mutex> lockguard(this->mtx);
        if(this->isstop){
            throw std::runtime_error("error: thread pool is stopped");
        }
        // 将任务添加到队列
        // lambda表达式将(*task)()封装成一个返回值可忽略的可调对象，之前创建的task是无参数的，
        // 所以整体lambda就是一个无参数，无返回值的可调对象，可以放入std::function<void()>类型的队列中
        this->taskque.emplace( [task](){(*task)();} );
        ++this->pendingTasks;
    }

    this->log("task enqueue, total tasks = " + std::to_string(this->getTasksCount()) + ".");

    // 唤醒一个线程去执行任务
    cv.notify_one();

    // 返回future
    return resfuture;
}
