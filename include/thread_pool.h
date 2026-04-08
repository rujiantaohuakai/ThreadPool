#pragma once

#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <functional>
#include <queue>
#include <vector>
#include <stdexcept>


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
    
private:
    void worker();  // 线程的执行内容

    bool isstop;    // 表示当前线程池是否停止，true是停止
    std::condition_variable cv;     // 条件变量
    std::mutex mtx;                 // 互斥锁
    std::vector<std::thread> workers;          // 线程池（线程队列）
    std::queue<std::function<void()>> taskque;    // 任务队列

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

    // 获得functype
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
    }

    // 唤醒一个线程去执行任务
    cv.notify_one();

    // 返回future
    return resfuture;
}
